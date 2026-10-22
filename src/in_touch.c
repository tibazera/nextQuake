/*
Android on-screen touch controls. See in_touch.h.

Game: floating move stick on the left, free look on the rest of the screen,
buttons on the right (fire, jump, weapon next/prev) and a top row
(scores, chat, console, menu).
Menus/console/chat: d-pad, enter/back/keyboard buttons, and a tap moves the
menu cursor and clicks.
*/

#include <SDL.h>
#include "quakedef.h"
#include "in_touch.h"

#ifdef EZ_TOUCH

#ifdef __ANDROID__
#include <jni.h>
#endif
#include "input.h"
#include "keys.h"
#include "menu.h"

// menu_imgui.cpp: a Dear ImGui text field has focus
extern int MenuUI_WantsTextInput(void);
#include "draw.h"
#include "vid.h"
#include "screen.h"
#include "in_touch.h"

extern kbutton_t in_speed;
extern cvar_t cl_movespeedkey;

cvar_t in_touch            = { "in_touch",            "2" };    // 0 off, 1 on, 2 auto
cvar_t in_touch_scale      = { "in_touch_scale",      "1" };
cvar_t in_touch_alpha      = { "in_touch_alpha",      "0.35" };
cvar_t in_touch_look_sens  = { "in_touch_look_sens",  "1" };    // negative inverts
cvar_t in_touch_accel      = { "in_touch_accel",      "0" };    // fast drags turn up to (1 + accel) times as far; 0 off
cvar_t in_touch_haptic     = { "in_touch_haptic",     "0" };    // short vibration on button press
cvar_t in_touch_console_size = { "in_touch_console_size", "0.4" };   // console height (fraction of the screen) while the soft keyboard is up

enum { B_FIRE, B_JUMP, B_NEXT, B_PREV, B_SCORES, B_CHAT, B_CONSOLE, B_MENU,
       B_UP, B_DOWN, B_LEFT, B_RIGHT, B_ENTER, B_BACK, B_KBD, B_COUNT };
#define B_FIRST_NAV B_UP

// Kenney "Mobile Controls" (CC0) bases, game-icons.net (CC BY 3.0) icons; see assets/touch.
enum {
	P_BUTTON, P_HELD, P_STICK_BASE, P_STICK_NUB,
	P_DPAD_N, P_DPAD_S, P_DPAD_E, P_DPAD_W,
	P_ICON_FIRE, P_ICON_JUMP, P_ICON_MENU, P_ICON_TALK, P_ICON_KEY, P_ICON_CHECK,
	P_ICON_COG, P_ICON_SKULL, P_ICON_L, P_ICON_R, P_ICON_BACK, P_COUNT
};
static const char *pic_names[P_COUNT] = {
	"gfx/touch/button", "gfx/touch/button_held", "gfx/touch/stick_base", "gfx/touch/stick_nub",
	"gfx/touch/dpad_north", "gfx/touch/dpad_south", "gfx/touch/dpad_east", "gfx/touch/dpad_west",
	"gfx/touch/icon_fire", "gfx/touch/icon_jump", "gfx/touch/icon_menu", "gfx/touch/icon_talk",
	"gfx/touch/icon_key", "gfx/touch/icon_checkmark", "gfx/touch/icon_cog", "gfx/touch/icon_skull",
	"gfx/touch/icon_button_l", "gfx/touch/icon_button_r", "gfx/touch/icon_arrow_curved",
};

typedef struct { const char *label; float cx, cy, r; int key; int icon; int shape; } touch_button_t;   // shape: P_DPAD_* piece, or -1 for a round button

