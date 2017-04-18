struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float4 normal : NORMAL;
};

float4 scaleVecTo1(float4 vec)
{
    float factor = 0.0f;
    
    if (vec.x > factor)
    {
        factor = vec.x;
    }
    if (vec.y > factor)
    {
        factor = vec.y;
    }
    if (vec.z > factor)
    {
        factor = vec.z;
    }
    
    factor = 1.0f / factor;

    return vec * factor;
}

float4 main(PixelInputType input) : SV_TARGET
{
    float4 ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
    //float4 ambient = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4 lightPos = (100.0f, 200.0f, -50.0f, 1.0f);
    float4 finalColor = ambient;

    //Diffuse Light
    float4 lightDir = -normalize(lightPos - input.position);

    float lightIntensity = saturate(dot(input.normal, lightDir));
    if (lightIntensity > 0.0f)
    {
        finalColor += (diffuse * lightIntensity);
    }

    finalColor = saturate(finalColor);

    finalColor = scaleVecTo1(input.normal) * finalColor;

    return finalColor; ;
}