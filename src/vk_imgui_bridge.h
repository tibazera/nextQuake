/*
Dear ImGui <-> Vulkan renderer bridge (plain C interface).

vk_imgui.cpp (C++) implements the ImGui SDL3 + Vulkan backends part; menu_imgui.cpp (C++) owns the
ImGui context and the menus; vk_imgui_glue.c (C) fills in the Vulkan handles from the engine.
The engine's C headers are not C++-safe, so everything crosses as plain data.
*/

#ifndef VK_IMGUI_BRIDGE_H
#define VK_IMGUI_BRIDGE_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct muivk_init_s {
	void* window;                           // SDL_Window*
	uint32_t apiVersion;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	uint32_t queueFamily;
	VkQueue queue;
	VkPipelineCache pipelineCache;
	VkRenderPass renderPass;                // main render pass (the UI is drawn inside it)
	VkSampleCountFlagBits msaaSamples;
	uint32_t minImageCount;
	uint32_t imageCount;
	const unsigned char* vertexShaderSpv;   // custom vertex shader with the pre-rotation
	unsigned int vertexShaderLength;
	int preRotationDegrees;                 // 0/90/180/270
	float swapchainWidth;                   // real (pre-rotated) swapchain image size
	float swapchainHeight;
	VkCommandBuffer (*currentCommandBuffer)(void);   // the frame's command buffer, NULL outside a frame
} muivk_init_t;

// The ImGui context must already exist when MUIVK_BackendInit is called.
int  MUIVK_BackendInit(const muivk_init_t* info);   // nonzero on success
void MUIVK_BackendShutdown(void);
int  MUIVK_BackendReady(void);
void MUIVK_ProcessEvent(const void* sdlEvent);      // const SDL_Event*
void MUIVK_NewFrame(void);                          // Vulkan + SDL3 backend NewFrame
void MUIVK_RenderDrawData(void);                    // records ImGui::GetDrawData() into the current command buffer
void MUIVK_SetPreRotation(int degrees, float bufferWidth, float bufferHeight);
// A finger acting as the mouse pointer: phase 0 down, 1 motion, 2 up; x/y normalised 0..1 over the window.
void MUIVK_TouchPointer(int phase, float x, float y);
// vk_imgui_glue.c: re-read the swapchain size/rotation (called every frame the menus draw).
void VK_ImGui_SyncSwapchain(void);
// menu_imgui.cpp: a menu text field has focus, so the on-screen keyboard should be up.
int  MenuUI_WantsTextInput(void);

// User textures (map previews): returns an ImTextureID (a VkDescriptorSet), 0 on failure.
unsigned long long MUIVK_AddTexture(VkImageView imageView);
void MUIVK_RemoveTexture(unsigned long long textureId);

#ifdef __cplusplus
}
#endif

#endif