static touch_button_t buttons[B_COUNT] = {
	[B_FIRE] = { "FIRE", .icon = P_ICON_FIRE, .shape = -1 }, [B_JUMP] = { "JUMP", .icon = P_ICON_JUMP, .shape = -1 },
	[B_NEXT] = { "W+", .icon = P_ICON_R, .shape = -1 }, [B_PREV] = { "W-", .icon = P_ICON_L, .shape = -1 },
	[B_SCORES] = { "TAB", .icon = P_ICON_SKULL, .shape = -1 }, [B_CHAT] = { "SAY", .icon = P_ICON_TALK, .shape = -1 },
	[B_CONSOLE] = { "CON", .icon = P_ICON_COG, .shape = -1 }, [B_MENU] = { "ESC", .icon = P_ICON_MENU, .shape = -1 },
	[B_UP] = { "^", .key = K_UPARROW, .icon = -1, .shape = P_DPAD_N }, [B_DOWN] = { "v", .key = K_DOWNARROW, .icon = -1, .shape = P_DPAD_S },
	[B_LEFT] = { "<", .key = K_LEFTARROW, .icon = -1, .shape = P_DPAD_W }, [B_RIGHT] = { ">", .key = K_RIGHTARROW, .icon = -1, .shape = P_DPAD_E },
	[B_ENTER] = { "OK", .key = K_ENTER, .icon = P_ICON_CHECK, .shape = -1 }, [B_BACK] = { "BACK", .key = K_ESCAPE, .icon = P_ICON_BACK, .shape = -1 },
	[B_KBD] = { "KBD", .icon = P_ICON_KEY, .shape = -1 },
};

static mpic_t *Pic(int id)
{
	// looked up by name each time: cached pics are dropped on vid_restart
	return Draw_CachePicSafe(pic_names[id], false, true);
}

typedef enum { ROLE_NONE, ROLE_STICK, ROLE_LOOK, ROLE_BUTTON, ROLE_CURSOR } finger_role_t;
typedef struct {
	qbool used;
	long long id;
	finger_role_t role;
	int button;
	float ax, ay; // stick anchor (normalized)
	float x, y;
	double down_time;
	qbool firing;
} finger_t;

#define MAX_FINGERS 10
static finger_t fingers[MAX_FINGERS];

static qbool active;
static qbool last_game_dest;
static float stick_x, stick_y;     // -1..1
static float look_dx, look_dy;     // accumulated, normalized screen fractions
static qbool kbd_nudge;
static int attack_fingers;
static double last_look_tap = -1.0;
static float last_look_tap_x, last_look_tap_y;
static double next_repeat;

// cursor click is delayed so the menu sees the new pointer position first
static int click_state;            // 0 idle, 1 waiting to press, 2 pressed
static int click_delay;
static qbool click_release_req;

void IN_TouchInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&in_touch);
	Cvar_Register(&in_touch_scale);
	Cvar_Register(&in_touch_alpha);
	Cvar_Register(&in_touch_look_sens);
	Cvar_Register(&in_touch_accel);
	Cvar_Register(&in_touch_haptic);
	Cvar_Register(&in_touch_console_size);
	SDL_DisableScreenSaver();   // keep the screen awake while playing
	Cvar_ResetCurrentGroup();
#ifndef __ANDROID__
	// iOS ships the control art inside the app bundle; the engine loads it as loose files
	for (int i = 0; i < P_COUNT; i++) {
		char src[MAX_OSPATH], dst[MAX_OSPATH];
		const char *name = pic_names[i] + strlen("gfx/touch/");

		snprintf(dst, sizeof(dst), "%s/ezquake/gfx/touch/%s.png", com_basedir, name);
		if (!SDL_GetPathInfo(dst, NULL)) {
			snprintf(src, sizeof(src), "%stouch/%s.png", SDL_GetBasePath(), name);
			SDL_CreateDirectory(va("%s/ezquake/gfx/touch", com_basedir));
			SDL_CopyFile(src, dst);
		}
	}
#endif
}

qbool IN_TouchActive(void)
{
	return active;
}

// Auto mode follows the last kind of input used: any real mouse/keyboard event
// switches the overlay (and the soft keyboard) off, a finger on the screen
// switches it back on. SDL_HasKeyboard()/SDL_HasMouse() alone are not
// reliable on Android (a hub keyboard/mouse can go unreported).
static double last_physical_mouse;
static double last_physical_key = -11.0;
static qbool auto_touch = true;
#define PHYSICAL_GRACE_SECONDS 3.0

