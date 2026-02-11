
struct Params
{
    uint src_offset;
    uint src_bytes_per_row;
    uint dst_bytes_per_row;
    uint row_count;
};

[[vk::binding(0, 0)]] ConstantBuffer<Params> params;
[[vk::binding(1, 0)]] ByteAddressBuffer       src_buffer;
[[vk::binding(2, 0)]] RWByteAddressBuffer      dst_buffer;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint col = dtid.x;
    uint row = dtid.y;

    if (row >= params.row_count) return;

    uint words_per_row = params.src_bytes_per_row >> 2;
    if (col >= words_per_row) return;

    uint byte_col = col << 2;
    uint src_addr = params.src_offset + row * params.src_bytes_per_row + byte_col;
    uint dst_addr = row * params.dst_bytes_per_row + byte_col;

    dst_buffer.Store(dst_addr, src_buffer.Load(src_addr));
}
