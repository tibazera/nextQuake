$ErrorActionPreference = 'Stop'
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../src/in_touch.c')).Replace("`r`n", "`n")
function Read-Function($name) {
    $start = $source.IndexOf($name)
    if ($start -lt 0) { throw "Missing function: $name" }
    $start = $source.LastIndexOf("`n", $start) + 1
    $end = $source.IndexOf("`n}`n", $start) + 3
    return $source.Substring($start, $end - $start)
}
$enum = [regex]::Match($source, 'enum \{ B_FIRE[\s\S]*?B_COUNT \};').Value
$fingerTypes = [regex]::Match($source, 'typedef enum \{ ROLE_NONE[\s\S]*?} finger_t;').Value
$functions = @('IN_TouchPhysicalInput(', 'IN_TouchConsoleActive(', 'IN_TouchWantsTextInput(', 'IN_TouchConsumeKeyboardNudge(', 'GameDest(', 'ButtonVisible(', 'ButtonRepeats(', 'ButtonPress(', 'ButtonRelease(', 'IN_TouchReleaseAll(', 'IN_TouchFrame(', 'FindFinger(', 'HasRole(', 'UpdateStick(', 'IN_TouchFinger(', 'IN_TouchMove(') | ForEach-Object { Read-Function $_ }
$harness = @'
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
typedef bool qbool;
#define B_FIRST_NAV B_UP
typedef struct { float forwardmove, sidemove; } usercmd_t;
#define YAW 1
#define PITCH 0
#define max(a,b) ((a) > (b) ? (a) : (b))
#define bound(a,b,c) ((b) < (a) ? (a) : ((b) > (c) ? (c) : (b)))
static struct { float value; } cl_movespeedkey = {2}, cl_forwardspeed = {200}, cl_sidespeed = {200}, cl_yawspeed = {140}, cl_pitchspeed = {150}, in_touch_look_sens = {1}, cl_independentPhysics = {0};
static struct { int state; } in_speed;
static struct { float viewangles[3], minpitch, maxpitch; bool paused, spectator, intermission; } cl = {{0}, -70, 80, false, false, false};
enum { ca_active = 4 };
static struct { double trueframetime; bool demoplayback; int state; } cls = {0, false, ca_active};
#define SCR_NEED_CONSOLE_BACKGROUND (cls.state < ca_active && !cl.intermission)
static double physframetime;
static struct { int conwidth, conheight; } vid = {1920, 1080};
static float stick_x, stick_y, look_dx, look_dy;
static void V_StopPitchDrift(void) {}
enum { key_game, key_console, key_message, key_menu };
enum { K_ESCAPE = 27, K_MOUSE1 = 200 };
static int key_dest;
static bool active = true, auto_touch = true, kbd_nudge;
static bool last_game_dest;
static bool imgui_open, imgui_wants_text;
static qbool M_ImGui_IsOpen(void) { return imgui_open; }
static int MenuUI_WantsTextInput(void) { return imgui_wants_text; }
static int attack_fingers;
static double last_look_tap = -1.0;
static float last_look_tap_x, last_look_tap_y;
static double last_physical_key, last_physical_mouse, next_repeat;
static struct { int key; } buttons[64];
static const char *command;
static double test_time = 100.0;
static double Sys_DoubleTime(void) { return test_time; }
static void Cbuf_AddText(const char *text) { command = text; }
static void TapKey(int key) { (void)key; }
'@
$fingerHarness = @'
#define MAX_FINGERS 10
#define PHYSICAL_GRACE_SECONDS 3.0
enum { IN_TOUCH_DOWN, IN_TOUCH_MOTION, IN_TOUCH_UP, IN_TOUCH_CANCEL };
static finger_t fingers[MAX_FINGERS];
static struct { int integer; } in_touch = {2};
static bool SDL_HasKeyboard(void) { return true; }
static bool SDL_HasMouse(void) { return true; }
static void Key_Event(int key, bool down) { (void)key; (void)down; }
static int hit_button = B_FIRE, click_state, click_delay;
static bool click_release_req;
static float cursor_x, cursor_y;
static int HitButton(float x, float y) { (void)x; (void)y; return hit_button; }
static float UnitSize(void) { return 10; }
static int VID_RenderWidth2D(void) { return vid.conwidth; }
static int VID_RenderHeight2D(void) { return vid.conheight; }
static void Com_Printf(const char *text, ...) { (void)text; }
static void ClickRelease(void) { click_state = 0; }
'@
$checks = @'
int main(void) {
    key_dest = key_game;
    cls.state = 0;
    active = false;
    IN_TouchFrame();
    assert(active);
    assert(IN_TouchWantsTextInput());
    assert(!GameDest());
    cls.state = ca_active;
    assert(!IN_TouchWantsTextInput());
    imgui_open = imgui_wants_text = true;
    assert(IN_TouchWantsTextInput());
    imgui_open = imgui_wants_text = false;
    assert(GameDest());
    ButtonPress(B_NEXT);
    assert(strcmp(command, "weapon 10\n") == 0);
    ButtonPress(B_PREV);
    assert(strcmp(command, "weapon 12\n") == 0);
    key_dest = key_message;
    IN_TouchPhysicalInput(true);
    assert(!IN_TouchWantsTextInput());
    ButtonPress(B_CHAT);
    assert(strcmp(command, "messagemode\n") == 0);
    assert(IN_TouchWantsTextInput());
    assert(IN_TouchConsumeKeyboardNudge());
    assert(!IN_TouchConsumeKeyboardNudge());
    IN_TouchPhysicalInput(true);
    ButtonPress(B_FIRE);
    assert(!IN_TouchWantsTextInput());
    ButtonRelease(B_FIRE);
    ButtonPress(B_KBD);
    assert(IN_TouchWantsTextInput());
    IN_TouchPhysicalInput(true);
    ButtonPress(B_CONSOLE);
    assert(strcmp(command, "toggleconsole\n") == 0);
    key_dest = key_console;
    assert(IN_TouchWantsTextInput());
    key_dest = key_game;
    assert(!IN_TouchWantsTextInput());
    command = NULL;
    ButtonPress(B_KBD);
    assert(command && strcmp(command, "toggleconsole\n") == 0);
    key_dest = key_console;
    assert(IN_TouchWantsTextInput());
    assert(IN_TouchConsumeKeyboardNudge());
    assert(ButtonVisible(B_KBD) && ButtonVisible(B_CONSOLE));
    assert(!ButtonVisible(B_CHAT));
    ButtonPress(B_CONSOLE);
    assert(strcmp(command, "toggleconsole\n") == 0);
    key_dest = key_game;
    assert(!IN_TouchWantsTextInput());
    key_dest = key_message;
    active = false;
    assert(!IN_TouchWantsTextInput());
    active = true;
    key_dest = key_game;
    stick_x = 1;
    stick_y = -1;
    cls.trueframetime = physframetime = 1.0 / 60.0;
    usercmd_t cmd = {0};
    for (int i = 0; i < 120; ++i) IN_TouchMove(&cmd);
    assert(cl.viewangles[YAW] == 0 && cl.viewangles[PITCH] == 0);
    assert(cmd.sidemove > 0 && cmd.forwardmove > 0);
    look_dx = 0.1f;
    look_dy = -0.1f;
    IN_TouchMove(&cmd);
    assert(fabs(cl.viewangles[YAW] + 19) < 0.01);
    assert(cl.viewangles[PITCH] < 0);
    float yaw = cl.viewangles[YAW], pitch = cl.viewangles[PITCH];
    for (int i = 0; i < 120; ++i) IN_TouchMove(&cmd);
    assert(cl.viewangles[YAW] == yaw && cl.viewangles[PITCH] == pitch);
    in_touch_look_sens.value = 0.5;
    look_dx = 0.1f;
    IN_TouchMove(&cmd);
    assert(fabs(cl.viewangles[YAW] - yaw + 9.5) < 0.01);
    look_dy = -10;
    IN_TouchMove(&cmd);
    assert(cl.viewangles[PITCH] == cl.minpitch);
    look_dy = 10;
    IN_TouchMove(&cmd);
    assert(cl.viewangles[PITCH] == cl.maxpitch);
    cl.paused = true;
    yaw = cl.viewangles[YAW];
    look_dx = 0.1f;
    IN_TouchMove(&cmd);
    assert(cl.viewangles[YAW] == yaw && look_dx == 0);
    cl.paused = false;
    in_touch_look_sens.value = 1;
    cl.viewangles[YAW] = cl.viewangles[PITCH] = 0;
    IN_TouchFinger(IN_TOUCH_DOWN, 1, 0.8f, 0.8f, 0, 0);
    assert(strcmp(command, "+attack\n") == 0);
    IN_TouchFinger(IN_TOUCH_MOTION, 1, 0.9f, 0.7f, 0.1f, -0.1f);
    IN_TouchMove(&cmd);
    assert(fabs(cl.viewangles[YAW] + 19) < 0.01 && cl.viewangles[PITCH] < 0);
    assert(strcmp(command, "+attack\n") == 0);
    IN_TouchFinger(IN_TOUCH_UP, 1, 0.9f, 0.7f, 0, 0);
    assert(strcmp(command, "-attack\n") == 0);
    hit_button = B_JUMP;
    IN_TouchFinger(IN_TOUCH_DOWN, 2, 0.8f, 0.8f, 0, 0);
    IN_TouchFinger(IN_TOUCH_MOTION, 2, 0.9f, 0.7f, 0.1f, -0.1f);
    assert(look_dx == 0 && look_dy == 0);
    IN_TouchFinger(IN_TOUCH_UP, 2, 0.9f, 0.7f, 0, 0);
    hit_button = -1;
    IN_TouchFinger(IN_TOUCH_DOWN, 3, 0.2f, 0.8f, 0, 0);
    IN_TouchFinger(IN_TOUCH_MOTION, 3, 0.3f, 0.7f, 0.1f, -0.1f);
    assert(look_dx == 0 && look_dy == 0 && stick_y < 0);
    IN_TouchFinger(IN_TOUCH_UP, 3, 0.3f, 0.7f, 0, 0);
    assert(stick_x == 0 && stick_y == 0);
    command = NULL;
    IN_TouchFinger(IN_TOUCH_DOWN, 4, 0.8f, 0.5f, 0, 0);
    assert(command == NULL);
    IN_TouchFinger(IN_TOUCH_MOTION, 4, 0.9f, 0.4f, 0.1f, -0.1f);
    assert(look_dx > 0 && look_dy < 0 && command == NULL);
    IN_TouchFinger(IN_TOUCH_UP, 4, 0.9f, 0.4f, 0, 0);
    assert(command == NULL);
    IN_TouchFinger(IN_TOUCH_DOWN, 5, 0.8f, 0.5f, 0, 0);
    IN_TouchFinger(IN_TOUCH_UP, 5, 0.8f, 0.5f, 0, 0);
    IN_TouchFinger(IN_TOUCH_DOWN, 6, 0.8f, 0.5f, 0, 0);
    assert(command && strcmp(command, "+attack\n") == 0);
    IN_TouchFinger(IN_TOUCH_MOTION, 6, 0.9f, 0.4f, 0.1f, -0.1f);
    assert(look_dx > 0 && look_dy < 0);
    IN_TouchFinger(IN_TOUCH_CANCEL, 6, 0.9f, 0.4f, 0, 0);
    assert(strcmp(command, "-attack\n") == 0 && attack_fingers == 0);
    command = NULL;
    IN_TouchFinger(IN_TOUCH_DOWN, 7, 0.8f, 0.5f, 0, 0);
    IN_TouchFinger(IN_TOUCH_UP, 7, 0.8f, 0.5f, 0, 0);
    test_time += 0.4;
    IN_TouchFinger(IN_TOUCH_DOWN, 8, 0.8f, 0.5f, 0, 0);
    assert(command == NULL);
    IN_TouchFinger(IN_TOUCH_UP, 8, 0.8f, 0.5f, 0, 0);
    IN_TouchFinger(IN_TOUCH_DOWN, 9, 0.6f, 0.5f, 0, 0);
    assert(command == NULL);
    IN_TouchFinger(IN_TOUCH_UP, 9, 0.6f, 0.5f, 0, 0);
    IN_TouchFinger(IN_TOUCH_DOWN, 10, 0.6f, 0.5f, 0, 0);
    assert(strcmp(command, "+attack\n") == 0);
    IN_TouchReleaseAll();
    assert(strcmp(command, "-attack\n") == 0 && attack_fingers == 0);
    return 0;
}
'@
$testDir = Join-Path ([IO.Path]::GetTempPath()) ('ezquake-touch-' + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($testDir) | Out-Null
[IO.File]::WriteAllText((Join-Path $testDir 'test.c'), $harness + "`n" + $enum + "`n" + $fingerTypes + "`n" + $fingerHarness + "`n" + ($functions -join "`n") + "`n" + $checks.Replace('\\n', '\n'))
$batch = '@echo off' + "`r`n" + 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul' + "`r`n" + 'cd /d "' + $testDir + '"' + "`r`n" + 'cl /nologo /W4 test.c /Fe:test.exe > build.log 2>&1' + "`r`n" + 'if errorlevel 1 (type build.log & exit /b 1)' + "`r`n" + 'test.exe'
[IO.File]::WriteAllText((Join-Path $testDir 'run.bat'), $batch)
& cmd.exe /c (Join-Path $testDir 'run.bat')
if ($LASTEXITCODE -ne 0) { throw 'Touch keyboard regression test failed' }
Write-Output 'Touch keyboard regression test passed'
