#ifndef SHADER_UTIL
#define SHADER_UTIL

static const uint PATCH_SIZE = 255;
static const float LIGHT_INTENSITY = 0.6f;
static const float AMBIENT_INTENSITY = 0.1f;
static const float HEIGHTMAP_SCALE = 2000.0f;

cbuffer PassConstants : register(b0)
{
    float4x4 g_ViewProjection;
	float3 g_SunDir;
    float g_SunIntensity;
	int2 g_CameraXy;
	int g_Pad0[2];
}

cbuffer ObjectConstants : register(b1)
{
	uint2 g_PatchXy;
	uint g_PatchColor;
    int g_Pad1[1];
}

struct VertexIn
{
    float2 PositionL : SV_Position;
};

struct VertexOut
{
    float4 PositionH : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 LoadColor(uint col)
{
    float b = (col & 0x000000FF) / 255.0f;
    float g = ((col & 0x0000FF00) >> 8) / 255.0f;
    float r = ((col & 0x00FF0000) >> 16) / 255.0f;
    float a = ((col & 0xFF000000) >> 24) / 255.0f;
    return float4(r, g, b, a);
}

float3 EvalSh(float3 _dir)
{
#define k01 0.2820947918 // sqrt( 1/PI)/2
#define k02 0.4886025119 // sqrt( 3/PI)/2
#define k03 1.0925484306 // sqrt(15/PI)/2
#define k04 0.3153915652 // sqrt( 5/PI)/4
#define k05 0.5462742153 // sqrt(15/PI)/4

    float3 shEnv[9];
    shEnv[0] = float3(0.967757057878229854, 0.976516067990363390, 0.891218272348969998); /* Band 0 */
    shEnv[1] = float3(-0.384163503608655643, -0.423492289131209787, -0.425532726148547868); /* Band 1 */
    shEnv[2] = float3(0.055906294587354334, 0.056627436881069373, 0.069969936396987467);
    shEnv[3] = float3(0.120985157386215209, 0.119297994074027414, 0.117111965829213599);
    shEnv[4] = float3(-0.176711633774331106, -0.170331404095516392, -0.151345020570876621); /* Band 2 */
    shEnv[5] = float3(-0.124682114349692147, -0.119340785411183953, -0.096300354204368860);
    shEnv[6] = float3(0.001852378550138503, -0.032592784164597745, -0.088204495001329680);
    shEnv[7] = float3(0.296365482782109446, 0.281268696656263029, 0.243328223888495510);
    shEnv[8] = float3(-0.079826665303240341, -0.109340956251195970, -0.157208859664677764);

    float3 nn = _dir.zxy;

    float sh[9];
    sh[0] = k01;
    sh[1] = -k02 * nn.y;
    sh[2] = k02 * nn.z;
    sh[3] = -k02 * nn.x;
    sh[4] = k03 * nn.y * nn.x;
    sh[5] = -k03 * nn.y * nn.z;
    sh[6] = k04 * (3.0 * nn.z * nn.z - 1.0);
    sh[7] = -k03 * nn.x * nn.z;
    sh[8] = k05 * (nn.x * nn.x - nn.y * nn.y);

    float3 rgb = 0.0;
    rgb += shEnv[0] * sh[0] * 1.0;
    rgb += shEnv[1] * sh[1] * 2.0 / 2.5;
    rgb += shEnv[2] * sh[2] * 2.0 / 2.5;
    rgb += shEnv[3] * sh[3] * 2.0 / 2.5;
    rgb += shEnv[4] * sh[4] * 1.0 / 2.5;
    rgb += shEnv[5] * sh[5] * 0.5;
    rgb += shEnv[6] * sh[6] * 0.5;
    rgb += shEnv[7] * sh[7] * 0.5;
    rgb += shEnv[8] * sh[8] * 0.5;

    return rgb;
}

float3 Shade(const float3 normal, const float3 lightDir, const float lightIntensity, const float occlusion, const float
             ambientIntensity)
{
	return saturate(dot(normal, lightDir) * lightIntensity + occlusion * ambientIntensity);
}

float3 GammaCorrect(const float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

#endif
