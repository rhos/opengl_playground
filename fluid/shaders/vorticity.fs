#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D velocity;

in vec2 uv;
in vec2 uv_l;
in vec2 uv_r;
in vec2 uv_t;
in vec2 uv_b;

void main(){
    float l = texture(velocity, uv_l).y;
    float r = texture(velocity, uv_r).y;
    float t = texture(velocity, uv_t).x;
    float b = texture(velocity, uv_b).x;
    float vort = r - l - t + b;
    color = vec3(vort * 0.5, 0.0, 0.0);
}