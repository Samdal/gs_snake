#version 330 core
precision mediump float;

layout(location = 0) out vec4 diffuseColor;
uniform int u_map[32];
uniform vec2 u_resolution;
uniform vec2 u_food;

void main()
{
    vec2 st = (gl_FragCoord.xy / u_resolution.xy) * 31.0;
    int x = int(floor(st.x));
    int y = int(floor(st.y));
    diffuseColor.g = float(u_map[y] & (1 << x));
    diffuseColor.r = float(x == int(u_food.x) && y == int(u_food.y));
    diffuseColor.b = 0.0;
    diffuseColor.a = 1.0;
}
