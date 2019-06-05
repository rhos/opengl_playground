#version 410 core

layout (location = 0) out vec3 color;

in vec2 uv;
uniform sampler2D field;
uniform float val;

void main(){
    color = texture(field, uv).xyz * val;
}