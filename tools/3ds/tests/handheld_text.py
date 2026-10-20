#!/usr/bin/env python3
"""Check handheld reflow, padding and content preservation, optionally with retail font metrics."""
from pathlib import Path
import json
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
fixtures = ['MOVE WITH THE CIRCLE PAD.\\PRESS A TO JUMP.\\HOLD B TO RUN.\\HOLD L + CIRCLE PAD TO MOVE THE CAMERA.',
            'PRESS A TO ACCELERATE.\\STEER WITH THE CIRCLE PAD.\\PRESS B TO BRAKE OR REVERSE.\\PRESS Y OR R TO HANDBRAKE.',
            '  WORDS\n\nWITH   SPACES  ', 'W' * 255, '']
advances = {i: 18.0 for i in range(32, 256)}
advances[32] = 8.0
font_height = 37.0
retail = 0
data = root / 'sdcard/3ds/simpsons/art/frontend'
font = data / 'scrooby2/resource/_ingame/font0_16.p3d'
bible = data / 'scrooby/resource/txtbible/srr2.p3d'
if font.is_file() and bible.is_file():
    b = font.read_bytes()
    a = 12
    _, header, total = struct.unpack_from('<III', b, a)
    cursor = a + 16
    cursor += 1 + b[cursor]
    cursor += 1 + b[cursor]
    font_height = struct.unpack_from('<4f', b, cursor)[2]
    cursor = a + header
    while cursor < a + total:
        kind, head, size = struct.unpack_from('<III', b, cursor)
        if kind == 0x22001:
            count = struct.unpack_from('<I', b, cursor + 12)[0]
            advances = {}
            for i in range(count):
                record = cursor + 16 + i * 40
                advances[struct.unpack_from('<I', b, record + 36)[0]] = struct.unpack_from('<f', b, record + 32)[0]
        cursor += size
    b = bible.read_bytes()
    a = 12 + struct.unpack_from('<I', b, 16)[0] + 12
    a += 1 + b[a]
    assert b[a] == ord('E')
    count, modulo, size = struct.unpack_from('<III', b, a + 1)
    hashes, offsets, strings = a + 13, a + 13 + count * 4, a + 13 + count * 8
    entries = {}
    for i in range(count):
        offset = strings + struct.unpack_from('<I', b, offsets + i * 4)[0]
        end = offset
        while b[end:end + 2] != b'\0\0':
            end += 2
        entries[struct.unpack_from('<I', b, hashes + i * 4)[0]] = b[offset:end].decode('utf-16le')
    overrides = dict(re.findall(r'\{"(TUTORIAL_\d+)", "([^"\n]*)"\}',
                               (root / 'libs/scrooby/src/3dscontrols.h').read_text()))
    for number in range(100):
        key = f'TUTORIAL_{number:03}'
        value = 0
        for c in key:
            value = (64 * value + ord(c)) % modulo
        if value in entries:
            text = overrides.get(key, entries[value]).replace('\\\\', '\\').replace('\\', '\n')
            fixtures.append(text[:255])
            retail += 1

layout_source = (root / 'code/presentation/gui/utility/hudmap.cpp').read_text()
layout_start = layout_source.index('                    const float blockHeight=height+(lineCount-1)*(height+5);')
layout_end = layout_source.index(';', layout_source.index('const float firstLine=', layout_start)) + 1
baseline_layout = layout_source[layout_start:layout_end]

literal = lambda value: 'u' + json.dumps(value, ensure_ascii=True)
widths = ','.join(f'{{{code},{advance}f}}' for code, advance in advances.items())
program = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "code/presentation/gui/utility/handheldtext.h"
std::map<unsigned,float> advances={''' + widths + r'''};
float measure(const char16_t* text,float height) {
    float width=0;
    while(*text) { auto found=advances.find(*text++); width+=found==advances.end()?18:found->second; }
    return width*height/''' + str(font_height) + r'''f;
}
std::u16string content(const std::u16string& value) {
    std::u16string result;
    for(auto c:value) if(c!=' ' && c!='\n' && c!='\r') result+=c;
    return result;
}
int main() {
    std::vector<std::u16string> fixtures={''' + ','.join(literal(s.replace('\\', '\n')) for s in fixtures) + r'''};
    unsigned minimum=16;
    for(const auto& text:fixtures) {
        char16_t lines[16][256]={};
        unsigned count=0;
        int height=16;
        bool fits=false;
        for(int candidate=16;candidate>=10;--candidate) {
            height=candidate;
            count=HandheldText::Wrap(text.c_str(),lines,[&](const char16_t* line){return measure(line,height)<=216;});
            fits=count<=16 && (!count || height+(count-1)*(height+5)<=113);
            if(fits) break;
        }
        // An artificial unbroken 255-glyph word may need more vertical space.
        // All authored tutorials must fit the actual padded card at readable size.
        if(text!=std::u16string(255,u'W')) {
            assert(fits);
            if(unsigned(height)<minimum) minimum=height;
            if(count) for(float baseline : {18.5f,30.0f,44.4f}) {
                struct Metrics {
                    float baseline;
                    float GetFontBaseLine() { return baseline; }
                    float GetFontHeight() { return 37.0f; }
                } metrics{baseline};
                auto* font=&metrics;
                const unsigned lineCount=count;
''' + baseline_layout + r'''
                const float top=firstLine-height*font->GetFontBaseLine()/font->GetFontHeight();
                const float bottom=top+blockHeight;
                assert(top>=73.999f && bottom<=187.001f);
                assert(std::abs((top+bottom)*.5f-130.5f)<.001f);
            }
        }
        std::u16string combined;
        for(unsigned row=0;row<count && row<16;++row) {
            assert(measure(lines[row],height)<=216.001f);
            combined+=lines[row];
        }
        assert(content(combined)==content(text));
    }
    assert(measure(u"X HIDE TUTORIALS",13)<=160);
    assert(measure(u"A CONTINUE",13)<=100);
    printf("PASS: %zu reflow cases; every character preserved, 216 px width, 113 px text area, readable authored text and single-line footer. Minimum authored height: %u px.\n",fixtures.size(),minimum);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-handheld-text-') as temporary:
    path = Path(temporary)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I', str(root),
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print(f'Retail check: {retail} English tutorial entries with actual font advances.' if retail else 'Retail data absent; synthetic fixtures only.')
