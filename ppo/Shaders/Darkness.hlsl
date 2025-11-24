// Darkness.hlsl : 월드 좌표 + 깊이버퍼 기반 시야 마스크

#include "Common.hlsl"

static const int MaxFogUnits = 64;

struct DarknessUnit
{
    // x,y,z = 유닛 월드 위치, w = 시야 반경(월드 단위)
    float4 CenterPosRadius;
};

cbuffer cbDarkness : register(b1)
{
    DarknessUnit gUnits[MaxFogUnits];
    int gUnitCount;
    float3 gPad;

    float4 gDarkColor; // 기본 어둠 색 + 알파
    float4 gGlowColor; // 필요하면 가장자리 발광 색으로 사용 가능
}

// 깊이버퍼: t0, space2
Texture2D gDepthTex : register(t0, space2);

struct VertexIn
{
    float2 Pos : POSITION; // -1 ~ +1 (clip space) 풀스크린 삼각형/쿼드
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

    // vin.Pos 는 이미 -1~+1 범위의 클립 좌표라고 가정
    v.PosH = float4(vin.Pos, 0.0f, 1.0f);
    v.NDC = vin.Pos;

    // NDC(-1~1) -> 텍스처 좌표(0~1)
    v.TexC.x = vin.Pos.x * 0.5f + 0.5f;
    v.TexC.y = -vin.Pos.y * 0.5f + 0.5f;

    return v;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // 1) 깊이버퍼 샘플
    float depth = gDepthTex.Sample(gsamPointClamp, pin.TexC).r;

    // 깊이 == 1.0 이면 카메라가 아무것도 안 찍은 곳(하늘/배경) → 그냥 어둡게
    if (depth >= 1.0f - 1e-4f)
    {
        return float4(0.0f, 0.0f, 0.0f, gDarkColor.a);
    }

    // 2) 깊이(NDC z) 복원
    float z_ndc = depth * 2.0f - 1.0f;
    float2 ndc = pin.NDC;

    float4 posH = float4(ndc.x, ndc.y, z_ndc, 1.0f);

    // 3) ViewProj 역행렬로 월드좌표 복원
    float4 posW = mul(posH, gInvViewProj);
    posW /= posW.w;
    float3 pixelPos = posW.xyz;

    // 4) 유닛들 기준으로 어둠 정도 계산
    float darkness = 1.0f; // 1 = 완전 어둠, 0 = 완전 밝음

    [loop]
    for (int i = 0; i < gUnitCount; ++i)
    {
        float3 uPos = gUnits[i].CenterPosRadius.xyz;
        float rad = gUnits[i].CenterPosRadius.w;

        float dist = distance(pixelPos, uPos);

        // inner ~ outer 사이에서 소프트하게 전환
        float inner = rad;
        float outer = rad * 1.2f;

        float a = saturate((dist - inner) / max(outer - inner, 0.001f));

        // 여러 유닛이 있으면 가장 밝아지는(=a가 작은) 값 기준
        darkness = min(darkness, a);
    }

    float4 col = gDarkColor;
    col.a *= darkness; // 어둠 알파에 강도 적용

    return col;
}