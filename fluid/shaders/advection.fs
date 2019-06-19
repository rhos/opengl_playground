#version 410 core
layout (location = 0) out vec3 color;

in vec2 uv;
uniform sampler2D velocity;
uniform sampler2D quantity;
uniform float dt;
uniform float dissipation;
uniform vec2 st;

void main(){
    vec2 vel = texture(velocity, uv).xy;
    vec2 fromCoord = uv - dt * vel * st;

    vec3 quan = texture(quantity, fromCoord).xyz;
    color = vec3(dissipation * quan);
}
