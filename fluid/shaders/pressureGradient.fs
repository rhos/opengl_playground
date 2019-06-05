#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D pressure;
uniform sampler2D velocity;

in vec2 uv;
in vec2 uv_l;
in vec2 uv_r;
in vec2 uv_t;
in vec2 uv_b;

void main(){
    float l = texture(pressure, uv_l).x;
    float r = texture(pressure, uv_r).x;
    float t = texture(pressure, uv_t).x;
    float b = texture(pressure, uv_b).x;
    vec2 vel = texture(velocity, uv).xy;
    vel -= vec2(r-l, t-b);
    color = vec3(vel, 0.0);
}