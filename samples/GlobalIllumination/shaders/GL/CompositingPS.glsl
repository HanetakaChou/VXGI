char const g_CompositingPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
    mat4x4 g_ViewProjMatrixInv;
    mat4x4 g_LightViewProjMatrix;
    vec4 g_LightDirection;
    vec4 g_DiffuseColor;
    vec4 g_LightColor;
    vec4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

layout(binding = 0) uniform sampler2D t_GBufferAlbedo;
layout(binding = 1) uniform sampler2D t_GBufferNormal;
layout(binding = 2) uniform sampler2D t_GBufferDepth;
layout(binding = 3) uniform sampler2D t_IndirectDiffuse;
layout(binding = 4) uniform sampler2D t_IndirectSpecular;
layout(binding = 5) uniform sampler2DShadow t_ShadowMap;

in vec4 gl_FragCoord;
in vec2 v_clipSpacePos;
out vec4 f_color;

const vec2 g_SamplePositions[] = {
    // Poisson disk with 16 points
    vec2(-0.3935238f, 0.7530643f),
    vec2(-0.3022015f, 0.297664f),
    vec2(0.09813362f, 0.192451f),
    vec2(-0.7593753f, 0.518795f),
    vec2(0.2293134f, 0.7607011f),
    vec2(0.6505286f, 0.6297367f),
    vec2(0.5322764f, 0.2350069f),
    vec2(0.8581018f, -0.01624052f),
    vec2(-0.6928226f, 0.07119545f),
    vec2(-0.3114384f, -0.3017288f),
    vec2(0.2837671f, -0.179743f),
    vec2(-0.3093514f, -0.749256f),
    vec2(-0.7386893f, -0.5215692f),
    vec2(0.3988827f, -0.617012f),
    vec2(0.8114883f, -0.458026f),
    vec2(0.08265103f, -0.8939569f)
};

float GetShadow(vec3 fragmentPos)
{
    fragmentPos -= g_LightDirection.xyz * 1.0f;

    vec4 clipPos = vec4(fragmentPos, 1.0f) * g_LightViewProjMatrix;

    if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || abs(clipPos.z) > clipPos.w)
    {
        return 0;
    }

    clipPos.xyz /= clipPos.w;
    clipPos.xyz = clipPos.xyz * 0.5 + 0.5;
   // clipPos.z *= 0.9999;

    float shadow = 0;
    float totalWeight = 0;

    for(int nSample = 0; nSample < 16; ++nSample)
    {
        vec2 offset = g_SamplePositions[nSample];
        float weight = 1.0;
        offset *= 2 * g_rShadowMapSize;
        float shadowSample = texture(t_ShadowMap, vec3(clipPos.xy + offset, clipPos.z));
        shadow += shadowSample * weight;
        totalWeight += weight;
    }

    shadow /= totalWeight;
    shadow = pow(shadow, 2.2);

    return shadow;
}

float Luminance(vec3 color)
{
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
}

vec3 ConvertToLDR(vec3 color)
{
    float srcLuminance = Luminance(color);

    float sqrWhiteLuminance = 50;
    float scaledLuminance = srcLuminance * 8;
    float mappedLuminance = (scaledLuminance * (1 + scaledLuminance / sqrWhiteLuminance)) / (1 + scaledLuminance);

    return color * (mappedLuminance / srcLuminance);
}

void main()
{
    ivec2 pixelPos = ivec2(gl_FragCoord.xy);

    vec4 albedo = texelFetch(t_GBufferAlbedo, pixelPos, 0);
    vec3 normal = texelFetch(t_GBufferNormal, pixelPos, 0).xyz;
    vec4 indirectDiffuse = bool(g_EnableIndirectDiffuse) ? texelFetch(t_IndirectDiffuse, pixelPos, 0) : vec4(0);
    vec3 indirectSpecular = bool(g_EnableIndirectSpecular) ? texelFetch(t_IndirectSpecular, pixelPos, 0).rgb : vec3(0);
    float z = texelFetch(t_GBufferDepth, pixelPos, 0).x;
    z = z * 2 - 1;
    
    vec4 worldPos = vec4(v_clipSpacePos.xy, z, 1) * g_ViewProjMatrixInv;
    worldPos.xyz /= worldPos.w;

    float shadow = GetShadow(worldPos.xyz);
    float NdotL = clamp(-dot(normal.xyz, g_LightDirection.xyz), 0.0, 1.0);

    vec3 result = albedo.rgb * (g_LightColor.rgb * (shadow * NdotL) + mix(g_AmbientColor.rgb, indirectDiffuse.rgb, indirectDiffuse.a)) 
                  + albedo.a * indirectSpecular.rgb;

    result = ConvertToLDR(result);

    f_color = vec4(result, 1);
})";