void IN_TouchPhysicalInput(qbool key)
{
	double now = Sys_DoubleTime();

	if (key) {
		last_physical_key = now;
	}
	else {
		last_physical_mouse = now;
	}
	auto_touch = false;
}

qbool IN_TouchConsoleActive(void)
{
	return key_dest == key_console || (key_dest == key_game && SCR_NEED_CONSOLE_BACKGROUND);
}

qbool IN_TouchWantsTextInput(void)
{
	// The soft keyboard also produces TEXTINPUT, which would double every key
	// typed on a physical keyboard; keep it closed while one is in use.
	return active && (IN_TouchConsoleActive() || key_dest == key_message || (M_ImGui_IsOpen() && MenuUI_WantsTextInput()))
		&& Sys_DoubleTime() - last_physical_key > 10.0;
}

// While the soft keyboard is up, keep the console (and so its input line) in the part of the
// screen the keyboard does not cover.
float IN_TouchConsoleLines(float lines)
{
	if (IN_TouchWantsTextInput()) {
		return min(lines, vid.height * bound(0.2f, in_touch_console_size.value, 0.8f));
	}
	return lines;
}

qbool IN_TouchConsumeKeyboardNudge(void)
{
	qbool v = kbd_nudge;
	kbd_nudge = false;
	return v;
}

static qbool GameDest(void)
{
	return key_dest == key_game && !IN_TouchConsoleActive();
}

static float UnitSize(void)
{
	return vid.conheight * 0.11f * bound(0.5f, in_touch_scale.value, 2.0f);
}

static void Layout(void)
{
	float W = vid.conwidth, H = vid.conheight, u = UnitSize();
	float cy = H - 2.3f * u;

#define PLACE(b, x, y, rad) do { buttons[b].cx = (x); buttons[b].cy = (y); buttons[b].r = (rad); } while (0)
	// right cluster: FIRE in the corner, the others on an arc around it (left, up-left, up)
	float fx = W - 1.6f * u, fy = H - 1.6f * u, arc = 2.2f * u, diag = arc * 0.7071f;
	PLACE(B_FIRE,  fx, fy, 1.0f * u);
	PLACE(B_JUMP,  fx - arc, fy, 0.8f * u);
	PLACE(B_PREV,  fx - diag, fy - diag, 0.65f * u);
	PLACE(B_NEXT,  fx, fy - arc, 0.65f * u);
	PLACE(B_ENTER, fx, fy, 1.0f * u);
	PLACE(B_BACK,  fx - arc, fy, 0.8f * u);

	// top row, right aligned: same size, height and spacing
	float ty = 0.8f * u, tr = 0.55f * u, step = 1.4f * u;
	PLACE(B_MENU,    W - 0.8f * u, ty, tr);
	PLACE(B_CONSOLE, W - 0.8f * u - step, ty, tr);
	PLACE(B_CHAT,    W - 0.8f * u - step * 2, ty, tr);
	PLACE(B_SCORES,  W - 0.8f * u - step * 3, ty, tr);
	PLACE(B_KBD,     W - 0.8f * u - (GameDest() ? step * 4 : 0), ty, tr);

	PLACE(B_UP,    2.4f * u, cy - 1.5f * u, 0.7f * u);
	PLACE(B_DOWN,  2.4f * u, cy + 1.5f * u, 0.7f * u);
	PLACE(B_LEFT,  0.9f * u, cy, 0.7f * u);
	PLACE(B_RIGHT, 3.9f * u, cy, 0.7f * u);
#undef PLACE
}

static qbool ButtonVisible(int b)
{
	return b == B_KBD || (b == B_CONSOLE && key_dest == key_console) || (b < B_FIRST_NAV) == GameDest();
}

static int HitButton(float nx, float ny)
{
	float px = nx * vid.conwidth, py = ny * vid.conheight;
	int b;

	Layout();
	for (b = 0; b < B_COUNT; ++b) {
		float dx = px - buttons[b].cx, dy = py - buttons[b].cy, r = buttons[b].r * 1.15f;

		if (ButtonVisible(b) && dx * dx + dy * dy <= r * r) {
			return b;
		}
	}
	return -1;
}

