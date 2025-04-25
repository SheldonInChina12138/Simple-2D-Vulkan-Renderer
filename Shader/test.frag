#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;  // 【新增】接收顶点位置

layout(location = 0) out vec4 outColor;

void main() {
    // 计算到正方形边界的距离（假设正方形边长为1.0）
    float edgeWidth = 0.05;  // 边缘宽度（可调）
    float distanceToEdge = min(
        min(abs(fragPos.x - 0.5), abs(fragPos.x + 0.5)),  // X方向
        min(abs(fragPos.y - 0.5), abs(fragPos.y + 0.5))   // Y方向
    );
    
    // 如果接近边缘，则混合黑色
    float edgeFactor = smoothstep(0.0, edgeWidth, distanceToEdge);
    vec3 finalColor = mix(vec3(0.0), fragColor, edgeFactor);  // 混合黑色和原色
    
    outColor = vec4(finalColor, 1.0);
}