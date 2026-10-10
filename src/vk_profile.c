/*
Optional Vulkan frame profiler. See vk_profile.h.
*/

#if defined(RENDERER_OPTION_VULKAN) && !defined(EZQUAKE_ANDROID_RELEASE)

#include <vulkan/vulkan.h>
#include "quakedef.h"
#include "vk_local.h"
#include "vk_profile.h"

extern cvar_t vk_profile;

#define VK_PROFILE_REPORT_SECONDS 2.0

static VkQueryPool queryPools[VK_MAX_FRAMES_IN_FLIGHT];
static qbool slotSubmitted[VK_MAX_FRAMES_IN_FLIGHT];
static qbool timestampsUnavailable;

static double timerTotals[VK_PROF_TIMER_COUNT];
static uint64_t counterTotals[VK_PROF_COUNTER_COUNT];
static double gpuTotalMs, gpuMaxMs;
static unsigned int gpuSamples;
static unsigned int frames;
static double frameCpuTotalMs;
static double frameStart;
static double reportStart;

qbool VK_ProfileEnabled(void)
{
	return vk_profile.integer != 0;
}

double VK_ProfileNow(void)
{
	return vk_profile.integer ? Sys_DoubleTime() : 0.0;
}

void VK_ProfileAddTime(vk_prof_timer_t timer, double startTime)
{
	if (vk_profile.integer && timer < VK_PROF_TIMER_COUNT) {
		timerTotals[timer] += (Sys_DoubleTime() - startTime) * 1000.0;
	}
}

void VK_ProfileAddCount(vk_prof_counter_t counter, unsigned int n)
{
	if (vk_profile.integer && counter < VK_PROF_COUNTER_COUNT) {
		counterTotals[counter] += n;
	}
}

static void VK_ProfileDestroyPools(void)
{
	int i;

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		if (queryPools[i] != VK_NULL_HANDLE && vk_options.logicalDevice != VK_NULL_HANDLE) {
			vkDestroyQueryPool(vk_options.logicalDevice, queryPools[i], NULL);
		}
		queryPools[i] = VK_NULL_HANDLE;
		slotSubmitted[i] = false;
	}
}

static qbool VK_ProfileEnsurePools(void)
{
	int i;

	if (timestampsUnavailable) {
		return false;
	}
	if (queryPools[0] != VK_NULL_HANDLE) {
		return true;
	}
	if (vk_options.physicalDeviceProperties.limits.timestampPeriod <= 0.0f) {
		timestampsUnavailable = true;
		return false;
	}

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		VkQueryPoolCreateInfo info;

		VK_InitialiseStructure(info);
		info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		info.queryCount = 2;
		if (vkCreateQueryPool(vk_options.logicalDevice, &info, NULL, &queryPools[i]) != VK_SUCCESS) {
			VK_ProfileDestroyPools();
			timestampsUnavailable = true;
			return false;
		}
	}
	return true;
}

static void VK_ProfileReport(double now)
{
	double span = now - reportStart;

	if (span < VK_PROFILE_REPORT_SECONDS) {
		return;
	}
	if (frames) {
		double n = frames;

		Com_Printf(
			"vkprof: %.1f fps | cpu %.2fms | gpu %.2fms (max %.2f) | wait: fence %.2f acquire %.2f image %.2f | submit %.2f present %.2f | world draws %.0f (static %.0f) idx up %.0f\n",
			frames / span,
			frameCpuTotalMs / n,
			gpuSamples ? gpuTotalMs / gpuSamples : 0.0,
			gpuMaxMs,
			timerTotals[VK_PROF_FENCE_WAIT] / n,
			timerTotals[VK_PROF_ACQUIRE] / n,
			timerTotals[VK_PROF_IMAGE_FENCE] / n,
			timerTotals[VK_PROF_SUBMIT] / n,
			timerTotals[VK_PROF_PRESENT] / n,
			counterTotals[VK_PROF_WORLD_DRAWS] / n,
			counterTotals[VK_PROF_WORLD_STATIC_DRAWS] / n,
			counterTotals[VK_PROF_WORLD_INDEXES_UPLOADED] / n);
	}
	memset(timerTotals, 0, sizeof(timerTotals));
	memset(counterTotals, 0, sizeof(counterTotals));
	gpuTotalMs = gpuMaxMs = 0;
	gpuSamples = frames = 0;
	frameCpuTotalMs = 0;
	reportStart = now;
}

void VK_ProfileFrameBegin(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
	double now;

	if (!vk_profile.integer) {
		return;
	}
	if (frameIndex >= VK_MAX_FRAMES_IN_FLIGHT) {
		return;
	}

	now = Sys_DoubleTime();
	if (reportStart == 0) {
		reportStart = now;
	}
	frameStart = now;

	if (!VK_ProfileEnsurePools()) {
		return;
	}

	// This slot's previous frame has completed (frame fence already waited on).
	if (slotSubmitted[frameIndex]) {
		uint64_t stamps[2];

		slotSubmitted[frameIndex] = false;
		if (vkGetQueryPoolResults(vk_options.logicalDevice, queryPools[frameIndex], 0, 2, sizeof(stamps), stamps, sizeof(stamps[0]), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS && stamps[1] >= stamps[0]) {
			double ms = (double)(stamps[1] - stamps[0]) * vk_options.physicalDeviceProperties.limits.timestampPeriod / 1000000.0;

			gpuTotalMs += ms;
			gpuMaxMs = max(gpuMaxMs, ms);
			++gpuSamples;
		}
	}

	vkCmdResetQueryPool(commandBuffer, queryPools[frameIndex], 0, 2);
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools[frameIndex], 0);
}

void VK_ProfileFrameEnd(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
	if (!vk_profile.integer || frameIndex >= VK_MAX_FRAMES_IN_FLIGHT || queryPools[frameIndex] == VK_NULL_HANDLE) {
		return;
	}
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools[frameIndex], 1);
}

void VK_ProfileFrameSubmitted(uint32_t frameIndex)
{
	double now;

	if (!vk_profile.integer || frameIndex >= VK_MAX_FRAMES_IN_FLIGHT) {
		return;
	}
	if (queryPools[frameIndex] != VK_NULL_HANDLE) {
		slotSubmitted[frameIndex] = true;
	}
	now = Sys_DoubleTime();
	frameCpuTotalMs += (now - frameStart) * 1000.0;
	++frames;
	VK_ProfileReport(now);
}

void VK_ProfileShutdown(void)
{
	VK_ProfileDestroyPools();
	timestampsUnavailable = false;
}

#endif // RENDERER_OPTION_VULKAN
