#version 450

// 从 Vertex Shader 输入的颜色（如果启用）
layout(location = 0) in vec3 fragColor;

// 最终输出颜色
layout(location = 0) out vec4 outColor;

// 【关键修改】Uniform Buffer（与 C++ 代码绑定）
layout(binding = 0) uniform UniformBufferObject {
    vec4 color;  // 动态颜色
} ubo;

void main() {
    // 使用 Uniform 颜色（覆盖顶点颜色）
    outColor = ubo.color;
    
    // 如需混合顶点颜色，可改为：
    // outColor = vec4(fragColor * ubo.color.rgb, 1.0);
}