/*
iOS first-run game data download, the counterpart of the Android launcher
(GameDataInstaller.java). Fetches the essential nQuake distfiles listed in
gamedata.txt (generated at build time from GameDataManifest.java) into the
app's Documents folder, showing a plain progress screen.

Each file goes to "<path>.part" and is renamed only once complete, so an
interrupted download never looks finished and a retry skips what is done.
*/

#include <SDL.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define GAMEDATA_URL "https://raw.githubusercontent.com/nQuake/distfiles/master/"
#define GAMEDATA_MAX 1024

typedef struct { char remote[256]; char local[256]; long long size; } gamedata_entry_t;

static gamedata_entry_t entries[GAMEDATA_MAX];
static int entry_count;
static char basedir[1024];
static long long total_size;
static SDL_AtomicInt done_files, done_bytes_kb, finished, failed;
static char failed_name[256], failed_reason[256];
static long long file_bytes;

static int FileExists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && st.st_size > 0;
}

static void MakeParentDirs(const char *path)
{
	char dir[1024];
	char *slash;

	SDL_strlcpy(dir, path, sizeof(dir));
	slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		SDL_CreateDirectory(dir);
	}
}

static int LoadManifest(void)
{
	char path[1024], line[600];
	FILE *f;

	SDL_snprintf(path, sizeof(path), "%sgamedata.txt", SDL_GetBasePath());
	f = fopen(path, "r");
	if (!f) {
		return 0;
	}
	while (entry_count < GAMEDATA_MAX && fgets(line, sizeof(line), f)) {
		gamedata_entry_t *e = &entries[entry_count];

		if (sscanf(line, "%255s %255s %lld", e->remote, e->local, &e->size) == 3) {
			total_size += e->size;
			entry_count++;
		}
	}
	fclose(f);
	return entry_count > 0;
}

static size_t WriteChunk(void *data, size_t size, size_t nmemb, void *stream)
{
	size_t n = fwrite(data, size, nmemb, (FILE *)stream);

	file_bytes += (long long)(n * size);
	SDL_SetAtomicInt(&done_bytes_kb, SDL_GetAtomicInt(&done_bytes_kb) + (int)(n * size / 1024));
	return n;
}

static int DownloadThread(void *unused)
{
	CURL *curl = curl_easy_init();
	char cainfo[1024];
	int i;

	SDL_snprintf(cainfo, sizeof(cainfo), "%scacert.pem", SDL_GetBasePath());

	for (i = 0; curl && i < entry_count; i++) {
		char url[512], dst[1024], part[1100];
		FILE *out;
		long status = 0;
		CURLcode res;

		SDL_snprintf(dst, sizeof(dst), "%s/%s", basedir, entries[i].local);
		if (FileExists(dst)) {
			SDL_SetAtomicInt(&done_bytes_kb, SDL_GetAtomicInt(&done_bytes_kb) + (int)(entries[i].size / 1024));
			SDL_SetAtomicInt(&done_files, i + 1);
			continue;
		}
		MakeParentDirs(dst);
		SDL_snprintf(part, sizeof(part), "%s.part", dst);
		SDL_snprintf(url, sizeof(url), GAMEDATA_URL "%s", entries[i].remote);
		out = fopen(part, "wb");
		if (!out) {
			SDL_snprintf(failed_reason, sizeof(failed_reason), "cannot write %s", part);
			break;
		}
		file_bytes = 0;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteChunk);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
		curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo);   // mbedtls has no access to the iOS trust store
		res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
		fclose(out);
		if (res != CURLE_OK || status != 200 || rename(part, dst) != 0) {
			SDL_snprintf(failed_reason, sizeof(failed_reason), "%s (HTTP %ld)", curl_easy_strerror(res), status);
			remove(part);
			break;
		}
		SDL_SetAtomicInt(&done_files, i + 1);
	}
	if (SDL_GetAtomicInt(&done_files) < entry_count) {
		SDL_strlcpy(failed_name, entry_count ? entries[SDL_GetAtomicInt(&done_files)].local : "gamedata.txt", sizeof(failed_name));
		SDL_Log("gamedata: %s: %s", failed_name, failed_reason);
		SDL_SetAtomicInt(&failed, 1);
	}
	if (curl) {
		curl_easy_cleanup(curl);
	}
	SDL_SetAtomicInt(&finished, 1);
	return 0;
}

static void DrawText(SDL_Renderer *r, float x, float y, float scale, const char *text)
{
	SDL_SetRenderScale(r, scale, scale);
	SDL_RenderDebugText(r, x / scale, y / scale, text);
	SDL_SetRenderScale(r, 1, 1);
}

// Blocks until the essential data is in <documents>, downloading it when id1/pak0.pak is missing.
void IOS_EnsureGameData(void)
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	char pak[1100];
	int retry = 1;

	SDL_strlcpy(basedir, SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS), sizeof(basedir));
	if (basedir[0] && basedir[strlen(basedir) - 1] == '/') {
		basedir[strlen(basedir) - 1] = 0;
	}
	SDL_snprintf(pak, sizeof(pak), "%s/id1/pak0.pak", basedir);
	if (FileExists(pak) || !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		return;
	}
	if (!SDL_CreateWindowAndRenderer("nextQuake", 0, 0, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
		return;
	}
	LoadManifest();

	while (retry) {
		SDL_Thread *thread;

		retry = 0;
		SDL_SetAtomicInt(&done_files, 0);
		SDL_SetAtomicInt(&done_bytes_kb, 0);
		SDL_SetAtomicInt(&finished, 0);
		SDL_SetAtomicInt(&failed, 0);
		thread = SDL_CreateThread(DownloadThread, "gamedata", NULL);

		for (;;) {
			SDL_Event ev;
			int w, h, files = SDL_GetAtomicInt(&done_files);
			float progress = total_size > 0 ? (float)SDL_GetAtomicInt(&done_bytes_kb) * 1024.0f / (float)total_size : 0;
			char line[300];
			SDL_FRect bar;
			int is_failed = SDL_GetAtomicInt(&finished) && SDL_GetAtomicInt(&failed);

			while (SDL_PollEvent(&ev)) {
				if (is_failed && ev.type == SDL_EVENT_FINGER_UP) {
					retry = 1;
				}
			}
			if (SDL_GetAtomicInt(&finished) && (!is_failed || retry)) {
				break;
			}

			SDL_GetRenderOutputSize(renderer, &w, &h);
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
			DrawText(renderer, w * 0.1f, h * 0.30f, h / 160.0f, "nextQuake");
			if (is_failed) {
				SDL_snprintf(line, sizeof(line), "Download failed: %s", failed_name);
				DrawText(renderer, w * 0.1f, h * 0.50f, h / 360.0f, line);
				DrawText(renderer, w * 0.1f, h * 0.58f, h / 480.0f, failed_reason);
				DrawText(renderer, w * 0.1f, h * 0.68f, h / 360.0f, "Check the connection and tap to retry.");
			}
			else {
				SDL_snprintf(line, sizeof(line), "Downloading game data: %d / %d files", files, entry_count);
				DrawText(renderer, w * 0.1f, h * 0.50f, h / 360.0f, line);
				bar.x = w * 0.1f; bar.y = h * 0.62f; bar.w = w * 0.8f; bar.h = h * 0.03f;
				SDL_RenderRect(renderer, &bar);
				bar.w *= SDL_min(progress, 1.0f);
				SDL_RenderFillRect(renderer, &bar);
			}
			SDL_RenderPresent(renderer);
			SDL_Delay(50);
		}
		SDL_WaitThread(thread, NULL);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
