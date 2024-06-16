char const g_CompositingPS[] = R"(#version 450 core
layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
    mat4x4 g_ViewProjMatrixInv;
    mat4x4 g_LightViewProjMatrix;
    vec4 g_LightPos;
    vec4 g_LightColor;
    vec4 g_AmbientColor;
    float g_rShadowMapSize;
    uint g_EnableIndirectDiffuse;
    uint g_EnableIndirectSpecular;
};

layout(binding = 1) uniform sampler2D t_GBufferGBufferA;
layout(binding = 0) uniform sampler2D t_GBufferGBufferC;
layout(binding = 2) uniform sampler2D t_GBufferDepth;
layout(binding = 3) uniform sampler2D t_IndirectDiffuse;
layout(binding = 4) uniform sampler2D t_IndirectSpecular;
layout(binding = 5) uniform sampler2DShadow t_ShadowMap;

in vec2 v_clipSpacePos;

layout(location = 0) out vec4 f_color;

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

float GetShadow(vec3 worldPos)
{
    vec3 light_direction = normalize(g_LightPos.xyz - worldPos);

    worldPos += light_direction * 1.0f;

    vec4 clipPos = vec4(worldPos, 1.0f) * g_LightViewProjMatrix;

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

    vec4 GBufferA = texelFetch(t_GBufferGBufferA, pixelPos, 0);
    vec4 GBufferC = texelFetch(t_GBufferGBufferC, pixelPos, 0);

    vec3 normal = GBufferA.xyz;
    float roughness = GBufferA.w;
    vec3 base_color = GBufferC.xyz;
    float metallic = GBufferC.w;

    // \[Bhatia 2017\] [Saurabh Bhatia. "glTF 2.0: PBR Materials." GTC 2017.](https://www.khronos.org/assets/uploads/developers/library/2017-gtc/glTF-2.0-and-PBR-GTC_May17.pdf)
    vec3 specular_color_dielectric = vec3(0.04, 0.04, 0.04);
    vec3 specular_color = mix(specular_color_dielectric, base_color, metallic);
    vec3 diffuse_color = base_color - specular_color;

    float z = texelFetch(t_GBufferDepth, pixelPos, 0).x;
    z = z * 2 - 1;
    
    vec4 worldPosV4 = vec4(v_clipSpacePos.xy, z, 1) * g_ViewProjMatrixInv;
    vec3 worldPos = worldPosV4.xyz / worldPosV4.w;

    vec4 indirectDiffuse = bool(g_EnableIndirectDiffuse) ? texelFetch(t_IndirectDiffuse, pixelPos, 0) : vec4(0);
    vec3 indirectSpecular = bool(g_EnableIndirectSpecular) ? texelFetch(t_IndirectSpecular, pixelPos, 0).rgb : vec3(0);

    vec3 radiance = vec3(0.0, 0.0, 0.0);

    vec3 light_direction = normalize(g_LightPos.xyz - worldPos.xyz);
    float NdotL = dot(normal, light_direction);
    if (NdotL > 0.0)
    {
        float shadow = GetShadow(worldPos);
        radiance += diffuse_color * g_LightColor.rgb * shadow * NdotL;
    }

    radiance += diffuse_color * mix(g_AmbientColor.rgb, indirectDiffuse.rgb, indirectDiffuse.a);
    radiance += roughness * indirectSpecular.rgb;

    f_color =  vec4(ConvertToLDR(radiance), 1.0);
})";
