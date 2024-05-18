#include "Common.hlsl"


struct VertexOut
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD;
};

// Constant data that varies per frame.
//cbuffer cbUIObject : register(b0)
//{
//   uint2 UICenter;
//    uint2 UIsize;
//    uint2 screenSize;
//    float alpha;
//};

VertexOut VS(uint VertexID : SV_VertexID)
{
    VertexOut vout;
    
    //float2 center = float2(gWorld._11, gWorld._12);
    //float2 size = float2(gWorld._13, gWorld._14);
    //float2 screensize = float2(gWorld._21, gWorld._22);
    
    
    // VertexIn ���� ������ ClipSpace�� ��ȯ���ִ� �� -> ���� Ǯ��ũ�� �׸� ==> Pos�� ���� �ٲ�� �׸�ŭ ���� ��ȭ
    
    if (VertexID == 0) { vout.Pos = float4(-0.8, 1, 0, 1); vout.UV = float2(0,0); }
    else if (VertexID == 1) { vout.Pos = float4(0.8, -1, 0, 1); vout.UV = float2(1,1); }
    else if (VertexID == 2) { vout.Pos = float4(-0.8, -1, 0, 1); vout.UV = float2(0,1); }
    else if (VertexID == 3) { vout.Pos = float4(-0.8, 1, 0, 1); vout.UV = float2(0,0); }
    else if (VertexID == 4) { vout.Pos = float4(0.8, 1, 0, 1); vout.UV = float2(1,0); }
    else if (VertexID == 5) { vout.Pos = float4(0.8, -1, 0, 1); vout.UV = float2(1,1); }


    
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    //float alpha = float(gWorld._23);
    return gDiffuseMap[7].Sample(gsamAnisotropicWrap, pin.UV);
    
    //return float4(pin.UV.x, pin.UV.y, 0, 1);
}

