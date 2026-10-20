#ifndef P3D_GEOMETRY_INPUT_HPP
#define P3D_GEOMETRY_INPUT_HPP

namespace p3dGeometryInput {
extern bool enabled;
extern unsigned records, reads, bytes;
const unsigned BlockRecords = 64;

// Only unpublished, nonskinned primitive groups use this path. Keep each
// attribute's original consumer and order; combine only adjacent source reads.
template<class Record, unsigned Words, class File, class DataType, class Consumer, class Yield>
void Read(File* file, int count, DataType type, Consumer consume, Yield yield)
{
    static_assert(sizeof(Record) == Words * 4, "Geometry record has padding");
    Record input[BlockRecords];
    unsigned calls = 0;
    for(int offset = 0; offset < count;)
    {
        const unsigned remaining = unsigned(count - offset);
        const unsigned chunk = remaining < BlockRecords ? remaining : BlockRecords;
        file->GetData(input, chunk * Words, type);
        for(unsigned i = 0; i < chunk; ++i) consume(input[i], unsigned(offset) + i);
        // File reads also gate cooperative progress by call count. Retain a
        // deadline check after each bounded block when combining those reads.
        yield();
        offset += int(chunk);
        ++calls;
    }
    if(count > 0)
    {
        __atomic_fetch_add(&records, unsigned(count), __ATOMIC_RELAXED);
        __atomic_fetch_add(&reads, calls, __ATOMIC_RELAXED);
        __atomic_fetch_add(&bytes, unsigned(count) * sizeof(Record), __ATOMIC_RELAXED);
    }
}
}
#endif
