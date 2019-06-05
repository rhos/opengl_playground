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
    vec2 fromCoord = uv - dt * st * texture(velocity, uv).xy;
    vec4 new = 0.99f * texture(quantity, fromCoord);
    color = new.xyz;
    //if (texture(velocity, uv) == texture(quantity, uv))
    //    color = vec3(texture(velocity, uv).xy,0);
    //else
    //    color = vec3(0);
    //color = texture(quantity, uv).xyz;
    //color = texture(quantity, uv).xyz;
}