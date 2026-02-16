
struct BlitParams
{
    float2 src_uv_min;
    float2 src_uv_max;
};

[[vk::binding(0, 0)]] ConstantBuffer<BlitParams> params;
[[vk::binding(1, 0)]] Texture2D<float4>           src_texture;
[[vk::binding(2, 0)]] SamplerState                src_sampler;

struct VertexStageOutput
{
    [[vk::location(0)]] float2 texcoord;
};

[shader("fragment")]
float4 FragmentMain(VertexStageOutput input) : SV_Target
{
    // Flip V: WebGPU NDC is Y-up but texture UV (0,0) is top-left.
    float2 tc = float2(input.texcoord.x, 1.0 - input.texcoord.y);
    float2 uv = lerp(params.src_uv_min, params.src_uv_max, tc);
    return src_texture.SampleLevel(src_sampler, uv, 0.0);
}
