cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    
    float3 gEyePosW;
    float  gPad0; 

    // Material
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;

    // Light
    float3 gLightDir;
    float  gPad1;
    float4 gLightColor;
    float4 gAmbientLight;
};

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    // Assume nonuniform scaling is not used or handled in C++
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    vout.PosH = mul(posW, gViewProj);

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

    // Base Color
    float4 totalAlbedo = gDiffuseAlbedo;

    // Ambient
    float4 ambient = gAmbientLight * totalAlbedo;

    // Diffuse
    float diffuseFactor = dot(lightVec, pin.NormalW);
    float4 diffuse = float4(0, 0, 0, 0);
    float4 specular = float4(0, 0, 0, 0);

    if(diffuseFactor > 0.0f) {
        diffuse = diffuseFactor * gLightColor * totalAlbedo;

        float3 v = reflect(-lightVec, pin.NormalW);
        float3 halfVec = normalize(lightVec + toEyeW);
        float shininess = (1.0f - gRoughness) * 256.0f; // rough approx
        
        float specFactor = pow(max(dot(pin.NormalW, halfVec), 0.0f), shininess);
        float3 fresnel = SchlickFresnel(gFresnelR0, halfVec, lightVec);
        
        specular = float4(fresnel * specFactor, 1.0f) * gLightColor;
    }

    float4 litColor = ambient + diffuse + specular;
    
    // Preserve Alpha for Blending/Stenciling
    litColor.a = gDiffuseAlbedo.a;

    return litColor;
}
