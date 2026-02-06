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
    // 直接传递坐标，不进行矩阵变换（假设坐标已经是裁剪空间坐标，即在 -1 到 1 之间）
    vout.PosH = float4(vin.Pos, 1.0f);
    vout.Color = vin.Color;
    return vout;
}

float4 PS(VertexShaderOutput pin) : SV_Target
{
    return pin.Color;
}
