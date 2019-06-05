#version 410 core

layout (location = 0) out vec3 color;

in vec2 uv;

uniform sampler2D renderedTexture;

void main(){
    color = texture(renderedTexture, uv).xyz;
}