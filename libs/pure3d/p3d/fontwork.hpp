#ifndef P3D_FONT_WORK_HPP
#define P3D_FONT_WORK_HPP
#ifdef SRR2_OLD3DS_NATIVE_BENCH
namespace p3dFontWork {
extern bool enabled;
extern bool quadStream;
extern unsigned quadBatches,quadGlyphs,quadFallbacks;
extern unsigned preparedLines,preparedGlyphs,longLines;
const unsigned Capacity=256;
// Resolve one bounded line. Longer lines use the original renderer in full.
template<class Character,class Lookup>
bool Prepare(const Character* text,int* glyphs,unsigned& count,Lookup lookup)
{
    count=0;
    while(text[count] && text[count]!='\n'){
        if(count==Capacity)return false;
        glyphs[count]=lookup(text[count]);++count;
    }
    return true;
}
}
#endif
#endif
