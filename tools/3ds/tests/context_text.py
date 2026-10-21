#!/usr/bin/env python3
"""Validate retail contextual text with the shipped font advances and production reflow."""
from pathlib import Path
import json
import re
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[3]
data = root / 'sdcard/3ds/simpsons/art/frontend'
bible = data / 'scrooby/resource/txtbible/srr2.p3d'
if not bible.is_file():
    raise SystemExit('Retail data required: run against the installed game assets.')
b = bible.read_bytes()
a = 12 + struct.unpack_from('<I', b, 16)[0] + 12
a += 1 + b[a]
assert b[a] == ord('E')
count, modulo, _ = struct.unpack_from('<III', b, a + 1)
entries = {}
for i in range(count):
    key = struct.unpack_from('<I', b, a + 13 + i * 4)[0]
    start = a + 13 + count * 8 + struct.unpack_from('<I', b, a + 13 + count * 4 + i * 4)[0]
    end = start
    while b[end:end + 2] != b'\0\0':
        end += 2
    entries[key] = b[start:end].decode('utf-16le')

def lookup(key):
    value = 0
    for character in key:
        value = (64 * value + ord(character)) % modulo
    return entries.get(value)

def metrics(name):
    b = (data / f'scrooby2/resource/_ingame/{name}.p3d').read_bytes()
    a = 12
    _, header, total = struct.unpack_from('<III', b, a)
    cursor = a + 16
    cursor += 1 + b[cursor]
    cursor += 1 + b[cursor]
    height = struct.unpack_from('<4f', b, cursor)[2]
    cursor = a + header
    widths = {}
    while cursor < a + total:
        kind, _, size = struct.unpack_from('<III', b, cursor)
        if kind == 0x22001:
            for i in range(struct.unpack_from('<I', b, cursor + 12)[0]):
                record = cursor + 16 + i * 40
                widths[struct.unpack_from('<I', b, record + 36)[0]] = struct.unpack_from('<f', b, record + 32)[0] / height
        cursor += size
    assert widths
    return widths

fonts = [metrics('font0_16'), metrics('font1_14')]
# width, requested glyph height, visible lines, top, reserved bottom.
areas = {
    'MissionInfo': (284, 17, 8, 48, 211),
    'MissionObjective': (272, 16, 7, 54, 186),
    'MissionFailureReason': (288, 16, 2, 42, 79),
    'MissionFailureHint': (288, 14, 2, 80, 117),
    'CardDescription': (288, 15, 7, 82, 211),
    'CardEpisode': (288, 15, 2, 40, 79),
}
fixtures = []
summary = {}
for path in (data / 'scrooby2/pages').glob('*.pag'):
    for element in ET.parse(path).iter():
        if element.tag not in ('Text', 'MultiText'):
            continue
        name = element.get('Name')
        area = areas.get(name)
        if name == 'Message' and path.stem == 'Hud':
            area = (237, 13, 3, 190, 237)
        elif name == 'Message' and path.stem in ('GenericPrompt', 'ErrorPrompt', 'Message'):
            area = (288, 16, 4, 42, 122)
        if not area:
            continue
        for string in element.iter('String'):
            key = string.get('stringId')
            text = lookup(key) if key else None
            if not text:
                continue
            # Match the port's real control-caption substitutions.
            overrides = dict(re.findall(r'\{"([^"]+)", "([^"\n]*)"\}',
                (root / 'libs/scrooby/src/3dscontrols.h').read_text()))
            text = overrides.get(key, text).replace('\\', ' ')
            assert len(text) < 512, (key, len(text))
            fixtures.append((key, text, area))
            if name == 'Message' and path.stem in ('GenericPrompt', 'ErrorPrompt'):
                # The narrowest frontend TV prompt keeps every retained line.
                fixtures.append((key + '_TV', text, (224, 16, 2, 56, 102)))
            summary[path.stem] = summary.get(path.stem, 0) + 1
assert len(fixtures) > 500, summary
literal = lambda text: 'u' + json.dumps(text, ensure_ascii=True)
font_maps = ','.join('{' + ','.join(f'{{{c},{w:.9f}f}}' for c, w in font.items()) + '}' for font in fonts)
rows = ','.join('{' + literal(key) + ',' + literal(text) + ',' + ','.join(map(str, area)) + '}' for key, text, area in fixtures)
program = r'''
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "code/presentation/gui/utility/handheldtext.h"
struct Fixture {const char16_t* key;const char16_t* text;float width,height;unsigned visible;float top,bottom;};
std::vector<std::map<unsigned,float>> fonts={''' + font_maps + r'''};
std::vector<Fixture> fixtures={''' + rows + r'''};
std::u16string content(const char16_t* text){std::u16string result;while(*text){if(*text!=' ' && *text!='\n' && *text!='\r')result+=*text;++text;}return result;}
int main(){
 unsigned scroll=0;unsigned worst=0;
 for(auto& advances:fonts)for(const auto& test:fixtures){
  const auto measure=[&](const char16_t* text,float height){float w=0;while(*text){auto i=advances.find(*text++);w+=i==advances.end()?0.5f:i->second;}return w*height;};
  char16_t lines[16][256]={};unsigned count=0;float size=test.height;
  for(;size>=12;--size){
   count=HandheldText::Wrap(test.text,lines,[&](const char16_t* line){return measure(line,size)<=test.width;});
   if(count<=test.visible)break;
  }
  if(size<12)size=12;
  if(std::u16string(test.key).find(u"MISSION_OBJECTIVE")==0 && test.width==272)assert(count<=test.visible);
  assert(count<=16); // No retail text may disappear beyond the retained scroll buffer.
  if(count>worst)worst=count;
  std::u16string joined;
  for(unsigned row=0;row<count;++row){assert(measure(lines[row],size)<=test.width+0.001f);joined+=lines[row];}
  assert(content(joined.c_str())==content(test.text));
  unsigned shown=count<test.visible?count:test.visible;
  assert(test.top+shown*size+(shown?shown-1:0)*3<=test.bottom);
  // Visit every scrolling window; final line must remain reachable.
  if(count>shown){++scroll;std::u16string seen;for(unsigned offset=0;offset<=count-shown;++offset)seen+=lines[offset+shown-1];assert(seen.find(lines[count-1])!=std::u16string::npos);}
 }
 printf("PASS: %zu retail contextual strings across both original fonts; no lost characters, width overflow or footer overlap. %u scroll cases, maximum %u retained lines.\n",fixtures.size(),scroll,worst);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-context-text-') as temporary:
    path = Path(temporary)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I', str(root),
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Retail sources:', ', '.join(f'{key}: {value}' for key, value in sorted(summary.items())))
