// FogOfWar.hlsl

Texture2D gFogTex : register(t0);
SamplerState gsamLinear : register(s0);

struct VertexIn
{
    float2 PosL : POSITION; // -1~+1
    float2 TexC : TEXCOORD; // 0~1
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = float4(vin.PosL, 0.0f, 1.0f);
    vout.TexC = vin.TexC;
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float fog = gFogTex.Sample(gsamLinear, pin.TexC).r;

    // Hidden (0) → 검정색 불투명
    if (fog < 0.01f)
        return float4(0.0, 0.0, 0.0, 0.95);

    // Seen (128/255 = 0.5 근방) → 회색 반투명
    if (fog < 0.6f)
        return float4(0.1, 0.1, 0.1, 0.6);

    // Visible (1) → 완전 투명
    return float4(0.0, 0.0, 0.0, 0.0);
}