static void TapKey(int key)
{
	Key_Event(key, true);
	Key_Event(key, false);
}

static qbool ButtonRepeats(int b)
{
	return b >= B_UP && b <= B_RIGHT;
}

// short "key click" vibration (SDLActivity.performHapticClick), also the menus' press feedback
void IN_TouchHapticPulse(void)
{
#ifdef __ANDROID__
	JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
	jobject activity = (jobject)SDL_GetAndroidActivity();
	jclass cls;
	jmethodID mid;

	if (!env || !activity) {
		return;
	}
	cls = (*env)->GetObjectClass(env, activity);
	mid = cls ? (*env)->GetStaticMethodID(env, cls, "performHapticClick", "()V") : NULL;
	if (mid) {
		(*env)->CallStaticVoidMethod(env, cls, mid);
	}
	else {
		(*env)->ExceptionClear(env);
	}
	if (cls) {
		(*env)->DeleteLocalRef(env, cls);
	}
	(*env)->DeleteLocalRef(env, activity);
#endif // ponytail: no haptics on iOS yet, UIImpactFeedbackGenerator needs an Objective-C shim
}

static void Haptic(void)
{
	if (in_touch_haptic.integer > 0) {
		IN_TouchHapticPulse();
	}
}

static void ButtonPress(int b)
{
	Haptic();
	switch (b) {
	case B_FIRE:
		if (attack_fingers++ == 0) Cbuf_AddText("+attack\n");
		break;
	case B_JUMP:    Cbuf_AddText("+jump\n"); break;
	case B_NEXT:    Cbuf_AddText("weapon 10\n"); break;
	case B_PREV:    Cbuf_AddText("weapon 12\n"); break;
	case B_SCORES:  Cbuf_AddText("+showscores\n"); break;
	case B_CHAT:
	case B_CONSOLE:
	case B_KBD:
		// An explicit touch request overrides the physical-keyboard grace period.
		last_physical_key = Sys_DoubleTime() - 11.0;
		kbd_nudge = true;
		if (b == B_CHAT) Cbuf_AddText("messagemode\n");
		if (b == B_CONSOLE || (b == B_KBD && key_dest != key_console && key_dest != key_message)) Cbuf_AddText("toggleconsole\n");
		break;
	case B_MENU:    TapKey(K_ESCAPE); break;
	default:        TapKey(buttons[b].key); break;
	}
	if (ButtonRepeats(b)) {
		next_repeat = Sys_DoubleTime() + 0.4;
	}
}

static void ButtonRelease(int b)
{
	switch (b) {
	case B_FIRE:
		if (attack_fingers > 0 && --attack_fingers == 0) Cbuf_AddText("-attack\n");
		break;
	case B_JUMP:   Cbuf_AddText("-jump\n"); break;
	case B_SCORES: Cbuf_AddText("-showscores\n"); break;
	default: break;
	}
}

static void ClickRelease(void)
{
	if (click_state == 2) {
		Key_Event(K_MOUSE1, false);
	}
	click_state = 0;
	click_release_req = false;
}

void IN_TouchReleaseAll(void)
{
	int i;

	for (i = 0; i < MAX_FINGERS; ++i) {
		if (fingers[i].used && fingers[i].role == ROLE_BUTTON) {
			ButtonRelease(fingers[i].button);
		}
		else if (fingers[i].used && fingers[i].role == ROLE_LOOK && fingers[i].firing) {
			ButtonRelease(B_FIRE);
		}
	}
	memset(fingers, 0, sizeof(fingers));
	ClickRelease();
	stick_x = stick_y = 0;
	look_dx = look_dy = 0;
	last_look_tap = -1.0;
}

