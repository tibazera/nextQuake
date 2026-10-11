/*
Dear ImGui on the Vulkan renderer: SDL3 + Vulkan backends facade (C++ side of vk_imgui_bridge.h).

The ImGui context is owned by menu_imgui.cpp. The UI is recorded into the engine's current
command buffer, inside the main render pass, after the 2D HUD.
*/

#include "vk_imgui_bridge.h"

#include <SDL3/SDL.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

static bool s_ready = false;
static unsigned int* s_vertexSpv = NULL;
static VkCommandBuffer (*s_currentCommandBuffer)(void) = NULL;

static void MUIVK_CheckResult(VkResult result)
{
	if (result != VK_SUCCESS) {
		SDL_Log("imgui vulkan: VkResult %d", (int)result);
	}
}

extern "C" int MUIVK_BackendInit(const muivk_init_t* info)
{
	ImGui_ImplVulkan_InitInfo init;

	if (s_ready) {
		MUIVK_BackendShutdown();
	}
	if (!info || !info->window || !info->vertexShaderSpv || !info->vertexShaderLength || !ImGui::GetCurrentContext()) {
		return 0;
	}

	if (!ImGui_ImplSDL3_InitForVulkan((SDL_Window*)info->window)) {
		return 0;
	}

	// the custom vertex shader must outlive the backend and be 4-byte aligned
	s_vertexSpv = (unsigned int*)malloc(info->vertexShaderLength);
	if (!s_vertexSpv) {
		ImGui_ImplSDL3_Shutdown();
		return 0;
	}
	memcpy(s_vertexSpv, info->vertexShaderSpv, info->vertexShaderLength);

	memset(&init, 0, sizeof(init));
	init.ApiVersion = info->apiVersion;
	init.Instance = info->instance;
	init.PhysicalDevice = info->physicalDevice;
	init.Device = info->device;
	init.QueueFamily = info->queueFamily;
	init.Queue = info->queue;
	init.DescriptorPoolSize = 128;              // font atlas + map previews
	init.MinImageCount = info->minImageCount;
	init.ImageCount = info->imageCount;
	init.PipelineCache = info->pipelineCache;
	init.PipelineInfoMain.RenderPass = info->renderPass;
	init.PipelineInfoMain.Subpass = 0;
	init.PipelineInfoMain.MSAASamples = info->msaaSamples;
	init.CheckVkResultFn = MUIVK_CheckResult;
	init.MinAllocationSize = 1024 * 1024;
	init.CustomShaderVertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	init.CustomShaderVertCreateInfo.codeSize = info->vertexShaderLength;
	init.CustomShaderVertCreateInfo.pCode = s_vertexSpv;

	if (!ImGui_ImplVulkan_Init(&init)) {
		ImGui_ImplSDL3_Shutdown();
		free(s_vertexSpv);
		s_vertexSpv = NULL;
		return 0;
	}

	s_currentCommandBuffer = info->currentCommandBuffer;
	MUIVK_SetPreRotation(info->preRotationDegrees, info->swapchainWidth, info->swapchainHeight);
	s_ready = true;
	return 1;
}

extern "C" void MUIVK_BackendShutdown(void)
{
	if (s_ready) {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		s_ready = false;
	}
	if (s_vertexSpv) {
		free(s_vertexSpv);
		s_vertexSpv = NULL;
	}
	s_currentCommandBuffer = NULL;
}

extern "C" int MUIVK_BackendReady(void)
{
	return s_ready ? 1 : 0;
}

extern "C" void MUIVK_ProcessEvent(const void* sdlEvent)
{
	if (s_ready && sdlEvent) {
		ImGui_ImplSDL3_ProcessEvent((const SDL_Event*)sdlEvent);
	}
}

extern "C" void MUIVK_NewFrame(void)
{
	if (s_ready) {
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
	}
}

extern "C" void MUIVK_RenderDrawData(void)
{
	VkCommandBuffer commandBuffer;

	if (!s_ready || !s_currentCommandBuffer) {
		return;
	}
	commandBuffer = s_currentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE) {
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

extern "C" void MUIVK_SetPreRotation(int degrees, float bufferWidth, float bufferHeight)
{
	ImGui_ImplVulkan_SetPreRotation(degrees, bufferWidth, bufferHeight);
}

extern "C" void MUIVK_TouchPointer(int phase, float x, float y)
{
	ImGuiIO* io;

	if (!s_ready || !ImGui::GetCurrentContext()) {
		return;
	}
	io = &ImGui::GetIO();
	io->AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
	io->AddMousePosEvent(x * io->DisplaySize.x, y * io->DisplaySize.y);
	if (phase == 0) {
		io->AddMouseButtonEvent(0, true);
	}
	else if (phase == 2) {
		io->AddMouseButtonEvent(0, false);
		// no hover once the finger is lifted
		io->AddMousePosEvent(-FLT_MAX, -FLT_MAX);
	}
}

extern "C" unsigned long long MUIVK_AddTexture(VkImageView imageView)
{
	if (!s_ready || imageView == VK_NULL_HANDLE) {
		return 0;
	}
	return (unsigned long long)ImGui_ImplVulkan_AddTexture(imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

extern "C" void MUIVK_RemoveTexture(unsigned long long textureId)
{
	if (s_ready && textureId) {
		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)textureId);
	}
}
