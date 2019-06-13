#version 410 core

layout (location = 0) out vec3 color;

uniform sampler2D velocity;
uniform sampler2D vorticity;
uniform float dxscale;
uniform float dt;

in vec2 uv;
in vec2 uv_l;
in vec2 uv_r;
in vec2 uv_t;
in vec2 uv_b;

void main(){
    float l = texture(vorticity, uv_l).x;
    float r = texture(vorticity, uv_r).x;
    float t = texture(vorticity, uv_t).x;
    float b = texture(vorticity, uv_b).x;
    float c = texture(vorticity, uv).x;
    // vec2 force = 0.5 * vec2(abs(t) - abs(b), abs(r) - abs(l));
    // float lengthRSqr = 1.0 / max(2.4414e-4, dot(force,force));
    // force *= lengthRSqr * dxscale * vec2(1,-1);
    // vec2 vel = texture(velocity, uv).xy;
    // color = vec3(vel + force * dt, 0.0);

    vec2 force = 0.5 * vec2(abs(t) - abs(b), abs(r) - abs(l));
    force /= length(force) + 0.0001;
    force *= dxscale * c;
    force.y *= -1.0;
    vec2 vel = texture(velocity, uv).xy;
    color = vec3(vel + force * dt, 0.0);
}