void IN_TouchFrame(void)
{
	qbool kb = SDL_HasKeyboard(), ms = SDL_HasMouse();
	qbool want;
	static int last_kb = -1, last_ms = -1;
	int mode = in_touch.integer;
	qbool game = GameDest();

	want = mode == 1 || (mode == 2 && auto_touch);

	if (want != active || kb != last_kb || ms != last_ms) {
		Com_Printf("touch: mode=%d keyboard=%d mouse=%d -> %s\n", mode, kb, ms, want ? "on" : "off");
		last_kb = kb;
		last_ms = ms;
	}
	if (want != active) {
		IN_TouchReleaseAll();
		active = want;
	}
	if (!active) {
		return;
	}

	if (game != last_game_dest) {
		IN_TouchReleaseAll();
		last_game_dest = game;
	}

	if (click_state == 1 && --click_delay <= 0) {
		Key_Event(K_MOUSE1, true);
		click_state = 2;
		if (click_release_req) {
			ClickRelease();
		}
	}

	if (!game) {
		double now = Sys_DoubleTime();
		int i;

		for (i = 0; i < MAX_FINGERS; ++i) {
			if (fingers[i].used && fingers[i].role == ROLE_BUTTON && ButtonRepeats(fingers[i].button) && now >= next_repeat) {
				TapKey(buttons[fingers[i].button].key);
				next_repeat = now + 0.08;
			}
		}
	}
}

static finger_t *FindFinger(long long id)
{
	int i;

	for (i = 0; i < MAX_FINGERS; ++i) {
		if (fingers[i].used && fingers[i].id == id) {
			return &fingers[i];
		}
	}
	return NULL;
}

static qbool HasRole(finger_role_t role)
{
	int i;

	for (i = 0; i < MAX_FINGERS; ++i) {
		if (fingers[i].used && fingers[i].role == role) {
			return true;
		}
	}
	return false;
}

static void UpdateStick(finger_t *f)
{
	float r = 2.0f * UnitSize();
	float dx = (f->x - f->ax) * vid.conwidth / r, dy = (f->y - f->ay) * vid.conheight / r;
	float len = sqrtf(dx * dx + dy * dy);

	if (len > 1.0f) {
		dx /= len;
		dy /= len;
		len = 1.0f;
	}
	if (len < 0.15f) {
		dx = dy = 0;
	}
	else {
		// rescale past the dead zone so the throw starts from 0
		float k = (len - 0.15f) / (0.85f * len);

		dx *= k;
		dy *= k;
	}
	stick_x = dx;
	stick_y = dy;
}

