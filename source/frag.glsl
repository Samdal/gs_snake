#version 330 core
precision mediump float;

layout(location = 0) out vec4 diffuseColor;
uniform int u_map[32];
uniform vec2 u_resolution;
uniform vec2 u_food;

void main()
{
    vec2 st = (gl_FragCoord.xy / u_resolution.xy) * 32.0;
    uint x = uint(floor(st.x));
    uint y = uint(floor(st.y));
    diffuseColor.g = float(uint(u_map[y]) & uint(1 << x));
    diffuseColor.r = float(x == uint(u_food.x) && y == uint(u_food.y));
    diffuseColor.b = 0.0;
    diffuseColor.a = 1.0;
}
