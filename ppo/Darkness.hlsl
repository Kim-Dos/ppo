// Darkness.hlsl


// 카메라/프로젝션 정보 (Common.hlsl의 cbPass 내용 그대로)
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
}

// 깊이버퍼 (0~1)
Texture2D gDepthTex : register(t0);
// 샘플러 – 기존 static sampler에서 pointClamp로 맞춰서 사용
SamplerState gsamPointClamp : register(s0);

static const int MaxFogUnits = 64;




// x,y,z : 유닛 월드 위치 (PosW)
// w     : 시야 반경 (월드 단위, 예: 600.0f)
struct DarknessUnit
{
    float4 CenterPosRadius;
};

cbuffer cbDarkness : register(b1)
{
    DarknessUnit gUnits[MaxFogUnits];
    int gUnitCount;
    float3 gPad;
    float4 gDarkColor; // 암흑 색
    float4 gGlowColor; // Glow 색
};

struct VertexIn
{
    float2 Pos : POSITION; // 풀스크린 [-1,1]
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 NDC : TEXCOORD0;
    float2 TexC : TEXCOORD1;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = float4(vin.Pos, 0.0f, 1.0f);
    vout.NDC = vin.Pos;

    // NDC(-1~1) → UV(0~1)
    vout.TexC.x = vin.Pos.x * 0.5f + 0.5f;
    vout.TexC.y = -vin.Pos.y * 0.5f + 0.5f;

    return vout;
}
float4 PS(VertexOut pin) : SV_TARGET
{
    // 1) 화면 좌표
    float2 ndc = pin.NDC;

    // 2) 깊이 읽기 (0~1)
    float depth = gDepthTex.Sample(gsamPointClamp, pin.TexC).r;

    // 깊이가 1.0(하늘/비어있음)이면 그냥 암흑 유지해도 됨
    if (depth >= 1.0f - 1e-4f)
    {
        return float4(0, 0, 0, 1);
    }

    // 3) clip space 위치 (x_ndc, y_ndc, z_ndc, 1)
    float z_ndc = depth * 2.0f - 1.0f;
    float4 posH = float4(ndc.x, ndc.y, z_ndc, 1.0f);

    // 4) 월드 좌표 복원: world = mul(invViewProj, clip)
    float4 posW = mul(posH, gInvViewProj);
    posW /= posW.w;
    float3 worldPos = posW.xyz;

    // 5) 초기 darkness = 1 (완전 어두움)
    float darkness = 1.0f;

    // 6) 각 유닛의 월드 위치/반경으로 거리 체크
    [loop]
    for (int i = 0; i < gUnitCount; ++i)
    {
        float3 unitPosW = gUnits[i].CenterPosRadius.xyz;
        float radius = gUnits[i].CenterPosRadius.w;

        float dist = distance(worldPos, unitPosW);

        // 부드러운 경계용 – 반경 바깥쪽 20%를 소프트 영역으로
        float inner = radius;
        float outer = radius * 1.2f;
        float a = saturate((dist - inner) / max(outer - inner, 0.001f));

        darkness = min(darkness, a);
    }

    // 7) 색/알파 적용
    float4 col = gDarkColor;
    col.a *= darkness;

    return col;
}