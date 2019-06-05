#version 410 core

layout (location = 0) out vec3 color;

in vec2 uv;
// uniform sampler2D velocity;
// uniform sampler2D quantity;
// uniform float dt;
// uniform float dissipation;
// uniform vec2 st;

void main(){
    //vec2 fromCoord = uv - dt * texture(velocity, uv).xy * st;
    //color = texture(quantity, fromCoord).xyz * dissipation;
    color = vec3(0.0,uv);
}