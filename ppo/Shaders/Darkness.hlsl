// 유닛 최대 개수
static const int MaxFogUnits = 64;

struct DarknessUnit
{
    float4 CenterRadiusSoft;
    // x,y : center NDC
    // z   : radius
    // w   : softness
};

cbuffer cbDarkness : register(b1) // 루트 파라미터 1
{
    DarknessUnit gUnits[MaxFogUnits];
    int gUnitCount;
    float3 gPad;
    float4 gDarkColor;
};

struct VertexIn
{
    float2 Pos : POSITION; // 풀스크린 [-1,1]
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 NDC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = float4(vin.Pos, 0.0f, 1.0f);
    vout.NDC = vin.Pos; // 이미 NDC
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float2 ndc = pin.NDC;

    // darkness = 1  → 완전 암흑
    // darkness = 0  → 완전 밝음
    float darkness = 1.0f;

    [loop]
    for (int i = 0; i < gUnitCount; ++i)
    {
        float2 center = gUnits[i].CenterRadiusSoft.xy;
        float radius = gUnits[i].CenterRadiusSoft.z;
        float softness = gUnits[i].CenterRadiusSoft.w;

        float dist = length(ndc - center);

        // dist <= radius        → 그 유닛 기준으로는 밝음 (0)
        // radius~radius+soft    → 0~1로 서서히 어두움
        // radius+soft 이상      → 1 (그 유닛 기준으론 완전 어둠)
        float a = saturate((dist - radius) / max(softness, 0.0001f));

        // 여러 유닛 중에서 **가장 밝게 만드는 값**을 선택해야 하므로 최소값을 사용
        darkness = min(darkness, a);
    }

    float4 col = gDarkColor;
    col.a *= darkness; // 최종 알파

    return col;
}