#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D divergence;
uniform sampler2D pressure;
uniform sampler2D border;

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
    float c = texture(pressure, uv).x;

    if (uv_l.x < 0.0) { l = c; }
    if (uv_r.x > 1.0) { r = c; }
    if (uv_t.y > 1.0) { t = c; }
    if (uv_b.y < 0.0) { b = c; }

    
    float diver = texture(divergence, uv).x;
    float newPressure = (l + r + b + t - diver) * 0.25;
    color = vec3(newPressure, 0.0, 0.0);
}