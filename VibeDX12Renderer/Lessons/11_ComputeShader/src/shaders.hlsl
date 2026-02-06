cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

// --- Graphics Pipeline Resources ---
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL  : POSITION;
    float2 TexC  : TEXCOORD;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float2 TexC  : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.TexC = vin.TexC;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return gTexture.Sample(gSampler, pin.TexC);
}

// --- Compute Pipeline Resources ---
// t0 is Input Image (SRV) - mapped to gInput
// u0 is Output Image (UAV) - mapped to gOutput

Texture2D<float4> gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(16, 16, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 xy = dispatchThreadID.xy;
    
    // Get dimensions (assuming 512x512 for now or Query)
    uint width, height;
    gInput.GetDimensions(width, height);
    
    if(xy.x >= width || xy.y >= height) return;

    // Simple Box Blur (Radius 2)
    float4 colorSum = float4(0,0,0,0);
    int count = 0;
    int radius = 4;
    
    for(int i = -radius; i <= radius; ++i)
    {
        for(int j = -radius; j <= radius; ++j)
        {
            int2 offset = int2(i, j);
            int2 samplePos = xy + offset;
            
            // Clamp to boundary
            samplePos = clamp(samplePos, int2(0,0), int2(width-1, height-1));
            
            colorSum += gInput[samplePos];
            count++;
        }
    }
    
    gOutput[xy] = colorSum / (float)count;
}
