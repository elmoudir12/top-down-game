#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D spriteTexture;

void main() {
    vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    vec4 tex = texture(spriteTexture, uv);
    if (tex.a < 0.01) discard;
    outColor = tex;
}
