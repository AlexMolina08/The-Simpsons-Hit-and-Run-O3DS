// Fixed-capacity FIFO: preserve the newest intervals, count overwritten ones.
#ifndef OLD3DS_RING_H
#define OLD3DS_RING_H
template<class T, unsigned Capacity> class Old3dsRing
{
public:
    Old3dsRing() : first(0), count(0), overwritten(0) {}
    T& Push()
    {
        unsigned slot = (first + count) % Capacity;
        if(count == Capacity) { first = (first + 1) % Capacity; ++overwritten; }
        else ++count;
        return records[slot];
    }
    unsigned Size() const { return count; }
    unsigned Overwritten() const { return overwritten; }
    const T& At(unsigned i) const { return records[(first + i) % Capacity]; }
    void Clear() { first = count = overwritten = 0; }
private:
    T records[Capacity];
    unsigned first, count, overwritten;
};
#endif
