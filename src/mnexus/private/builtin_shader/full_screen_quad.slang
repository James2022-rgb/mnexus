
struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float2 texcoord;
}

[shader("vertex")]
VertexStageOutput VertexMain(
    uint vertex_id : SV_VertexID
)
{
    VertexStageOutput output;
    output.texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.sv_position = float4(output.texcoord * 2.0 + -1.0, 0.0, 1.0);
    return output;
}
