#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D quantity;
uniform float aspect;
uniform vec2 position;
uniform vec3 dir;
uniform float radius;

in vec2 uv;

void main(){
    vec2 p = uv - position;
    p.x *= aspect;
    vec3 splat = exp(-dot(p, p) / radius) * dir;
    vec3 base = texture(quantity, uv).xyz;
    color = base + splat;
}