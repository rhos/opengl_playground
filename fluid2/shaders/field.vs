#version 410 core

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;

uniform vec2 st;

out vec2 uv;
out vec2 uv_l;
out vec2 uv_r;
out vec2 uv_t;
out vec2 uv_b;

void main(){
    uv = in_uv;
    uv_l = uv - vec2(st.x, 0.0);
    uv_r = uv + vec2(st.x, 0.0);
    uv_t = uv + vec2(0.0, st.y);
    uv_b = uv - vec2(0.0, st.y);
    gl_Position = vec4(in_position,0.0,1.0);
}

