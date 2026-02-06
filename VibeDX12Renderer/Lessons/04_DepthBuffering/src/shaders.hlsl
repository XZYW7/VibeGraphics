cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj; 
};

struct Vertex
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct VertexShaderOutput
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexShaderOutput VS(Vertex vin)
{
    VertexShaderOutput vout;
    
    // 补全为 float4，并在最后一位设为 1.0 用于矩阵乘法
    float4 posW = float4(vin.Pos, 1.0f);
    
    // 变换到裁剪空间
    vout.PosH = mul(posW, gWorldViewProj);
    
    vout.Color = vin.Color;
    return vout;
}

float4 PS(VertexShaderOutput pin) : SV_Target
{
    return pin.Color;
}
