// Engine-independent reconstruction of the verified AdvancedCloth flow path.
// Inputs: linear data texture sample, seconds, source parameters and view terms.
// UV0 is the first mesh UV set, UV1 the second, UV2 the third.
float2 NarakaEffectUV(float2 uv0, float2 uv1, float2 uv2,
                     uint selector, float4 effectUVTransform)
{
    float2 uv = uv0 * float((selector & 1u) != 0u)
              + uv1 * float((selector & 2u) != 0u)
              + uv2 * float((selector & 4u) != 0u);
    return (uv - effectUVTransform.xy) / effectUVTransform.zw;
}

float NarakaMovingRegion(float region, float phase, float timeSeconds, float4 parameters)
{
    // x=phase frequency, y=angular speed (rad/s), z=threshold, w=sharpness.
    // This is a moving brightness field over a stationary sampled texture.
    float wave = sin(phase * parameters.x + timeSeconds * parameters.y) * 0.5 + 0.5;
    float shaped = pow(saturate((wave - parameters.z) / max(1.0 - parameters.z, 5.96046448e-8)), parameters.w);
    return region * region * shaped;
}

float NarakaFlowFacing(float normalDotView, float viewParameter)
{
    float facing = abs(normalDotView); // AdvancedCloth contract
    float front = pow(clamp(facing, 0.001, 0.999), max(0.0, 16.0 - viewParameter));
    float rim = pow(clamp(1.0 - facing, 0.001, 0.999), max(0.0, viewParameter - 16.0));
    return min(front, rim);
}

float3 NarakaPeriodicEmission(float4 packedLinear, float timeSeconds,
    float4 parameters0, float4 parameters1, float4 parameters2,
    float3 emissionColor, float4 effectColor0,
    float shadedNormalDotView, float geometricNormalDotView, float detailGate)
{
    float layer0 = NarakaMovingRegion(packedLinear.r, packedLinear.g, timeSeconds, parameters0)
                 * NarakaFlowFacing(shadedNormalDotView, parameters2.z);
    float layer1 = NarakaMovingRegion(packedLinear.b, packedLinear.a, timeSeconds, parameters1)
                 * NarakaFlowFacing(shadedNormalDotView, parameters2.w);
    float3 tint1 = effectColor0.rgb * lerp(float3(1.0, 1.0, 1.0), emissionColor, parameters2.y);
    float reverseIntensity = geometricNormalDotView < 0.0 ? effectColor0.a : 1.0;
    // Game exposure compensation is scene-global and cannot be inferred from PFB.
    // Apply this result through the destination engine's linear HDR emission path.
    return (layer0 * emissionColor + layer1 * tint1) * reverseIntensity * detailGate;
}

// Independent fx_uber path: ordinary scrolling uses UV + velocity * seconds.
// UV sampling offsets move the visible pattern opposite to their sign.
// The current outfit's values: main=(0,+0.4), second=(0,+0.07), distort=(0,-0.2).
float2 NarakaScrollingUV(float2 uv, float2 scale, float2 offset,
                        float2 scaleAnchor, float2 velocity, float timeSeconds)
{
    return (uv - scaleAnchor) * scale + scaleAnchor + offset + velocity * timeSeconds;
}
