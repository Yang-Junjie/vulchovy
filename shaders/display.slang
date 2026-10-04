// Fullscreen display pass: samples the linear HDR image produced by the ray
// tracing kernel, tonemaps it and writes it to the (sRGB) swapchain.

[[vk::binding(0, 0)]] Sampler2D g_hdr;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VSOutput vertexMain(uint vertex_id : SV_VertexID) {
    // Fullscreen triangle: (-1,-1), (3,-1), (-1,3) in Vulkan clip space.
    const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);

    VSOutput output;
    output.uv = uv;
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float3 tonemap_aces(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target {
    const float3 hdr = g_hdr.Sample(input.uv).rgb;
    return float4(tonemap_aces(hdr), 1.0);
}
