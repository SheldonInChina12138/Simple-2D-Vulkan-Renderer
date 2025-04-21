#version 450

// 【关键修改】定义 Uniform Buffer 绑定（与 C++ 代码一致）
layout(binding = 0) uniform UniformBufferObject {
    vec4 color;  // 颜色数据（虽然 Fragment Shader 使用，但为了结构一致可放在这里）
    mat4 model;  // 如需实例化渲染（INSTANCE_COUNT），可在此添加变换矩阵
} ubo;

// 顶点输入（如果后续需要顶点数据，可在此扩展）
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// 输出到 Fragment Shader
layout(location = 0) out vec3 fragColor;

void main() {
    // 硬编码三角形顶点（适配你的当前代码）
    vec2 positions[3] = vec2[](
        vec2(0.0, -0.5),
        vec2(0.5, 0.5),
        vec2(-0.5, 0.5)
    );
    
    // 直接输出顶点位置（后续可加上 ubo.model 变换）
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    
    // 传递颜色（如果启用顶点颜色）
    fragColor = inColor;  // 或用 ubo.color.rgb 忽略顶点颜色
}