// Defaults for testing
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    
    float3 gEyePosW;
    float  gPad0; 

    // Material Defaults
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;

    // Light
    float3 gLightDir;
    float  gPad1;
    float4 gLightColor;
    float4 gAmbientLight;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gsamLinear : register(s0);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise need inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    // Pass Tex Coord
    vout.TexC = vin.TexC;

    return vout;
}

float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0*f0*f0*f0*f0);
    return reflectPercent;
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float3 lightVec = normalize(-gLightDir); 

    // Sample Texture
    float4 texColor = gDiffuseMap.Sample(gsamLinear, pin.TexC);
    
    // Combine Texture with Material Color
    float4 totalAlbedo = texColor * gDiffuseAlbedo;

    // Ambient
    float4 ambient = gAmbientLight * totalAlbedo;

    // Diffuse
    float diffuseFactor = dot(lightVec, pin.NormalW);
    float4 diffuse = float4(0, 0, 0, 0);
    float4 specular = float4(0, 0, 0, 0);

    if(diffuseFactor > 0.0f) {
        diffuse = diffuseFactor * gLightColor * totalAlbedo;

        // Specular
        float3 v = reflect(-lightVec, pin.NormalW);
        float3 halfVec = normalize(lightVec + toEyeW);
        float shininess = (1.0f - gRoughness) * 256.0f; 
        float specFactor = pow(max(dot(pin.NormalW, halfVec), 0.0f), shininess);
        float3 fresnel = SchlickFresnel(gFresnelR0, halfVec, lightVec);
        
        specular = float4(fresnel * specFactor, 1.0f) * gLightColor;
    }

    float4 litColor = ambient + diffuse + specular;
    litColor.a = totalAlbedo.a;

    return litColor;
}
