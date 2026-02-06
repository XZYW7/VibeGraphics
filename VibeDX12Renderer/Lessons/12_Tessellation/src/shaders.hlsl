cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
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
    
    // Hardcoded tessellation factor for demo
    float tess = 8.0f; // Increase this to see more triangles

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
    return hout;
}

struct DomainOut
{
    float4 PosH : SV_POSITION;
};

[domain("quad")]
DomainOut DS(PatchTess patchTess, 
             float2 uv : SV_DomainLocation, 
             const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
    
    // Bilinear interpolation of position
    // Quad vertex order assumption:
    // 0 -- 1
    // |    |
    // 2 -- 3
    // (This depends on how we submit geometry, we will use a standard quad 0,1,2,3 strip-like or list)
    
    // Standard interpolation
    float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x); 
    float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x); 
    float3 p  = lerp(v1, v2, uv.y); 
    
    // Apply a simple wave function for displacement
    float r = sqrt(p.x*p.x + p.z*p.z);
    p.y = 0.3f * (cos(r * 4.0f - 2.0f));

    dout.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(0.2f, 0.8f, 0.4f, 1.0f);
}
