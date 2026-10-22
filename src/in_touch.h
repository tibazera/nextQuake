/*
Android on-screen touch controls.

Active when cvar in_touch is 1, or 2 (auto) following the last input used.
When inactive everything here is a cheap no-op so mouse/keyboard play is
unaffected.
*/

#ifndef EZQUAKE_IN_TOUCH_H
#define EZQUAKE_IN_TOUCH_H

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
#define EZ_TOUCH
#endif

#ifdef EZ_TOUCH

void IN_TouchInit(void);
void IN_TouchFrame(void);   // once per frame from the event loop
qbool IN_TouchActive(void);
qbool IN_TouchConsoleActive(void);
qbool IN_TouchWantsTextInput(void);
float IN_TouchConsoleLines(float lines);
qbool IN_TouchConsumeKeyboardNudge(void);
void IN_TouchPhysicalInput(qbool key);
void IN_TouchFinger(int phase, long long finger_id, float x, float y, float dx, float dy);
void IN_TouchMove(usercmd_t *cmd);
void IN_TouchDraw(void);
void IN_TouchReleaseAll(void);
void IN_TouchHapticPulse(void);
void VID_SafeAreaInsets(float *left, float *right);   // vid_sdl2.c, fractions of the window width
#ifndef __ANDROID__
void IOS_HapticPulse(void);   // ios_haptic.m
#endif

#define IN_TOUCH_DOWN   0
#define IN_TOUCH_MOTION 1
#define IN_TOUCH_UP     2
#define IN_TOUCH_CANCEL 3

#else

#define IN_TouchInit()                  ((void)0)
#define IN_TouchFrame()                 ((void)0)
#define IN_TouchActive()                (false)
#define IN_TouchConsoleActive()         (key_dest == key_console)
#define IN_TouchWantsTextInput()        (false)
#define IN_TouchConsoleLines(lines)     (lines)
#define IN_TouchConsumeKeyboardNudge()  (false)
#define IN_TouchPhysicalInput(key)      ((void)0)
#define IN_TouchMove(cmd)               ((void)0)
#define IN_TouchDraw()                  ((void)0)
#define IN_TouchReleaseAll()            ((void)0)
#define IN_TouchHapticPulse()           ((void)0)

#endif

#endif
