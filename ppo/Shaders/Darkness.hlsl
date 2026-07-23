#include "LightingUtil.hlsl" 

static const int MaxFogUnits = 64;

struct DarknessUnit
{
    float4 CenterPosRadius;
};

cbuffer cbDarkness : register(b1)
{
    DarknessUnit gUnits[MaxFogUnits];
    int gUnitCount;
    float3 gPad;
    float2 gMapSize;
    float2 gMapPad;
    float4 gDarkColor;
    float4 gGlowColor;
};

// PassCB에서 gInvViewProj만 필요
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    // 나머지는 안 써도 됨
};

// 샘플러 (Common.hlsl 없이 직접 선언)
SamplerState gsamPointClamp : register(s1);

Texture2D gDepthTex : register(t0, space2);

struct VertexIn
{
    float2 Pos : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 NDC : TEXCOORD0;
    float2 TexC : TEXCOORD1;
};

VertexOut VS(VertexIn vin)
{
    VertexOut v;
    v.PosH = float4(vin.Pos, 0.0f, 1.0f);
    v.NDC = vin.Pos;
    v.TexC.x = vin.Pos.x * 0.5f + 0.5f;
    v.TexC.y = -vin.Pos.y * 0.5f + 0.5f;
    return v;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float depth = gDepthTex.Sample(gsamPointClamp, pin.TexC).r;

    if (depth >= 1.0f - 1e-4f)
        return float4(0.0f, 0.0f, 0.0f, gDarkColor.a);

    float4 posH = float4(pin.NDC.x, pin.NDC.y, depth * 2.0f - 1.0f, 1.0f);
    float4 posW = mul(posH, gInvViewProj);
    posW /= posW.w;
    float3 pixelPos = posW.xyz;

    float darkness = 1.0f;

    [loop]
    for (int i = 0; i < gUnitCount; ++i)
    {
        float3 uPos = gUnits[i].CenterPosRadius.xyz;
        float rad = gUnits[i].CenterPosRadius.w;

        float dist = distance(pixelPos.xz, uPos.xz); // XZ 평면 거리
        float inner = rad;
        float outer = rad * 1.2f;
        float a = saturate((dist - inner) / max(outer - inner, 0.001f));
        darkness = min(darkness, a);
    }

    float alpha = gDarkColor.a * darkness;
    return float4(gDarkColor.rgb, alpha);
}