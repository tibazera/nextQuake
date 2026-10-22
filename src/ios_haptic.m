// iOS haptics for the touch controls and menus (in_touch.c)

#import <UIKit/UIKit.h>

void IOS_HapticPulse(void)
{
	static UIImpactFeedbackGenerator *generator;

	if (!generator) {
		generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
	}
	[generator impactOccurred];
}
