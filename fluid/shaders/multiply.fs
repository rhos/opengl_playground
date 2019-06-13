#version 410 core

layout (location = 0) out vec3 color;

in vec2 uv;
uniform sampler2D field;
uniform float val;

void main(){
    vec3 initial = texture(field, uv).xyz;
    color = initial * val;
}