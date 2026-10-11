/*
Dear ImGui menus on Vulkan: engine-side glue (C). Collects the Vulkan handles for the
backend facade in vk_imgui.cpp and gives the menu bridge its Vulkan texture hooks.
*/

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"
#include "vk_local.h"
#include "vk_imgui_bridge.h"
#include "menu_ui_bridge.h"

extern const unsigned char vk_imgui_vert_spv[];
extern const unsigned int vk_imgui_vert_spv_len;

#ifdef __ANDROID__
extern int VK_AndroidPreRotationDegrees(void);
#endif

static int VK_ImGuiRotation(void)
{
#ifdef __ANDROID__
	return VK_AndroidPreRotationDegrees();
#else
	return 0;
#endif
}

// map preview textures (see VK_ImGui_TextureID): descriptor cache, cleared whenever the backend restarts
#define VK_IMGUI_MAX_TEXTURES 24

typedef struct vk_imgui_texture_s {
	int index;                  // texture_ref.index, 0 = free slot
	VkImageView view;           // the view the descriptor was made from (changes if the texture is reloaded)
	unsigned long long id;      // ImTextureID (a VkDescriptorSet)
} vk_imgui_texture_t;

static vk_imgui_texture_t vk_imgui_textures[VK_IMGUI_MAX_TEXTURES];

// Called once the renderer is up (and again after vid_restart): hands the Vulkan handles to the menus.
void VK_ImGui_VidInit(void)
{
	muivk_init_t info;

	// a fresh backend has none of the old texture descriptors
	memset(vk_imgui_textures, 0, sizeof(vk_imgui_textures));
	memset(&info, 0, sizeof(info));
	info.window = vk_options.window;
	info.apiVersion = VK_API_VERSION_1_0;
	info.instance = vk_options.instance;
	info.physicalDevice = vk_options.physicalDevice;
	info.device = vk_options.logicalDevice;
	info.queueFamily = vk_options.physicalDeviceGraphicsQueueFamilyIndex;
	info.queue = vk_options.graphicsQueue;
	info.pipelineCache = vk_options.pipelineCache;
	info.renderPass = VK_MainRenderPass();
	info.msaaSamples = vk_options.msaaSamples;
	info.minImageCount = 2;
	info.imageCount = max((uint32_t)vk_options.swapChain.imageCount, (uint32_t)(VK_MAX_FRAMES_IN_FLIGHT + 1));
	info.vertexShaderSpv = vk_imgui_vert_spv;
	info.vertexShaderLength = vk_imgui_vert_spv_len;
	info.preRotationDegrees = VK_ImGuiRotation();
	info.swapchainWidth = (float)vk_options.swapChain.imageSize.width;
	info.swapchainHeight = (float)vk_options.swapChain.imageSize.height;
	info.currentCommandBuffer = VK_CurrentCommandBuffer;

	MenuUI_VidInit(&info);
}

// Keeps the UI rotation in step with the swapchain (resize, rotation, render height changes).
void VK_ImGui_SyncSwapchain(void)
{
	MUIVK_SetPreRotation(VK_ImGuiRotation(), (float)vk_options.swapChain.imageSize.width, (float)vk_options.swapChain.imageSize.height);
}

//=============================================================================
// map preview textures: an engine texture shown as an ImGui image
//=============================================================================


unsigned long long VK_ImGui_TextureID(texture_ref texture)
{
	int i, freeSlot = -1;
	VkImageView view;

	if (!MUIVK_BackendReady() || !R_TextureReferenceIsValid(texture) || !VK_TextureReady(texture)) {
		return 0;
	}
	view = VK_TextureImageView(texture);
	if (view == VK_NULL_HANDLE) {
		return 0;
	}

	for (i = 0; i < VK_IMGUI_MAX_TEXTURES; ++i) {
		if (vk_imgui_textures[i].index == (int)texture.index) {
			if (vk_imgui_textures[i].view == view) {
				return vk_imgui_textures[i].id;
			}
			// the texture was reloaded: the old descriptor points at a dead view
			MUIVK_RemoveTexture(vk_imgui_textures[i].id);
			vk_imgui_textures[i].index = 0;
			freeSlot = i;
			break;
		}
		if (freeSlot < 0 && vk_imgui_textures[i].index == 0) {
			freeSlot = i;
		}
	}
	if (freeSlot < 0) {
		return 0;
	}

	vk_imgui_textures[freeSlot].id = MUIVK_AddTexture(view);
	vk_imgui_textures[freeSlot].view = view;
	vk_imgui_textures[freeSlot].index = vk_imgui_textures[freeSlot].id ? (int)texture.index : 0;
	return vk_imgui_textures[freeSlot].id;
}

void VK_ImGui_ReleaseTexture(texture_ref texture)
{
	int i;

	for (i = 0; i < VK_IMGUI_MAX_TEXTURES; ++i) {
		if (vk_imgui_textures[i].index && vk_imgui_textures[i].index == (int)texture.index) {
			MUIVK_RemoveTexture(vk_imgui_textures[i].id);
			memset(&vk_imgui_textures[i], 0, sizeof(vk_imgui_textures[i]));
		}
	}
}

#endif // RENDERER_OPTION_VULKAN
