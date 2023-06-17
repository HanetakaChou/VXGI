char const g_FullScreenQuadVS[] = R"(#version 450 core
layout(row_major) uniform;

// in int gl_VertexID;

out gl_PerVertex
{
    vec4 gl_Position;
};

out vec2 v_clipSpacePos;

void main()
{
    uint u = ~gl_VertexID & 1;
    uint v = (gl_VertexID >> 1) & 1;
    gl_Position = vec4(vec2(u,v) * 2 - 1, 0, 1);
    gl_Position.y = -gl_Position.y;
    v_clipSpacePos = gl_Position.xy;
})";
