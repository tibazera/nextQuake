#version 450 core

// Dear ImGui vertex shader with the surface pre-rotation of the Android swapchain
// folded in (uRot, row-major 2x2, identity when the swapchain is not pre-rotated).
// Same inputs/outputs as the stock backends/vulkan shader so the stock fragment
// shader is kept. See ImGui_ImplVulkan_SetPreRotation in src/imgui/backends.
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
	vec2 uScale;
	vec2 uTranslate;
	vec4 uRot;
} pc;

out gl_PerVertex {
	vec4 gl_Position;
};

layout(location = 0) out struct {
	vec4 Color;
	vec2 UV;
} Out;

void main()
{
	vec2 p = aPos * pc.uScale + pc.uTranslate;

	Out.Color = aColor;
	Out.UV = aUV;
	gl_Position = vec4(p.x * pc.uRot.x + p.y * pc.uRot.y, p.x * pc.uRot.z + p.y * pc.uRot.w, 0.0, 1.0);
}
