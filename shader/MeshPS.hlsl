#include "ShaderUtil.hlsli"

SamplerState g_AnisotropicClamp : register(s0);
Texture2D g_Normal : register(t0);
Texture2D g_Albedo : register(t1);

float4 main(VertexOut i) : SV_TARGET
{
    const float4 sample = g_Normal.Sample(g_AnisotropicClamp, i.TexCoord);

    const float3 albedo = g_Albedo.Sample(g_AnisotropicClamp, i.TexCoord).rgb;
    float3 norm = sample.rbg;
    norm = normalize(norm * 2.0f - 1.0f);

    float3 col = Shade(norm, g_SunDir, g_SunIntensity, sample.a, AMBIENT_INTENSITY)
        * albedo /** LoadColor(g_PatchColor).rgb*/;
    //col = GammaCorrect(col);
    return float4(col, 1.0f);
}
