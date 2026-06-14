#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform SpriteUBO {
    mat4 projection;
} ubo;

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec2 scale;
} push;

void main() {
    vec2 worldPos = inPosition * push.scale + push.position;
    gl_Position = ubo.projection * vec4(worldPos, 0.0, 1.0);
    fragTexCoord = inTexCoord;
}