void IN_TouchFinger(int phase, long long id, float x, float y, float dx, float dy)
{
	finger_t *f;

	// the Dear ImGui menus take the finger as a pointer (see vid_sdl2.c)
	if (M_ImGui_IsOpen()) {
		return;
	}

	if (!active) {
		// Auto mode: a real touch turns the overlay on, unless a mouse or
		// keyboard was used a moment ago (then it is a stray/synthesised event).
		double now = Sys_DoubleTime();

		if (phase != IN_TOUCH_DOWN || in_touch.integer != 2 || auto_touch
			|| now - last_physical_mouse < PHYSICAL_GRACE_SECONDS || now - last_physical_key < PHYSICAL_GRACE_SECONDS) {
			return;
		}
		auto_touch = true;
		active = true;
		IN_TouchReleaseAll();
		Com_Printf("touch: screen touched -> on\n");
	}

	if (phase == IN_TOUCH_DOWN) {
		int i, b = HitButton(x, y);

		if (FindFinger(id)) {
			return;
		}
		for (i = 0; i < MAX_FINGERS && fingers[i].used; ++i)
			;
		if (i == MAX_FINGERS) {
			return;
		}
		f = &fingers[i];
		memset(f, 0, sizeof(*f));
		f->used = true;
		f->id = id;
		f->x = f->ax = x;
		f->y = f->ay = y;
		f->down_time = Sys_DoubleTime();

		if (b >= 0) {
			f->role = ROLE_BUTTON;
			f->button = b;
			ButtonPress(b);
		}
		else if (GameDest()) {
			if (x < 0.42f && !HasRole(ROLE_STICK)) {
				f->role = ROLE_STICK;
			}
			else if (x >= 0.42f && !HasRole(ROLE_LOOK)) {
				float tx = (x - last_look_tap_x) * vid.conwidth;
				float ty = (y - last_look_tap_y) * vid.conheight;
				float radius = UnitSize();

				f->role = ROLE_LOOK;
				f->firing = last_look_tap >= 0 && f->down_time - last_look_tap <= 0.3 && tx * tx + ty * ty <= radius * radius;
				last_look_tap = -1.0;
				if (f->firing) ButtonPress(B_FIRE);
			}
		}
		else if (key_dest == key_menu && !HasRole(ROLE_CURSOR)) {
			f->role = ROLE_CURSOR;
			cursor_x = x * VID_RenderWidth2D();
			cursor_y = y * VID_RenderHeight2D();
			click_state = 1;
			click_delay = 2;
			click_release_req = false;
		}
		return;
	}

	f = FindFinger(id);
	if (!f) {
		return;
	}
	f->x = x;
	f->y = y;

	if (phase == IN_TOUCH_MOTION) {
		switch (f->role) {
		case ROLE_STICK:
			UpdateStick(f);
			break;
		case ROLE_LOOK:
			look_dx += dx;
			look_dy += dy;
			break;
		case ROLE_BUTTON:
			if (f->button == B_FIRE) {
				look_dx += dx;
				look_dy += dy;
			}
			break;
		case ROLE_CURSOR:
			cursor_x = x * VID_RenderWidth2D();
			cursor_y = y * VID_RenderHeight2D();
			break;
		default:
			break;
		}
		return;
	}

	// UP or CANCEL
	switch (f->role) {
	case ROLE_STICK:
		stick_x = stick_y = 0;
		break;
	case ROLE_BUTTON:
		ButtonRelease(f->button);
		break;
	case ROLE_LOOK:
		if (f->firing) {
			ButtonRelease(B_FIRE);
		}
		else if (phase == IN_TOUCH_UP && Sys_DoubleTime() - f->down_time <= 0.25) {
			float tx = (f->x - f->ax) * vid.conwidth;
			float ty = (f->y - f->ay) * vid.conheight;
			float radius = UnitSize();

			if (tx * tx + ty * ty <= radius * radius) {
				last_look_tap = Sys_DoubleTime();
				last_look_tap_x = f->x;
				last_look_tap_y = f->y;
			}
		}
		break;
	case ROLE_CURSOR:
		if (click_state == 1) {
			click_release_req = true;
		}
		else {
			ClickRelease();
		}
		break;
	default:
		break;
	}
	f->used = false;
}

void IN_TouchMove(usercmd_t *cmd)
{
	float speed;

	if (!active || !GameDest()) {
		return;
	}

	speed = (in_speed.state & 1) ? cl_movespeedkey.value : 1.0f;
	cmd->forwardmove += -stick_y * speed * cl_forwardspeed.value;
	cmd->sidemove += stick_x * speed * cl_sidespeed.value;

	if (look_dx != 0 || look_dy != 0) {
		float sens = in_touch_look_sens.value * 190.0f;

		if (in_touch_accel.value > 0 && cls.frametime > 0) {
			// drag speed in screen widths per second; 2.5 or more gets the full boost
			float speed = sqrtf(look_dx * look_dx + look_dy * look_dy) / cls.frametime;

			sens *= 1.0f + in_touch_accel.value * min(speed / 2.5f, 1.0f);
		}

		if (!cl.paused || cls.demoplayback || cl.spectator) {
			cl.viewangles[YAW] -= look_dx * sens;
			cl.viewangles[PITCH] += look_dy * sens * ((float)vid.conheight / max(1, vid.conwidth));
		}
		cl.viewangles[PITCH] = bound(cl.minpitch, cl.viewangles[PITCH], cl.maxpitch);
		V_StopPitchDrift();
		look_dx = look_dy = 0;
	}
}

