/*
Optional Vulkan frame profiler (cvar vk_profile 1). Does nothing, and costs
a single cvar read per call site, while disabled.
Android Release builds compile it out entirely.

Prints one aggregated line every ~2 seconds: fps, GPU time (timestamps),
CPU time split into fence wait / acquire / image fence / submit / present,
and per-frame world draw / index counters.
*/

#ifndef EZQUAKE_VK_PROFILE_H
#define EZQUAKE_VK_PROFILE_H

#ifdef RENDERER_OPTION_VULKAN

typedef enum {
	VK_PROF_FENCE_WAIT,
	VK_PROF_ACQUIRE,
	VK_PROF_IMAGE_FENCE,
	VK_PROF_SUBMIT,
	VK_PROF_PRESENT,
	VK_PROF_TIMER_COUNT
} vk_prof_timer_t;

typedef enum {
	VK_PROF_WORLD_DRAWS,
	VK_PROF_WORLD_INDEXES_UPLOADED,
	VK_PROF_WORLD_STATIC_DRAWS,
	VK_PROF_COUNTER_COUNT
} vk_prof_counter_t;

#ifdef EZQUAKE_ANDROID_RELEASE
static inline qbool VK_ProfileEnabled(void) { return false; }
static inline double VK_ProfileNow(void) { return 0.0; }
static inline void VK_ProfileAddTime(vk_prof_timer_t timer, double startTime) { (void)timer; (void)startTime; }
static inline void VK_ProfileAddCount(vk_prof_counter_t counter, unsigned int n) { (void)counter; (void)n; }
static inline void VK_ProfileFrameBegin(VkCommandBuffer commandBuffer, uint32_t frameIndex) { (void)commandBuffer; (void)frameIndex; }
static inline void VK_ProfileFrameEnd(VkCommandBuffer commandBuffer, uint32_t frameIndex) { (void)commandBuffer; (void)frameIndex; }
static inline void VK_ProfileFrameSubmitted(uint32_t frameIndex) { (void)frameIndex; }
static inline void VK_ProfileShutdown(void) {}
#else
qbool VK_ProfileEnabled(void);
double VK_ProfileNow(void);
void VK_ProfileAddTime(vk_prof_timer_t timer, double startTime);
void VK_ProfileAddCount(vk_prof_counter_t counter, unsigned int n);
void VK_ProfileFrameBegin(VkCommandBuffer commandBuffer, uint32_t frameIndex);
void VK_ProfileFrameEnd(VkCommandBuffer commandBuffer, uint32_t frameIndex);
void VK_ProfileFrameSubmitted(uint32_t frameIndex);
void VK_ProfileShutdown(void);
#endif

#endif

#endif
