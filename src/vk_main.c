/*
Copyright (C) 2018 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// vk_main.c
// - Main entry point for Vulkan

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#ifdef __ANDROID__
#include <jni.h>
#include <SDL_system.h>
#include <android/log.h>
#include <android/native_window.h>
#include <dlfcn.h>
#endif

#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "vk_local.h"
#include "vk_profile.h"

vk_options_t vk_options;
static qbool vk_recreate_swapchain_requested;
static qbool vk_recreate_surface_requested;

#ifdef __ANDROID__
static qbool vk_android_startup_splash_hidden;

static void VK_AndroidHideStartupSplash(void)
{
	JNIEnv* env;
	jobject activity;
	jclass activity_class;
	jmethodID hide_method;

	if (vk_android_startup_splash_hidden) {
		return;
	}

	env = (JNIEnv*) SDL_AndroidGetJNIEnv();
	activity = (jobject) SDL_AndroidGetActivity();
	if (!env || !activity) {
		return;
	}

	activity_class = (*env)->GetObjectClass(env, activity);
	if (!activity_class) {
		(*env)->DeleteLocalRef(env, activity);
		return;
	}

	hide_method = (*env)->GetMethodID(env, activity_class, "hideStartupSplash", "()V");
	if (hide_method) {
		(*env)->CallVoidMethod(env, activity, hide_method);
		vk_android_startup_splash_hidden = true;
	}

	(*env)->DeleteLocalRef(env, activity_class);
	(*env)->DeleteLocalRef(env, activity);
}

// Asks the Java side to size the surface buffer to this height, aspect kept
// (0 = default 1080p envelope). The compositor upscales the smaller buffer for free, so this is
// the cheapest render-scale lever on phones; the swapchain follows through the
// normal OUT_OF_DATE/resize path.
void VK_AndroidSetRenderHeight(int height)
{
	JNIEnv* env = (JNIEnv*) SDL_AndroidGetJNIEnv();
	jobject activity = (jobject) SDL_AndroidGetActivity();
	jclass activity_class;
	jmethodID method;

	if (!env || !activity) {
		return;
	}
	activity_class = (*env)->GetObjectClass(env, activity);
	if (activity_class) {
		method = (*env)->GetMethodID(env, activity_class, "setRenderHeight", "(I)V");
		if (method) {
			(*env)->CallVoidMethod(env, activity, method, (jint)height);
		}
		(*env)->DeleteLocalRef(env, activity_class);
	}
	(*env)->DeleteLocalRef(env, activity);
	VK_RequestSwapChainRecreate();
}

// ANativeWindow_setFrameRate() needs API 30, but minSdk here is 29 -- the
// android-29 stub libandroid.so used at link time doesn't export the symbol
// at all, so it's resolved with dlsym() against the device's real libandroid.so
// at runtime instead of being called directly (which would fail to link).
//
// Without this call, SurfaceFlinger has no declared frame-rate intent from
// this app and falls back to inferring one from observed present timestamps,
// which tends to converge on a conservative cadence well below what the
// display and GPU can actually sustain (observed on a 120Hz-capable panel:
// render stuck around 65-85fps with the GPU and CPU both >95% idle, plus a
// stream of BLASTBufferQueue "NO_BUFFER_AVAILABLE" log spam consistent with
// the compositor pacing buffer hand-back to that inferred rate rather than
// our actual submission rate). Declaring a high target up front asks the
// compositor to size its buffer-queue pacing for that rate instead of
// guessing.
static void VK_AndroidRequestHighFrameRate(SDL_Window* window)
{
	typedef int32_t (*SetFrameRateFn)(ANativeWindow*, float, int8_t);
	static SetFrameRateFn setFrameRate;
	static qbool resolved;
	ANativeWindow* nativeWindow;
	int32_t result;

	if (!resolved) {
		void* lib = dlopen("libandroid.so", RTLD_NOW);
		resolved = true;
		if (lib) {
			setFrameRate = (SetFrameRateFn)dlsym(lib, "ANativeWindow_setFrameRate");
		}
	}
	if (!setFrameRate) {
		__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setFrameRate not available (dlsym failed)");
		return;
	}

	nativeWindow = (ANativeWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
	if (!nativeWindow) {
		__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setFrameRate skipped: no native window pointer yet");
		return;
	}

	// Aspirational: higher than any current Android panel's refresh rate, so
	// we're always asking for "as fast as the display/compositor allow" rather
	// than accidentally requesting less than the hardware supports.
	result = setFrameRate(nativeWindow, 240.0f, ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT);
	__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setFrameRate(240) result=%d", result);
}

// Defined in vk_swapchain.c -- returns the same 0/90/180/270 used to build the
// compensating rotation matrix in r_matrix.c.
extern int VK_AndroidPreRotationDegrees(void);

// Vulkan's VkSwapchainCreateInfoKHR::preTransform tells the WSI loader/driver
// that our content is pre-rotated, but on several Android devices/drivers
// (confirmed needed on this one) that alone does not get SurfaceFlinger's
// hardware composer to actually apply the corresponding rotation during
// scanout -- the buffer gets presented as-is, un-rotated, even though
// everything upstream (matrix math, swapchain extent) is correct. The
// ANativeWindow itself needs the same transform set via this separate NDK
// call so the window's BufferQueue/HWC layer carries the hint through to the
// actual display path, not just the Vulkan/SurfaceFlinger composition path
// (which is what `adb shell screencap` reads, and which looked correct even
// while the live panel did not -- that discrepancy is what pointed here).
static void VK_AndroidSetBuffersTransform(SDL_Window* window)
{
	typedef int32_t (*SetBuffersTransformFn)(ANativeWindow*, int32_t);
	static SetBuffersTransformFn setBuffersTransform;
	static qbool resolved;
	ANativeWindow* nativeWindow;
	int32_t transform;
	int32_t result;

	if (!resolved) {
		void* lib = dlopen("libandroid.so", RTLD_NOW);
		resolved = true;
		if (lib) {
			setBuffersTransform = (SetBuffersTransformFn)dlsym(lib, "ANativeWindow_setBuffersTransform");
		}
	}
	if (!setBuffersTransform) {
		__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setBuffersTransform not available (dlsym failed)");
		return;
	}

	nativeWindow = (ANativeWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
	if (!nativeWindow) {
		__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setBuffersTransform skipped: no native window pointer yet");
		return;
	}

	switch (VK_AndroidPreRotationDegrees()) {
		case 90: transform = ANATIVEWINDOW_TRANSFORM_ROTATE_90; break;
		case 180: transform = ANATIVEWINDOW_TRANSFORM_ROTATE_180; break;
		case 270: transform = ANATIVEWINDOW_TRANSFORM_ROTATE_270; break;
		default: transform = 0; break;
	}

	result = setBuffersTransform(nativeWindow, transform);
	__android_log_print(ANDROID_LOG_INFO, "VK_Android", "ANativeWindow_setBuffersTransform(%d) result=%d", transform, result);
}
#endif

void VK_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags);
void VK_DrawRectangle(float x, float y, float width, float height, byte* color);
void VK_AdjustImages(int first, int last, float x_offset, float y_offset);
void VK_PolyBlend(float v_blend[4]);
void VK_BrightenScreen(void);
void VK_TextureLoadCubemapFace(texture_ref cubemap, r_cubemap_direction_id direction, const byte* data, int width, int height);
void VK_CreateLightmapTextures(void);
void VK_UploadLightmap(int textureUnit, int lightmapnum);
void VK_BuildLightmap(int lightmapnum);
void VK_InvalidateLightmapTextures(void);
void VK_LightmapFrameInit(void);
void VK_LightmapShutdown(void);
void VK_RenderDynamicLightmaps(msurface_t* surface, qbool world);
void VK_DrawWaterSurfaces(void);
void VK_DeleteVAOs(void);
void VK_GenVertexArray(r_vao_id vao, const char* name);
void VK_BindVertexArray(r_vao_id vao);
void VK_BindVertexArrayElementBuffer(r_vao_id vao, r_buffer_id ref);
qbool VK_VertexArrayCreated(r_vao_id vao);
qbool VK_InitialiseVAOHandling(void);

static void VK_NoOperation(void)
{
}

static void VK_NoOperationCvar(cvar_t* cvar)
{
	(void)cvar;
}

static void VK_NoOperationBool(qbool value)
{
	(void)value;
}

static void VK_NoOperationState(r_state_id state)
{
	R_ApplyRenderingState(state);
}

static void VK_NoOperationEntity(entity_t* ent)
{
	(void)ent;
}

static void VK_NoOperationViewport(int x, int y, int width, int height)
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

static int VK_BrushModelCopyVertToBuffer(model_t* mod, void* vbo_buffer_, int position, float* source, int lightmap, int material, float scaleS, float scaleT, msurface_t* surf, qbool has_fb_texture, qbool has_luma_texture)
{
	(void)has_fb_texture;
	(void)has_luma_texture;
	{
		vbo_world_vert_t* target = (vbo_world_vert_t*)vbo_buffer_ + position;
		byte rgba[4];

		VectorCopy(source, target->position);
		target->material_coords[0] = source[3] * (scaleS ? scaleS : 1);
		target->material_coords[1] = source[4] * (scaleT ? scaleT : 1);
		target->material_coords[2] = material;
		target->lightmap_coords[0] = source[5];
		target->lightmap_coords[1] = source[6];
		target->lightmap_coords[2] = lightmap;
		target->detail_coords[0] = source[7];
		target->detail_coords[1] = source[8];

		if (surf->flags & SURF_DRAWSKY) {
			target->flags = TEXTURE_TURB_SKY;
		}
		else if (surf->flags & SURF_DRAWTURB) {
			target->flags = (surf->texinfo->texture->turbType & EZQ_SURFACE_TYPE);
			if (!target->flags) {
				target->flags = TEXTURE_TURB_OTHER;
			}
		}
		else if (mod->isworldmodel) {
			target->flags = EZQ_SURFACE_WORLD;
			target->flags += (surf->flags & SURF_DRAWFLAT_FLOOR ? EZQ_SURFACE_IS_FLOOR : 0);
			target->flags += (surf->flags & SURF_UNDERWATER ? EZQ_SURFACE_UNDERWATER : 0);
		}
		else {
			target->flags = 0;
		}
		target->flags += (surf->flags & SURF_DRAWALPHA ? EZQ_SURFACE_ALPHATEST : 0);

		COLOR_TO_RGBA(surf->texinfo->texture->flatcolor3ub, rgba);
		VectorScale(rgba, 1 / 255.0f, target->flatcolor);
		target->surface_num = mod->isworldmodel ? surf - mod->surfaces : 0;
	}

	return position + 1;
}

static qbool VK_False(void)
{
	return false;
}

static qbool VK_FalseFramebuffer(framebuffer_id id, int width, int height)
{
	(void)id;
	(void)width;
	(void)height;
	return false;
}

static const char* VK_DescriptiveString(void)
{
	if (vk_options.physicalDevice != VK_NULL_HANDLE && vk_options.physicalDeviceProperties.deviceName[0]) {
		return vk_options.physicalDeviceProperties.deviceName;
	}
	return "Vulkan";
}

static void VK_Screenshot(byte* buffer, size_t size)
{
	VkCommandBuffer cmd;
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	VkImage srcImage;
	VkImageMemoryBarrier barrier;
	VkBufferImageCopy region;
	void* mapped;
	uint32_t width, height;
	qbool swizzleBGRA;

	memset(buffer, 0, size);

	if (vk_options.logicalDevice == VK_NULL_HANDLE || !vk_options.swapChain.images || vk_options.swapChain.imageCount <= 0) {
		return;
	}

	width = vk_options.swapChain.imageSize.width;
	height = vk_options.swapChain.imageSize.height;
	if (!width || !height || size < (size_t)width * height * 3 || vk_options.frame.imageIndex >= (uint32_t)vk_options.swapChain.imageCount) {
		return;
	}

	// Screenshots are rare/not perf-sensitive, so a full vkDeviceWaitIdle to make
	// sure the swapchain image we're about to read has actually finished
	// presenting is fine here -- unlike the per-frame path, this has no business
	// being clever about synchronization.
	vkDeviceWaitIdle(vk_options.logicalDevice);

	srcImage = vk_options.swapChain.images[vk_options.frame.imageIndex];

	if (!VK_CreateBufferResource((VkDeviceSize)width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
		return;
	}

	cmd = VK_BeginImmediateCommands();
	if (cmd == VK_NULL_HANDLE) {
		vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
		vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
		return;
	}

	VK_InitialiseStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = srcImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VK_InitialiseStructure(region);
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VK_EndImmediateCommands(cmd);

	swizzleBGRA = (vk_options.physicalDeviceSurfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM ||
		vk_options.physicalDeviceSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB);

	if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, (VkDeviceSize)width * height * 4, 0, &mapped) == VK_SUCCESS) {
		uint32_t x, y;

		// Vulkan images are stored top-down; callers of renderer.Screenshot()
		// (SCR_ScreenshotWrite, Image_WriteJPEG with a negative stride, ...)
		// expect the GL convention instead: row 0 is the bottom of the image.
		for (y = 0; y < height; ++y) {
			const byte* srcRow = (const byte*)mapped + (size_t)y * width * 4;
			byte* dstRow = buffer + (size_t)(height - 1 - y) * width * 3;

			for (x = 0; x < width; ++x) {
				const byte* px = srcRow + (size_t)x * 4;
				byte* out = dstRow + (size_t)x * 3;

				if (swizzleBGRA) {
					out[0] = px[2];
					out[1] = px[1];
					out[2] = px[0];
				}
				else {
					out[0] = px[0];
					out[1] = px[1];
					out[2] = px[2];
				}
			}
		}
		vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
	}

	vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
	vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
}

static size_t VK_ScreenshotWidth(void)
{
	return vk_options.swapChain.imageSize.width ? vk_options.swapChain.imageSize.width : glConfig.vidWidth;
}

static size_t VK_ScreenshotHeight(void)
{
	return vk_options.swapChain.imageSize.height ? vk_options.swapChain.imageSize.height : glConfig.vidHeight;
}

static void VK_ClearRenderingSurface(qbool clear_color)
{
	(void)clear_color;
}

static void VK_DestroyFrameResources(void)
{
	uint32_t i;

	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vk_options.frame.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk_options.logicalDevice, vk_options.frame.imageAvailableSemaphores[i], NULL);
		}
		if (vk_options.frame.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk_options.logicalDevice, vk_options.frame.renderFinishedSemaphores[i], NULL);
		}
		if (vk_options.frame.inFlightFences[i] != VK_NULL_HANDLE) {
			vkDestroyFence(vk_options.logicalDevice, vk_options.frame.inFlightFences[i], NULL);
		}
	}
	if (vk_options.frame.commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vk_options.logicalDevice, vk_options.frame.commandPool, NULL);
	}
	Q_free(vk_options.frame.commandBuffers);
	Q_free(vk_options.frame.imageInFlightFences);
	memset(&vk_options.frame, 0, sizeof(vk_options.frame));
}

static qbool VK_CreateFrameResources(void)
{
	VkCommandPoolCreateInfo poolInfo = { 0 };
	VkCommandBufferAllocateInfo allocInfo = { 0 };
	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	VkFenceCreateInfo fenceInfo = { 0 };
	uint32_t i;

	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = VK_PhysicalDeviceGraphicsQueueFamilyIndex();
	if (vkCreateCommandPool(vk_options.logicalDevice, &poolInfo, NULL, &vk_options.frame.commandPool) != VK_SUCCESS) {
		return false;
	}

	vk_options.frame.commandBuffers = Q_malloc(vk_options.swapChain.imageCount * sizeof(vk_options.frame.commandBuffers[0]));
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk_options.frame.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = vk_options.swapChain.imageCount;
	if (vkAllocateCommandBuffers(vk_options.logicalDevice, &allocInfo, vk_options.frame.commandBuffers) != VK_SUCCESS) {
		VK_DestroyFrameResources();
		return false;
	}

	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(vk_options.logicalDevice, &semaphoreInfo, NULL, &vk_options.frame.imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(vk_options.logicalDevice, &semaphoreInfo, NULL, &vk_options.frame.renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(vk_options.logicalDevice, &fenceInfo, NULL, &vk_options.frame.inFlightFences[i]) != VK_SUCCESS) {
			VK_DestroyFrameResources();
			return false;
		}
	}
	vk_options.frame.imageInFlightFences = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.frame.imageInFlightFences[0]));

	return true;
}

static qbool VK_RecreateSwapChain(void)
{
	if (vk_options.window == NULL || vk_options.logicalDevice == VK_NULL_HANDLE || vk_options.surface == VK_NULL_HANDLE) {
		return false;
	}

	vkDeviceWaitIdle(vk_options.logicalDevice);

	VK_DestroyFrameResources();
	VK_DestroySwapChain();

	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_options.physicalDevice, vk_options.surface, &vk_options.physicalDeviceSurfaceCapabilities) != VK_SUCCESS) {
		return false;
	}

	if (!VK_CreateSwapChain(vk_options.window, vk_options.instance, vk_options.surface)) {
		return false;
	}
#ifdef __ANDROID__
	VK_AndroidSetBuffersTransform(vk_options.window);
#endif
	if (!VK_CreateSwapChainFramebuffers()) {
		return false;
	}
	if (!VK_CreateFrameResources()) {
		return false;
	}

	VK_HudSwapchainChanged();
	return true;
}

static qbool VK_RecreateSurfaceAndSwapChain(void)
{
	if (vk_options.window == NULL || vk_options.instance == VK_NULL_HANDLE || vk_options.logicalDevice == VK_NULL_HANDLE) {
		return false;
	}

	vkDeviceWaitIdle(vk_options.logicalDevice);

	VK_DestroyFrameResources();
	VK_DestroySwapChain();
	VK_DestroyWindowSurface(vk_options.instance, vk_options.surface);
	vk_options.surface = VK_NULL_HANDLE;

	if (!VK_CreateWindowSurface(vk_options.window, vk_options.instance, &vk_options.surface)) {
		return false;
	}
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_options.physicalDevice, vk_options.surface, &vk_options.physicalDeviceSurfaceCapabilities) != VK_SUCCESS) {
		return false;
	}
	if (!VK_CreateSwapChain(vk_options.window, vk_options.instance, vk_options.surface)) {
		return false;
	}
#ifdef __ANDROID__
	VK_AndroidSetBuffersTransform(vk_options.window);
#endif
	if (!VK_CreateSwapChainFramebuffers()) {
		return false;
	}
	if (!VK_CreateFrameResources()) {
		return false;
	}

	VK_HudSwapchainChanged();
	return true;
}

void VK_RequestSwapChainRecreate(void)
{
	vk_recreate_swapchain_requested = true;
}

void VK_RequestSurfaceRecreate(void)
{
	vk_recreate_surface_requested = true;
	vk_recreate_swapchain_requested = true;
}

void VK_BeginFrame(void)
{
	VkResult result;
	VkCommandBufferBeginInfo beginInfo = { 0 };
	VkRenderPassBeginInfo renderPassInfo = { 0 };
	VkClearValue clearValues[2] = { 0 };
	VkCommandBuffer commandBuffer;
	uint32_t frameIndex;
	VkFence frameFence;
	VkResult waitResult;

	if (vk_options.logicalDevice == VK_NULL_HANDLE || vk_options.frame.active) {
		return;
	}
	if (vk_recreate_surface_requested) {
		// Clear the request before attempting recreation, not after: if this
		// fails, retrying it unconditionally on every subsequent frame turns
		// a single transient failure (e.g. surface capabilities glitching
		// during a display state change) into an infinite recreate loop that
		// never reaches vkQueueSubmit/Present again -- the screen freezes
		// forever instead of getting another real chance via the normal
		// VK_ERROR_OUT_OF_DATE_KHR path next time it actually occurs.
		vk_recreate_surface_requested = false;
		vk_recreate_swapchain_requested = false;
		if (!VK_RecreateSurfaceAndSwapChain()) {
			return;
		}
	}
	else if (vk_recreate_swapchain_requested) {
		vk_recreate_swapchain_requested = false;
		if (!VK_RecreateSwapChain()) {
			return;
		}
	}
	if (vk_options.swapChain.handle == VK_NULL_HANDLE) {
		return;
	}

	frameIndex = vk_options.frame.currentFrame;
	frameFence = vk_options.frame.inFlightFences[frameIndex];
	{
		double profStart = VK_ProfileNow();

		waitResult = vkWaitForFences(vk_options.logicalDevice, 1, &frameFence, VK_TRUE, UINT64_MAX);
		VK_ProfileAddTime(VK_PROF_FENCE_WAIT, profStart);
	}
	if (waitResult != VK_SUCCESS) {
		Sys_Error("vulkan: frame fence wait failed: %d", waitResult);
	}

	// Finite timeout, not UINT64_MAX: with non-blocking present modes
	// (IMMEDIATE/MAILBOX) the CPU can render faster than the compositor
	// drains the swapchain's buffer queue for long enough to exhaust every
	// image. When that happens the compositor stops handing back buffers
	// (observed as BLASTBufferQueue "NO_BUFFER_AVAILABLE" in logcat) and an
	// infinite-timeout wait here never returns -- the app freezes forever
	// with no way to recover. A finite wait turns that into a recoverable
	// "no frame this loop", same idea as the OUT_OF_DATE/SUBOPTIMAL handling
	// below.
	{
		double profStart = VK_ProfileNow();

		result = vkAcquireNextImageKHR(vk_options.logicalDevice, vk_options.swapChain.handle, 1000000000ULL, vk_options.frame.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &vk_options.frame.imageIndex);
		VK_ProfileAddTime(VK_PROF_ACQUIRE, profStart);
	}
	if (result == VK_TIMEOUT || result == VK_NOT_READY) {
		return;
	}
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		VK_RequestSwapChainRecreate();
		return;
	}
	if (result == VK_ERROR_SURFACE_LOST_KHR) {
		VK_RequestSurfaceRecreate();
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		Sys_Error("vulkan: vkAcquireNextImageKHR failed: %d", result);
	}
	if (vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex] != VK_NULL_HANDLE) {
		{
			double profStart = VK_ProfileNow();

			waitResult = vkWaitForFences(vk_options.logicalDevice, 1, &vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex], VK_TRUE, UINT64_MAX);
			VK_ProfileAddTime(VK_PROF_IMAGE_FENCE, profStart);
		}
		if (waitResult != VK_SUCCESS) {
			Sys_Error("vulkan: swapchain image fence wait failed: %d", waitResult);
		}
	}
	vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex] = frameFence;

	// Anti-lag / low-latency input marker: as close to the start of the
	// frame's CPU work as we can get it, before any of the (potentially
	// expensive) command-buffer recording below. vid_vulkan_antilag is
	// re-checked every frame rather than latched -- toggling it only
	// gates whether these calls happen, nothing about pipeline/swapchain
	// state depends on it, so no vid_restart is needed.
	{
		extern cvar_t vid_vulkan_antilag;

#if EZQ_HAS_AMD_ANTI_LAG
		if (vid_vulkan_antilag.integer && vk_options.supportsAmdAntiLag) {
			VkAntiLagPresentationInfoAMD presentationInfo = { 0 };
			VkAntiLagDataAMD antiLagData = { 0 };

			presentationInfo.sType = VK_STRUCTURE_TYPE_ANTI_LAG_PRESENTATION_INFO_AMD;
			presentationInfo.stage = VK_ANTI_LAG_STAGE_INPUT_AMD;
			presentationInfo.frameIndex = vk_options.antiLagFrameIndex;

			antiLagData.sType = VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD;
			antiLagData.mode = VK_ANTI_LAG_MODE_ON_AMD;
			antiLagData.pPresentationInfo = &presentationInfo;

			vk_options.antiLagUpdateAMD(vk_options.logicalDevice, &antiLagData);
		}
		else if (vid_vulkan_antilag.integer && vk_options.supportsNvLowLatency2) {
#else
		if (vid_vulkan_antilag.integer && vk_options.supportsNvLowLatency2) {
#endif
			VkLatencySleepModeInfoNV sleepModeInfo = { 0 };
			VkLatencySleepInfoNV sleepInfo = { 0 };
			VkSetLatencyMarkerInfoNV markerInfo = { 0 };

			if (!vk_options.latencySleepModeSet) {
				sleepModeInfo.sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV;
				sleepModeInfo.lowLatencyMode = VK_TRUE;
				sleepModeInfo.lowLatencyBoost = VK_TRUE;
				vk_options.setLatencySleepModeNV(vk_options.logicalDevice, vk_options.swapChain.handle, &sleepModeInfo);
				vk_options.latencySleepModeSet = true;
			}

			markerInfo.sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
			markerInfo.presentID = vk_options.antiLagFrameIndex;
			markerInfo.marker = VK_LATENCY_MARKER_INPUT_SAMPLE_NV;
			vk_options.setLatencyMarkerNV(vk_options.logicalDevice, vk_options.swapChain.handle, &markerInfo);

			markerInfo.marker = VK_LATENCY_MARKER_SIMULATION_START_NV;
			vk_options.setLatencyMarkerNV(vk_options.logicalDevice, vk_options.swapChain.handle, &markerInfo);

			// vkLatencySleepNV does not block by itself -- it schedules the
			// driver to signal latencySleepSemaphore once it's time to let
			// the CPU proceed, so we wait on that semaphore right after.
			sleepInfo.sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV;
			sleepInfo.signalSemaphore = vk_options.latencySleepSemaphore;
			sleepInfo.value = vk_options.antiLagFrameIndex + 1;
			if (vk_options.latencySleepNV(vk_options.logicalDevice, vk_options.swapChain.handle, &sleepInfo) == VK_SUCCESS) {
				VkSemaphoreWaitInfo waitInfo = { 0 };
				VkSemaphore waitSemaphore = vk_options.latencySleepSemaphore;
				uint64_t waitValue = vk_options.antiLagFrameIndex + 1;

				waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
				waitInfo.semaphoreCount = 1;
				waitInfo.pSemaphores = &waitSemaphore;
				waitInfo.pValues = &waitValue;
				vk_options.waitSemaphores(vk_options.logicalDevice, &waitInfo, 1000000000ULL);
			}

			markerInfo.marker = VK_LATENCY_MARKER_SIMULATION_END_NV;
			vk_options.setLatencyMarkerNV(vk_options.logicalDevice, vk_options.swapChain.handle, &markerInfo);
		}
	}

	{
		commandBuffer = vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
		result = vkResetCommandBuffer(commandBuffer, 0);
		if (result != VK_SUCCESS) {
			Sys_Error("vulkan: vkResetCommandBuffer failed: %d", result);
		}

		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		if (result != VK_SUCCESS) {
			Sys_Error("vulkan: vkBeginCommandBuffer failed: %d", result);
		}
	}

	// Dynamic lightmaps queued by the previous frame are copied before the
	// render pass. This keeps transfer work on the frame submission and avoids
	// the queue-wide stalls caused by immediate command buffers.
	VK_ProfileFrameBegin(commandBuffer, frameIndex);
	VK_TextureFlushPendingUploads(commandBuffer, frameIndex);

	clearValues[0].color.float32[0] = vk_options.clearColor[0];
	clearValues[0].color.float32[1] = vk_options.clearColor[1];
	clearValues[0].color.float32[2] = vk_options.clearColor[2];
	clearValues[0].color.float32[3] = vk_options.clearColor[3] ? vk_options.clearColor[3] : 1.0f;
	clearValues[1].depthStencil.depth = glConfig.reversed_depth ? 0.0f : 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	// Real gamma/contrast/FXAA (VK_PostProcessActive) route the main pass into
	// the offscreen target instead of the swapchain image directly; the
	// composite pass that reads it back and writes the swapchain image runs
	// at the end of VK_EndFrame, after this render pass ends.
	vk_options.swapChain.postProcessActive = VK_PostProcessActive();

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VK_MainRenderPass();
	renderPassInfo.framebuffer = vk_options.swapChain.postProcessActive ?
		VK_PostProcessFramebuffer(vk_options.frame.imageIndex) : vk_options.swapChain.framebuffers[vk_options.frame.imageIndex];
	if (renderPassInfo.framebuffer == VK_NULL_HANDLE) {
		vk_options.swapChain.postProcessActive = false;
		renderPassInfo.framebuffer = vk_options.swapChain.framebuffers[vk_options.frame.imageIndex];
	}
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent = vk_options.swapChain.imageSize;
	renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(clearValues[0]);
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vk_options.frame.active = true;
}

VkCommandBuffer VK_CurrentCommandBuffer(void)
{
	if (!vk_options.frame.active || !vk_options.frame.commandBuffers) {
		return VK_NULL_HANDLE;
	}
	return vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
}

void VK_EndFrame(void)
{
	extern cvar_t vid_vulkan_antilag;
	VkCommandBuffer commandBuffer;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = { 0 };
	VkPresentInfoKHR presentInfo = { 0 };
	VkResult result;
	qbool presented = false;
	uint32_t frameIndex;
	VkFence frameFence;

	if (!vk_options.frame.active) {
		return;
	}
	frameIndex = vk_options.frame.currentFrame;
	frameFence = vk_options.frame.inFlightFences[frameIndex];

	commandBuffer = vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
	vkCmdEndRenderPass(commandBuffer);

	if (vk_options.swapChain.postProcessActive) {
		VkFramebuffer compositeFramebuffer = VK_PostProcessCompositeFramebuffer(vk_options.frame.imageIndex);

		if (compositeFramebuffer != VK_NULL_HANDLE) {
			VkRenderPassBeginInfo compositePassInfo = { 0 };

			VK_PostProcessTransitionForSampling(commandBuffer, vk_options.frame.imageIndex);

			compositePassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			compositePassInfo.renderPass = VK_PostProcessRenderPass();
			compositePassInfo.framebuffer = compositeFramebuffer;
			compositePassInfo.renderArea.offset.x = 0;
			compositePassInfo.renderArea.offset.y = 0;
			compositePassInfo.renderArea.extent = vk_options.swapChain.imageSize;

			vkCmdBeginRenderPass(commandBuffer, &compositePassInfo, VK_SUBPASS_CONTENTS_INLINE);
			VK_PostProcessComposite(commandBuffer, vk_options.frame.imageIndex);
			vkCmdEndRenderPass(commandBuffer);
		}
	}

	VK_ProfileFrameEnd(commandBuffer, frameIndex);
	{
		result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS) {
			Sys_Error("vulkan: vkEndCommandBuffer failed: %d", result);
		}
	}
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &vk_options.frame.imageAvailableSemaphores[frameIndex];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &vk_options.frame.renderFinishedSemaphores[frameIndex];

	result = vkResetFences(vk_options.logicalDevice, 1, &frameFence);
	if (result != VK_SUCCESS) {
		Sys_Error("vulkan: vkResetFences failed: %d", result);
	}
	{
		double profStart = VK_ProfileNow();

		result = vkQueueSubmit(vk_options.graphicsQueue, 1, &submitInfo, frameFence);
		VK_ProfileAddTime(VK_PROF_SUBMIT, profStart);
		if (result != VK_SUCCESS) {
			Sys_Error("vulkan: vkQueueSubmit failed: %d", result);
		}
		VK_ProfileFrameSubmitted(frameIndex);
	}

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk_options.frame.renderFinishedSemaphores[frameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk_options.swapChain.handle;
	presentInfo.pImageIndices = &vk_options.frame.imageIndex;

	// Present-stage anti-lag marker, immediately before the real present
	// call below -- mirrors the input-stage marker placed near the start
	// of VK_BeginFrame, correlated by the same antiLagFrameIndex.
	{
#if EZQ_HAS_AMD_ANTI_LAG
		if (vid_vulkan_antilag.integer && vk_options.supportsAmdAntiLag) {
			VkAntiLagPresentationInfoAMD presentationInfo = { 0 };
			VkAntiLagDataAMD antiLagData = { 0 };

			presentationInfo.sType = VK_STRUCTURE_TYPE_ANTI_LAG_PRESENTATION_INFO_AMD;
			presentationInfo.stage = VK_ANTI_LAG_STAGE_PRESENT_AMD;
			presentationInfo.frameIndex = vk_options.antiLagFrameIndex;

			antiLagData.sType = VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD;
			antiLagData.mode = VK_ANTI_LAG_MODE_ON_AMD;
			antiLagData.pPresentationInfo = &presentationInfo;

			vk_options.antiLagUpdateAMD(vk_options.logicalDevice, &antiLagData);
		}
		else if (vid_vulkan_antilag.integer && vk_options.supportsNvLowLatency2) {
#else
		if (vid_vulkan_antilag.integer && vk_options.supportsNvLowLatency2) {
#endif
			VkSetLatencyMarkerInfoNV markerInfo = { 0 };

			markerInfo.sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
			markerInfo.presentID = vk_options.antiLagFrameIndex;
			markerInfo.marker = VK_LATENCY_MARKER_PRESENT_START_NV;
			vk_options.setLatencyMarkerNV(vk_options.logicalDevice, vk_options.swapChain.handle, &markerInfo);
		}

		if (vid_vulkan_antilag.integer && (vk_options.supportsAmdAntiLag || vk_options.supportsNvLowLatency2)) {
			++vk_options.antiLagFrameIndex;
		}
	}

	{
		double profStart = VK_ProfileNow();

		result = vkQueuePresentKHR(vk_options.presentQueue, &presentInfo);
		VK_ProfileAddTime(VK_PROF_PRESENT, profStart);
	}
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Only a real error requires recreating the swapchain. SUBOPTIMAL is
		// just advisory -- the image was still presented fine, it just isn't
		// an exact match for the surface's "ideal" current extent. We
		// deliberately render at a smaller-than-display buffer size on
		// Android (see SDLSurface's render-scaling cap), so SUBOPTIMAL is the
		// expected steady state, not a transient condition: treating it as a
		// recreate trigger meant the swapchain (and a vkDeviceWaitIdle) was
		// being torn down and rebuilt on every single frame.
		VK_RequestSwapChainRecreate();
	}
	else if (result == VK_ERROR_SURFACE_LOST_KHR) {
		VK_RequestSurfaceRecreate();
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		Con_DPrintf("vulkan: vkQueuePresentKHR failed: %d\n", result);
	}
	else {
		presented = true;
	}

	if (vid_vulkan_antilag.integer && vk_options.supportsNvLowLatency2) {
		VkSetLatencyMarkerInfoNV markerInfo = { 0 };

		markerInfo.sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
		markerInfo.presentID = vk_options.antiLagFrameIndex;
		markerInfo.marker = VK_LATENCY_MARKER_PRESENT_END_NV;
		vk_options.setLatencyMarkerNV(vk_options.logicalDevice, vk_options.swapChain.handle, &markerInfo);
	}
#ifdef __ANDROID__
	if (presented) {
		VK_AndroidHideStartupSplash();
	}
#endif

	vk_options.frame.active = false;
	vk_options.frame.currentFrame = (frameIndex + 1) % VK_MAX_FRAMES_IN_FLIGHT;
}

// GLC_TimeRefresh/GLM_TimeRefresh spin the view through 360 degrees over 128
// R_RenderView() calls inside a single already-open GL context and measure
// wall time around that loop, relying on a final EnsureFinished (glFinish-
// equivalent) to flush. Vulkan has no equivalent mid-frame flush -- draw
// commands here only become real GPU work once a frame is submitted via
// VK_EndFrame -- so each iteration has to be a full R_BeginRendering/
// R_RenderView/R_EndRendering cycle (begin command buffer, draw, submit,
// present), same as a normal render frame minus 2D/HUD. The closing
// vkDeviceWaitIdle (not just EnsureFinished, which is a no-op here -- see
// the VK_EnsureFinished define) ensures the timing includes the last
// submitted frame's actual GPU completion, not just CPU-side queuing.
void VK_TimeRefresh(void)
{
	extern void R_SetupFrame(void);
	int i;
	int x, y, width, height;
	float start, stop, time;

	start = Sys_DoubleTime();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);

		R_BeginRendering(&x, &y, &width, &height);
		R_SetupFrame();
		R_RenderView();
		R_EndRendering();
	}

	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vk_options.logicalDevice);
	}
	stop = Sys_DoubleTime();
	time = stop - start;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);
}

qbool VK_Initialise(SDL_Window* window)
{
	memset(&vk_options, 0, sizeof(vk_options));
	vk_options.window = window;
	vk_options.clearColor[3] = 1.0f;

	if (!VK_CreateInstance(window, &vk_options.instance)) {
		return false;
	}

	if (!VK_CreateWindowSurface(window, vk_options.instance, &vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

#ifdef __ANDROID__
	VK_AndroidRequestHighFrameRate(window);
#endif

	if (!VK_SelectPhysicalDevice(vk_options.instance, vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	VK_DetermineMSAASampleCount();

	if (!VK_CreateLogicalDevice(vk_options.instance)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateSwapChain(window, vk_options.instance, vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}
#ifdef __ANDROID__
	VK_AndroidSetBuffersTransform(window);
#endif

	if (!VK_RenderPassCreate()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateSwapChainFramebuffers()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateFrameResources()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	Con_Printf("Vulkan initialised successfully\n");
	return true;
}

// vid_restart can run mid-frame: the console command is processed from
// Cbuf_Execute() inside the same Host_Frame() that already called
// VK_BeginFrame() for this frame (world/HUD draw calls recorded commands
// into it), with VK_EndFrame() not reached yet because the restart itself
// is what's running right now. That leaves the frame's command buffer in
// the "recording" state, still referencing this frame's descriptor
// sets/pipeline, when VK_Shutdown() goes on to destroy exactly those
// resources. Nothing was ever submitted to the GPU, so vkDeviceWaitIdle
// has nothing to wait for and doesn't protect this -- validation layers
// (VK_LAYER_KHRONOS_validation) confirmed this exact sequence on desktop:
// repeated "commandBuffer must be in the recording state" / "VkDescriptorSet
// was destroyed" errors immediately after a vid_restart, then an invalid
// VkPipeline handle on the next vkCmdBindPipeline. Ending the command
// buffer (without submitting it -- there's nothing valid left to present)
// before any of that teardown starts takes it out of the recording state
// cleanly, so the pool/device/instance destruction below has nothing left
// referencing torn-down objects.
void VK_AbandonActiveFrame(void)
{
	if (vk_options.frame.active && vk_options.frame.commandBuffers) {
		VkCommandBuffer commandBuffer = vk_options.frame.commandBuffers[vk_options.frame.imageIndex];

		vkEndCommandBuffer(commandBuffer);
		vk_options.frame.active = false;
	}
}

void VK_Shutdown(r_shutdown_mode_t mode)
{
	VK_AbandonActiveFrame();
	if (mode != r_shutdown_reload) {
#if EZQ_HAS_AMD_ANTI_LAG
		// VK_AMD_anti_lag requires a final OFF update before the device is
		// idled/destroyed -- otherwise the driver is left waiting on the next
		// half of an input/present marker pair that will never arrive, which
		// was observed to hang vkDeviceWaitIdle below into an AMD driver TDR
		// (timeout detection/recovery) on vid_restart while antilag was on.
		if (vk_options.supportsAmdAntiLag && vk_options.logicalDevice != VK_NULL_HANDLE) {
			VkAntiLagDataAMD antiLagData = { 0 };

			antiLagData.sType = VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD;
			antiLagData.mode = VK_ANTI_LAG_MODE_OFF_AMD;
			vk_options.antiLagUpdateAMD(vk_options.logicalDevice, &antiLagData);
		}
#endif

		if (vk_options.logicalDevice != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(vk_options.logicalDevice);
		}

		VK_ProfileShutdown();
		VK_HudResourcesShutdown();
		VK_WorldResourcesShutdown();
		VK_AliasModelResourcesShutdown();
		VK_Sprite3DResourcesShutdown();
		VK_TextureShutdown();
		VK_DestroyImmediateCommandPool();
		VK_DestroyFrameResources();

		VK_DestroySwapChain();

		VK_RenderPassDelete();

		VK_SavePipelineCache();
		if (vk_options.pipelineCache != VK_NULL_HANDLE) {
			vkDestroyPipelineCache(vk_options.logicalDevice, vk_options.pipelineCache, NULL);
			vk_options.pipelineCache = VK_NULL_HANDLE;
		}

		if (vk_options.latencySleepSemaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk_options.logicalDevice, vk_options.latencySleepSemaphore, NULL);
			vk_options.latencySleepSemaphore = VK_NULL_HANDLE;
		}

		if (vk_options.logicalDevice != VK_NULL_HANDLE) {
			vkDestroyDevice(vk_options.logicalDevice, NULL);
			vk_options.logicalDevice = VK_NULL_HANDLE;
		}

		if (vk_options.instance) {
			VK_DestroyWindowSurface(vk_options.instance, vk_options.surface);
			VK_ShutdownDebugCallback(vk_options.instance);
			vkDestroyInstance(vk_options.instance, NULL);
		}

		memset(&vk_options, 0, sizeof(vk_options));
	}

	// FIXME
}

void VK_PopulateConfig(void)
{
	const VkPhysicalDeviceLimits* limits = &vk_options.physicalDeviceProperties.limits;

	memset(&renderer, 0, sizeof(renderer));

	glConfig.renderer_string = (const unsigned char*)vk_options.physicalDeviceProperties.deviceName;
	glConfig.vendor_string = (const unsigned char*)"Vulkan";
	glConfig.version_string = (const unsigned char*)"Vulkan 1.0";
	glConfig.glsl_version = (const unsigned char*)"SPIR-V";
	glConfig.majorVersion = VK_VERSION_MAJOR(vk_options.physicalDeviceProperties.apiVersion);
	glConfig.minorVersion = VK_VERSION_MINOR(vk_options.physicalDeviceProperties.apiVersion);
	glConfig.texture_units = 16;
	glConfig.gl_max_size_default = limits->maxImageDimension2D ? (int)limits->maxImageDimension2D : 4096;
	glConfig.max_texture_depth = limits->maxImageArrayLayers ? (int)limits->maxImageArrayLayers : 1;
	glConfig.max_3d_texture_size = limits->maxImageDimension3D ? (int)limits->maxImageDimension3D : 1;
	glConfig.uniformBufferOffsetAlignment = (int)limits->minUniformBufferOffsetAlignment;
	glConfig.shaderStorageBufferOffsetAlignment = (int)limits->minStorageBufferOffsetAlignment;
	glConfig.supported_features =
		R_SUPPORT_RENDERING_SHADERS |
		R_SUPPORT_PRIMITIVERESTART |
		R_SUPPORT_MULTITEXTURING |
		R_SUPPORT_TEXTURE_SAMPLERS |
		R_SUPPORT_CUBE_MAPS;

#define VK_CvarForceRecompile             VK_NoOperationCvar
#define VK_PrintGfxInfo                   VK_PrintGfxInfo
#define VK_DescriptiveString              VK_DescriptiveString
#define VK_Viewport                       VK_NoOperationViewport
#define VK_InvalidateViewport             VK_NoOperation
#define VK_ApplyRenderingState            VK_NoOperationState
#define VK_PrepareModelRendering          VK_PrepareModelRendering
#define VK_PrepareAliasModel              GL_PrepareAliasModel
#define VK_DrawSky                        VK_NoOperation
#define VK_DrawWorld                      VK_DrawWorld
#define VK_DrawAliasFrame                 VK_DrawAliasFrame
#define VK_DrawAliasModelShadow           VK_DrawAliasModelShadow
#define VK_DrawAliasModelPowerupShell     VK_NoOperationEntity
#define VK_DrawAlias3ModelPowerupShell    VK_NoOperationEntity
#define VK_DrawSpriteModel                VK_DrawSpriteModel
#define VK_DrawSimpleItem                 VK_DrawSimpleItem
#define VK_DrawClassicParticles           VK_DrawClassicParticles
#define VK_DrawDisc                       VK_NoOperation
#define VK_LightmapFrameInit              VK_LightmapFrameInit
#define VK_RenderDynamicLightmaps         VK_RenderDynamicLightmaps
#define VK_InvalidateLightmapTextures     VK_InvalidateLightmapTextures
#define VK_LightmapShutdown               VK_LightmapShutdown
#define VK_SetupGL                        VK_NoOperation
#define VK_ChainBrushModelSurfaces        VK_ChainBrushModelSurfaces
#define VK_DrawBrushModel                 VK_DrawBrushModel
#define VK_BrushModelCopyVertToBuffer     VK_BrushModelCopyVertToBuffer
#define VK_ClearRenderingSurface          VK_ClearRenderingSurface
#define VK_DrawWaterSurfaces              VK_DrawWaterSurfaces
#define VK_ScreenDrawStart                VK_NoOperation
#define VK_EnsureFinished                 VK_NoOperation
#define VK_Begin2DRendering               VK_NoOperation
#define VK_IsFramebufferEnabled3D         VK_False
#define VK_RenderView                     VK_RenderView
#define VK_PreRenderView                  VK_PreRenderView
#define VK_PostProcessScreen              VK_BrightenScreen
#define VK_BrightenScreen                 VK_BrightenScreen
#define VK_PolyBlend                      VK_PolyBlend
#define VK_TimeRefresh                    VK_TimeRefresh
#define VK_Screenshot                     VK_Screenshot
#define VK_ScreenshotWidth                VK_ScreenshotWidth
#define VK_ScreenshotHeight               VK_ScreenshotHeight
#define VK_Prepare3DSprites               VK_Prepare3DSprites
#define VK_Draw3DSprites                  VK_Draw3DSprites
#define VK_Draw3DSpritesInline            VK_NoOperation
#define VK_RenderFramebuffers             VK_NoOperation
#define VK_FramebufferCreate              VK_FalseFramebuffer
#define VK_ProgramsInitialise             VK_NoOperation
#define VK_ProgramsShutdown               VK_NoOperationBool

#define RENDERER_METHOD(returntype, name, ...) renderer.name = VK_ ## name;
#include "r_renderer_structure.h"
#undef RENDERER_METHOD

	renderer.vaos_supported = VK_InitialiseVAOHandling();
}

#endif
