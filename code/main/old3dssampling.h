#ifndef OLD3DS_SAMPLING_H
#define OLD3DS_SAMPLING_H
// Sample once per block, rotating its position so repeated object orders do not
// always select the same vehicle or submesh. No game RNG state is consumed.
inline bool Old3dsRotatingSample(unsigned ordinal, unsigned bits)
{
    const unsigned mask=(1u<<bits)-1;
    return (ordinal&mask)==((ordinal>>bits)&mask);
}
#endif
