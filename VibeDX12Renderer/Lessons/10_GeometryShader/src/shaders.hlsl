cbuffer cbPerObject : register(b0)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float gPad;
};

struct VertexIn
{
    float3 PosW : POSITION;
    float2 SizeW : SIZE;
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 SizeW : SIZE;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    // Just pass data to GS
    vout.CenterW = vin.PosW;
    vout.SizeW = vin.SizeW;
    return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1], inout TriangleStream<GeoOut> triStream)
{
    // Compute Billboard vectors
    // To make the quad face the camera:
    // look = CameraPos - Center
    // right = look x up
    
    // We want "Vertical" Billboards (trees usually stand up straight)
    // So Up is fixed to (0,1,0).
    float3 up = float3(0.0f, 1.0f, 0.0f);
    
    float3 look = gEyePosW - gin[0].CenterW;
    look.y = 0.0f; // Walk on XZ plane
    look = normalize(look);
    
    float3 right = cross(up, look);
    
    float halfW = 0.5f * gin[0].SizeW.x;
    float halfH = 0.5f * gin[0].SizeW.y;
    
    float4 v[4];
    // Bottom Left
    v[0] = float4(gin[0].CenterW + halfW*right - halfH*up, 1.0f);
    // Bottom Right
    v[1] = float4(gin[0].CenterW - halfW*right - halfH*up, 1.0f); // Wait? Right vector direction check
    
    // Standard: Right is usually (1,0,0) if looking down -Z.
    // If we face camera, Right points to right of camera.
    // Let's verify standard math: Right = Cross(Up, Look).
    
    // Vertices order for Strip: BottomLeft, TopLeft, BottomRight, TopRight? 
    // Or BL, BR, TL, TR?
    // D3D Strip: v0, v1, v2 -> Tri1. v1, v2, v3 -> Tri2.
    
    // Let's use:
    // 0: Bottom Left
    // 1: Top Left
    // 2: Bottom Right
    // 3: Top Right
    
    // Wait, let's fix coordinates relative to right/up vectors
    // Center - Right + Up (Top Left)
    // Center + Right + Up (Top Right)
    // Center - Right - Up (Bottom Left)
    // Center + Right - Up (Bottom Right)
    
    float3 p0 = gin[0].CenterW - halfW*right - halfH*up; // Bottom Left? (Assuming Right points right)
    float3 p1 = gin[0].CenterW - halfW*right + halfH*up; // Top Left
    float3 p2 = gin[0].CenterW + halfW*right - halfH*up; // Bottom Right
    float3 p3 = gin[0].CenterW + halfW*right + halfH*up; // Top Right
    
    // Strip Order: BottomLeft, TopLeft, BottomRight, TopRight
    // 0, 1, 2 gives BL, TL, BR. (CCW?)
    // 1, 2, 3 gives TL, BR, TR. (CCW?)
    
    GeoOut gout;
    // Common Normal (facing camera)
    gout.NormalW = look; 
    
    // V0: Bottom Left
    gout.PosW = p0;
    gout.PosH = mul(float4(p0, 1.0f), gViewProj);
    gout.TexC = float2(0.0f, 1.0f);
    triStream.Append(gout);
    
    // V1: Top Left
    gout.PosW = p1;
    gout.PosH = mul(float4(p1, 1.0f), gViewProj);
    gout.TexC = float2(0.0f, 0.0f);
    triStream.Append(gout);
    
    // V2: Bottom Right
    gout.PosW = p2;
    gout.PosH = mul(float4(p2, 1.0f), gViewProj);
    gout.TexC = float2(1.0f, 1.0f);
    triStream.Append(gout);
    
    // V3: Top Right
    gout.PosW = p3;
    gout.PosH = mul(float4(p3, 1.0f), gViewProj);
    gout.TexC = float2(1.0f, 0.0f);
    triStream.Append(gout);
}

float4 PS(GeoOut pin) : SV_Target
{
    // Simple colored tree pattern
    float2 uv = pin.TexC - 0.5f; // -0.5 to 0.5 Range. Y is -0.5(Top) to 0.5(Bottom)
    
    // Trunk: Bottom center rectangle
    // uv.y goes from -0.5 (top) to +0.5 (bottom)
    bool isTrunk = (abs(uv.x) < 0.05f && uv.y > 0.1f);
    
    // Leaves: Circle shifted up
    float2 leafUV = uv;
    leafUV.y += 0.1f;
    float dist = length(leafUV);
    bool isLeaves = (dist < 0.35f);
    
    if(!isLeaves && !isTrunk) {
        clip(-1); // Discard invisible pixels
    }
    
    float3 color = float3(0,0,0);
    
    if(isLeaves) {
         // Radial Gradient Green
        color = lerp(float3(0.4f, 0.8f, 0.2f), float3(0.0f, 0.3f, 0.0f), dist*3.0f);
    } else {
        // Brown Trunk
        color = float3(0.45f, 0.25f, 0.1f);
    }

    return float4(color, 1.0f);
}
