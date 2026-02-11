
[[vk::binding(0, 0)]] Sampler2D u_texture_sampler;

struct VertexStageOutput
{
    [[vk::location(0)]] float2 texcoord;
}

[shader("fragment")]
float4 FragmentMain(VertexStageOutput input) : SV_Target
{
    return u_texture_sampler.Sample(input.texcoord);
}
