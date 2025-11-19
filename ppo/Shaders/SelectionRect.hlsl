cbuffer cbSelection : register(b1) // 루트 파라미터 1에 바인딩
{
    float2 gPosNDC; // 중심 (NDC)
    float2 gSizeNDC; // 크기 (NDC)
    float4 gColor; // 색
};

struct VertexIn
{
    float2 Pos : POSITION; // -0.5 ~ +0.5 로컬 좌표
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // 로컬(-0.5~0.5)을 크기 적용 + 중심 이동해서 NDC 좌표로
    float2 scaled = vin.Pos * gSizeNDC;
    float2 ndc = gPosNDC + scaled;

    vout.PosH = float4(ndc.x, ndc.y, 0.0f, 1.0f);
    vout.Color = gColor;
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    return pin.Color;
}