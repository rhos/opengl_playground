#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D velocity;
uniform sampler2D border;

uniform vec2 st;

in vec2 uv;
in vec2 uv_l;
in vec2 uv_r;
in vec2 uv_t;
in vec2 uv_b;

void main(){
    float l = texture(velocity, uv_l).x;
    float r = texture(velocity, uv_r).x;
    float t = texture(velocity, uv_t).y;
    float b = texture(velocity, uv_b).y;
    vec2 c = texture(velocity, uv).xy;
    //boundary
    if (uv_l.x < 0.0) { l = -c.x; }
    if (uv_r.x > 1.0) { r = -c.x; }
    if (uv_t.y > 1.0) { t = -c.y; }
    if (uv_b.y < 0.0) { b = -c.y; }

    color = vec3((r - l + t - b) * 0.5,0.0, 0.0);
    //color = vec3(t-b);
    //color = vec3(uv_t-uv_b, 0);
    //color = vec3(st*1000,0);
    //color = vec3(uv,0);
}