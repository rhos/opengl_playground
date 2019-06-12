#version 410 core
precision highp float;
precision highp sampler2D;

layout (location = 0) out vec3 color;

in vec2 uv;
uniform sampler2D velocity;
uniform sampler2D quantity;
uniform float dt;
uniform float dissipation;
uniform vec2 st;

void main(){
    //vec2 fromCoord = uv - dt * texture(velocity, uv).xy * st;
    //color =vec3(dissipation * texture(quantity, fromCoord).xy, 0);
    vec2 coord = uv - dt * texture(velocity, uv).xy * st;
    vec4 newcolor = 1.0f * texture(quantity, coord);
    color = newcolor.xyz;
}
