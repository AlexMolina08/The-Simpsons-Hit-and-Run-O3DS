// Check actual PC text-bible entries against the 3DS control overrides.
#include "libs/scrooby/src/3dscontrols.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>

static uint32_t Read(const std::vector<unsigned char>& b, size_t a)
{
    assert(a + 4 <= b.size());
    return b[a] | uint32_t(b[a+1]) << 8 | uint32_t(b[a+2]) << 16 | uint32_t(b[a+3]) << 24;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(input)), {});
    assert(Read(b, 12) == 0x1800d);
    size_t a = 12 + Read(b, 16);
    assert(Read(b, a) == 0x1800e);
    a += 12;
    a += 1 + b.at(a);
    assert(b.at(a++) == 'E');
    const unsigned count = Read(b,a), modulo = Read(b,a+4), bytes = Read(b,a+8);
    a += 12;
    const size_t hashes = a, offsets = a + count * 4, strings = a + count * 8;
    assert(strings + bytes <= b.size());
    unsigned changed = 0, tutorial = 0;
    for(unsigned i = 0; i < count; ++i)
    {
        const size_t offset = strings + Read(b, offsets + i * 4);
        std::vector<unsigned short> text;
        for(size_t j = offset; j + 1 < strings + bytes; j += 2)
        {
            const unsigned short c = b[j] | unsigned(b[j+1]) << 8;
            text.push_back(c);
            if(!c) break;
        }
        assert(!text.empty() && text.back() == 0);
        const unsigned hash = Read(b, hashes + i * 4);
        const char* replacement = Scrooby3DSControls::Replacement(hash, modulo, text.data());
        if(replacement)
        {
            ++changed;
            assert(strlen(replacement) < 256); // Tutorial bitmap buffer capacity.
            unsigned breaks = 0;
            for(const char* c = replacement; *c; ++c) if(*c == '\\') ++breaks;
            assert(breaks < 8); // Bitmap text supports eight lines.

            assert(!strstr(replacement,"CLICK") && !strstr(replacement,"SPACEBAR") && !strstr(replacement,"SHIFT"));
            if(hash == Scrooby3DSControls::Hash("TUTORIAL_011",modulo))
            {
                ++tutorial;
                assert(strstr(replacement,"PRESS Y"));
            }
            if(hash == Scrooby3DSControls::Hash("TUTORIAL_004",modulo))
            {
                ++tutorial;
                assert(strstr(replacement,"PRESS X"));
            }
        }
    }
    const unsigned short narrative[] = {'K','e','e','p',' ','c','o','n','t','r','o','l',0};
    assert(!Scrooby3DSControls::Replacement(123,modulo,narrative));
    assert(changed >= 13 && tutorial == 2);
    printf("3DS prompts: %u real catalog entries replaced; attack/interact distinct; narrative preserved\n",changed);
}
