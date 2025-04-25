#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inOffset;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragPos;  // 【新增】传递顶点位置到片段着色器

void main() {
    gl_Position = vec4(inPos + inOffset, 0.0, 1.0);
    fragColor = inColor;
    fragPos = inPos;  // 【新增】传递原始顶点坐标（未偏移）
}