static void DrawButton(int b, qbool held, float alpha)
{
	touch_button_t *btn = &buttons[b];
	float scale = btn->r / 18.0f;
	byte a = (byte)(bound(0.0f, alpha, 1.0f) * (held ? 255 : 160));
	float boost = bound(0.0f, alpha * 2.0f, 1.0f);
	mpic_t *shape = btn->shape >= 0 ? Pic(btn->shape) : Pic(P_BUTTON);
	float d = btn->r * 2.0f;

	if (shape) {
		mpic_t *icon = btn->icon >= 0 ? Pic(b == B_CONSOLE && key_dest == key_console ? P_ICON_BACK : btn->icon) : NULL;
		mpic_t *highlight = held ? Pic(P_HELD) : NULL;

		Draw_SAlphaSubPic2(btn->cx - btn->r, btn->cy - btn->r, shape, 0, 0, shape->width, shape->height, d / shape->width, d / shape->height, held ? 1.0f : boost);
		if (highlight && btn->shape < 0) {
			Draw_SAlphaSubPic2(btn->cx - btn->r, btn->cy - btn->r, highlight, 0, 0, highlight->width, highlight->height, d / highlight->width, d / highlight->height, 1.0f);
		}
		if (icon) {
			Draw_FitPicAlphaCenter(btn->cx - btn->r * 0.55f, btn->cy - btn->r * 0.55f, d * 0.55f, d * 0.55f, icon, held ? 1.0f : boost);
		}
		return;
	}

	Draw_AlphaCircleFillRGB(btn->cx, btn->cy, btn->r, RGBA_TO_COLOR(held ? 255 : 40, held ? 255 : 40, held ? 255 : 40, a));
	Draw_AlphaCircleOutlineRGB(btn->cx, btn->cy, btn->r, 2, RGBA_TO_COLOR(255, 255, 255, (byte)(alpha * 255)));
	Draw_SStringAligned((int)btn->cx, (int)(btn->cy - 4 * scale), btn->label, scale, 1.0f, false, text_align_center, 0);
}

void IN_TouchDraw(void)
{
	float alpha = bound(0.1f, in_touch_alpha.value, 1.0f);
	int b, i;

	if (!active || M_ImGui_IsOpen()) {
		return;
	}

	Layout();
	for (b = 0; b < B_COUNT; ++b) {
		qbool held = false;

		if (!ButtonVisible(b)) {
			continue;
		}
		for (i = 0; i < MAX_FINGERS; ++i) {
			held |= fingers[i].used && fingers[i].role == ROLE_BUTTON && fingers[i].button == b;
		}
		DrawButton(b, held, alpha);
	}

	if (GameDest()) {
		float r = 2.0f * UnitSize();
		float bx = 2.6f * UnitSize(), by = vid.conheight - 2.6f * UnitSize();
		float kx = 0, ky = 0;

		for (i = 0; i < MAX_FINGERS; ++i) {
			if (fingers[i].used && fingers[i].role == ROLE_STICK) {
				bx = fingers[i].ax * vid.conwidth;
				by = fingers[i].ay * vid.conheight;
				kx = stick_x * r;
				ky = stick_y * r;
			}
		}
		mpic_t *base = Pic(P_STICK_BASE);
		mpic_t *nub = Pic(P_STICK_NUB);

		if (base && nub) {
			float boost = bound(0.0f, alpha * 2.0f, 1.0f);

			Draw_FitPicAlphaCenter(bx - r, by - r, r * 2, r * 2, base, boost);
			Draw_FitPicAlphaCenter(bx + kx - 0.8f * UnitSize(), by + ky - 0.8f * UnitSize(), 1.6f * UnitSize(), 1.6f * UnitSize(), nub, boost);
		}
		else {
			Draw_AlphaCircleOutlineRGB(bx, by, r, 2, RGBA_TO_COLOR(255, 255, 255, (byte)(alpha * 255)));
			Draw_AlphaCircleFillRGB(bx + kx, by + ky, 0.8f * UnitSize(), RGBA_TO_COLOR(255, 255, 255, (byte)(alpha * 200)));
		}
	}
}

#endif // EZ_TOUCH
