Texture2D gDiffuseMap : register(t0);
SamplerState gsamLinear : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    vout.TexC = vin.TexC;
    return vout;
}

struct PatchTess
{
    float EdgeTess[4]   : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    
    // Tessellation factor
    float tess = 64.0f; // High detail for smooth terrain

    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.EdgeTess[3] = tess;
    
    pt.InsideTess[0] = tess;
    pt.InsideTess[1] = tess;
    
    return pt;
}

struct HullOut
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> p, 
           uint i : SV_OutputControlPointID,
           uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    hout.TexC = p[i].TexC;
    return hout;
}

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

[domain("quad")]
DomainOut DS(PatchTess patchTess, 
             float2 uv : SV_DomainLocation, 
             const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
    
    // Interpolate UV
    float2 tex1 = lerp(quad[0].TexC, quad[1].TexC, uv.x);
    float2 tex2 = lerp(quad[2].TexC, quad[3].TexC, uv.x);
    float2 tex  = lerp(tex1, tex2, uv.y);
    
    // Interpolate Position
    float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x); 
    float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x); 
    float3 p  = lerp(v1, v2, uv.y); 
    
    // Simple Terrain Height
    // In a real application, you might sample a heightmap here:
    // float h = gHeightMap.SampleLevel(gsamLinear, tex, 0).r;
    // p.y += (h * 10.0f);
    
    // Using procedural wave for now
    float r = sqrt(p.x*p.x + p.z*p.z);
    p.y = 1.5f * (cos(r * 0.5f)); 

    dout.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    dout.TexC = tex;
    
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return gDiffuseMap.Sample(gsamLinear, pin.TexC);
}
