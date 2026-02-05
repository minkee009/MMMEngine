#include "../CommonSharedPS.hlsli"

Texture2D _lutMap : register(t10);
Texture2D _roughness : register(t11);
Texture2D _ambientOcclusion : register(t12);

// PBR Ambient
TextureCube _specular : register(t20);
TextureCube _irradiance : register(t21);
Texture2D _brdflut : register(t22);

cbuffer ToonMatBuffer : register(b3)
{
    float4 mBaseColor;

	float mAoStrength; 
    float mDiffuseStr;
    float mSpecularStr;
    float mRoughness;

    float mLowLut;
    float mDiffGradientDistHalf;
    float mDiffGradientDepth;
    float mRimLightStr;

    float mEmissive;
    float3 mPadding;
}

// 수동 PCF
float CalculateShadowPCF(float4 LightPos)
{
    float currentShadowDepth = LightPos.z / LightPos.w; // 쉐도우맵 기준 NDC Z좌표
    float2 shadowUV = LightPos.xy / LightPos.w;

    shadowUV.y *= -1.0f;
    shadowUV = (shadowUV * 0.5f) + 0.5f;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
    shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;

    if (currentShadowDepth < 0.0f || currentShadowDepth > 1.0f)
        return 1.0f;

    float bias = 0.0002f;
    currentShadowDepth -= bias;

// 3x3 PCF
    float shadow = 0.0f;
    float2 texelSize = 1.0f / 4096.0f; // Shadow Map 크기

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += _shadowmap.SampleCmpLevelZero(_cmpsp0, shadowUV + offset, currentShadowDepth);
        }
    }
    shadow /= 9.0f;

    return shadow;
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0)
                * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 main(PS_INPUT input) : SV_TARGET
{    
    float3 normalTex = _normal.Sample(_sp0, input.Tex).xyz * 2.0f - 1.0f; // 정규화

	 // Normal
    float3 normalMap = _normal.Sample(_sp0, input.Tex).xyz;
    normalMap = normalize(normalMap * 2.0f - 1.0f);
    float3x3 tbn = float3x3(normalize(input.Tan), normalize(input.BiTan), normalize(input.Norm));
    float3 N = normalize(mul(normalMap, tbn));
    
    float3 V = normalize(mCamPos.xyz - input.W_Pos.xyz);
    float3 L = -mLightDir.xyz;
    float3 R = reflect(-V, N);  // 큐브맵 반사를 위한 리플렉트 벡터
	float RimNdotL = dot(mLightPos.xyz, N);
	float NdotL = saturate(dot(N, L));

     // 간접광 (IBL)
    float roughness = _roughness.Sample(_sp0, input.Tex).r * mRoughness;
    float3 irradiance = _irradiance.Sample(_sp0, N).rgb;
    float ao = _ambientOcclusion.Sample(_sp0, input.Tex).r * mAoStrength;

    float NdotV = saturate(dot(N,V));
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), normalTex, 0.0f);
    float3 F_ibl = fresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0 - F_ibl) * (1.0 - 0.0f);
    float3 diffuseIBL = kD_ibl * irradiance * normalTex;

    float4 ambient = diffuseIBL * ao;
    
    // 조명 위치와 픽셀 위치
    //float3 toLight =  - input.WorldPos;
    //float distance = length(toLight);

    // 감쇠 계수 (1 / d² 형태)
    //float attenuation = 1.0f / (distance * distance);
    
    //float lightDist = attenuation;
	float shadow = CalculateShadowPCF(input.S_Pos); // 그림자 인자 계산
	float shadowLut = _lutMap.Sample(_samPoint, float2(shadow * 0.5f + 0.495f, 0.5f)).r;

    float diff = NdotL;
	
	//float bandLevel = 1.0f;
	//diff = ceil(diff * bandLevel)/bandLevel;
	float diffLut = _lutMap.Sample(_samPoint, float2((diff * 0.5f) + 0.495f,0.5f)).r;  // 카툰렌더링 활성화
	

	float diffShadow = min(shadowLut, diffLut); // 그림자와 조명값 중 작은 값을 사용
	diffShadow = diffShadow > mLowLut ? diffShadow : (dot(N, -L) * mDiffGradientDistHalf + mDiffGradientDepth);

    float4 diffuse = mDiffuseStr * mLightColor * diffShadow;
    
    // Specular
    uint width, height, numMips;
    _specular.GetDimensions(0, width, height, numMips);

    float maxMip = float(numMips - 1);
    float lod = roughness * maxMip;

    float3 prefilteredColor = _specular.SampleLevel(_sp0, R, lod).rgb;
    float2 brdf = _brdflut.Sample(_sp0, float2(NdotV, roughness)).rg;
    float3 specTex = prefilteredColor * (F_ibl * brdf.x + brdf.y);

    float3 viewDir = normalize(mCamPos.xyz - input.W_Pos.xyz);
    float3 halfDir = normalize(viewDir + L); // 스펙큘러연산을 위한 하프 벡터
    
    //float spec = pow(saturate(dot(halfDir, N)), shininess) * sqrt(diff); // * sqrt(diff) <- 이걸 쓰면 shininess < 32 에서 아티팩트가 사라짐..!!! 
    
	specTex = smoothstep(0.01, 0.02f, specTex); // <- 카툰렌더링용 스펙큘러
	shadowLut = shadowLut > 0.999f ? 1.0f : 0.0f; // 카툰렌더링용 그림자
	shadowLut = diff > 0.6f ? shadowLut : 0.0f; // 디퓨즈가 낮으면 그림자 제거
	float4 specular = specTex * mSpecularStr * mLightColor * shadowLut;
    
	float4 emmisive = _emissive.Sample(_sp0, input.Tex);

    // 알파 클리핑용 디퓨즈 샘플링
    float4 baseTex = _albedo.Sample(_sp0, input.Tex) * mBaseColor;

    // 알파 임계값 설정 (0.1~0.5 정도 보통 사용)
    const float alphaCutoff = 0.5f;

    // 알파가 낮으면 픽셀 폐기
    clip(baseTex.a - alphaCutoff);

	float minusRimNdotL  = dot(-mLightPos.xyz , N);

	// 림 라이트
	float _RimAmount = 0.716;
	float4 rimDot = 1 - dot(viewDir, N);
	float rimIntensity = rimDot * RimNdotL;
	rimIntensity = smoothstep(_RimAmount - 0.01, _RimAmount + 0.01, rimIntensity);


	float _negativeRimAmount = 0.58;
	float rimMinusIntensity = rimDot * minusRimNdotL;
	rimMinusIntensity = smoothstep(_negativeRimAmount - 0.21, _negativeRimAmount + 0.21, rimMinusIntensity);

	float4 minusRim = rimMinusIntensity * diffuseIBL * 0.35 * mRimLightStr;
	
	float4 rim = rimIntensity * mLightColor * mRimLightStr;
    
    float3 baseRGB = (specular + diffuse + ambient + rim + minusRim).rgb * baseTex.rgb + emmisive.rgb ;

	//float3 toGradientPos = gradientPos - input.WorldPos;
	//float GradientDistance = length(toGradientPos);
	//GradientDistance = max(GradientDistance, 0.0001f);
	//float GradientAttenuation = 1.0f / (GradientDistance);
	//
	//GradientAttenuation *= 3.5f * gradientIntensity; 
	//GradientAttenuation = saturate(GradientAttenuation);
	//
	//baseRGB = baseRGB * GradientAttenuation;

    //return specular;
    return float4(baseRGB, baseTex.a);
}