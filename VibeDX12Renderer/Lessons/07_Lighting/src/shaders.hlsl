cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    float3 gEyePosW;
    float  gPad0;
    float3 gLightDir; 
    float  gPad1;
    float4 gLightColor;
    float4 gAmbientColor;
};

struct VertexIn {
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut {
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin) {
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // Assuming uniform scaling, so we can use gWorld for normals
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    vout.PosH = mul(posW, gViewProj);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target {
    float3 N = normalize(pin.NormalW);
    float3 L = normalize(-gLightDir); 
    float3 V = normalize(gEyePosW - pin.PosW); 
    float3 H = normalize(L + V); 

    // Material properties (hardcoded for now)
    float3 albedo = float3(0.6f, 0.2f, 0.2f); // Reddish
    float roughness = 0.5f;
    float shininess = 1.0f - roughness;
    float specPower = 128.0f * shininess;

    // Ambient
    float3 ambient = gAmbientColor.rgb * albedo;

    // Diffuse
    float NdotL = max(dot(N, L), 0.0f);
    float3 diffuse = NdotL * gLightColor.rgb * albedo;

    // Specular
    float NdotH = max(dot(N, H), 0.0f);
    float spec = pow(NdotH, specPower);
    float3 specular = spec * gLightColor.rgb * 0.5f; // Specular intensity

    float3 finalColor = ambient + diffuse + specular;
    
    return float4(finalColor, 1.0f);
}
