cbuffer cbCursor : register(b1) // ★ 루트 파라미터 슬롯 1 사용
{
    float2 gCursorPosNDC; // 마우스 위치 (NDC, -1~1)
    float2 gCursorSizeNDC; // 커서 크기 (NDC 기준)
};

Texture2D gCursorTex : register(t0); // 루트 파라미터 4의 texTable0에서 첫 번째 슬롯
SamplerState gsamAnisotropicWrap : register(s0);

struct VertexIn
{
    float2 Pos : POSITION; // 정점에서 -0.5~+0.5 정도의 로컬 위치
    float2 Tex : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // 로컬(-0.5~0.5)을 크기*위치 적용해서 NDC로 변환
    float2 scaled = vin.Pos * gCursorSizeNDC;
    float2 ndc = gCursorPosNDC + scaled;

    vout.PosH = float4(ndc.x, ndc.y, 0.0f, 1.0f);
    vout.Tex = vin.Tex;
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 color = gCursorTex.Sample(gsamAnisotropicWrap, pin.Tex);
    // 필요하면 알파 사용해서 투명도 처리 가능
    return color;
}