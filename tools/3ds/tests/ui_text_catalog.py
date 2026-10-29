#!/usr/bin/env python3
"""Audit actual Scrooby project text references against localized retail bibles and fonts."""
import argparse
import json
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--english-game', type=Path, default=root/'sdcard/3ds/simpsons')
parser.add_argument('--french-game', type=Path)
parser.add_argument('--spanish-game', type=Path)
parser.add_argument('--report', type=Path)
options = parser.parse_args()
controls = (root/'libs/scrooby/src/3dscontrols.h').read_text()

def entries(table):
    body = controls.split('static const Entry ' + table + '[] = {', 1)[1].split('};', 1)[0]
    return {json.loads('"'+key+'"'): json.loads('"'+text+'"')
            for key, text in re.findall(r'\{"([^"\n]*)", "([^"\n]*)"\}', body)}

corrections = entries('frenchCorrections')
keycaps = {'[ESC]':'B', '[ÉCHAP]':'B', '[ECHAP]':'B', '[ENTER]':'A', '[ENTRÉE]':'A', '[ENTREE]':'A'}

def get_bible(path, language):
    data=path.read_bytes()
    pos=12+struct.unpack_from('<I',data,16)[0]
    while pos+12<=len(data):
        kind,header,total=struct.unpack_from('<III',data,pos)
        if header<12 or total<header or pos+total>len(data): raise ValueError(f'Invalid chunk in {path}')
        cursor=pos+12;cursor+=1+data[cursor]
        if kind==0x1800e and data[cursor]==ord(language):
            count,modulo,size=struct.unpack_from('<III',data,cursor+1)
            hashes=cursor+13;offsets=hashes+count*4;strings=offsets+count*4
            if not modulo or strings+size>pos+total: raise ValueError(f'Invalid text table in {path}')
            result={}
            for index in range(count):
                key=struct.unpack_from('<I',data,hashes+index*4)[0]
                offset=struct.unpack_from('<I',data,offsets+index*4)[0]
                if offset%2 or offset+2>size: raise ValueError(f'Invalid text offset in {path}')
                start=strings+offset;end=start
                while end+2<=strings+size and data[end:end+2]!=b'\0\0':end+=2
                if end+2>strings+size: raise ValueError(f'Unterminated text in {path}')
                result[key]=data[start:end].decode('utf-16le')
            return result,modulo
        pos+=total
    raise ValueError(f'{path} has no {language} language table')

def references(folder):
    result={}
    for file in sorted(folder.glob('*.p3d')):
        data=file.read_bytes()
        def visit(start,end,parents):
            pos=start
            while pos<end:
                if pos+12>end: raise ValueError(f'Truncated chunk in {file}')
                kind,header,total=struct.unpack_from('<III',data,pos)
                if header<12 or total<header or pos+total>end: raise ValueError(f'Invalid chunk in {file}')
                name=''
                if kind in (0x18001,0x18002,0x18003,0x18004,0x18007):
                    length=data[pos+12];name=data[pos+13:pos+13+length].rstrip(b'\0').decode('latin1')
                if kind==0x1800b:
                    cursor=pos+12;length=data[cursor];cursor+=1
                    bible=data[cursor:cursor+length].rstrip(b'\0').decode('latin1');cursor+=length
                    length=data[cursor];cursor+=1
                    key=data[cursor:cursor+length].rstrip(b'\0').decode('latin1')
                    if bible=='srr2': result.setdefault(key,[]).append(file.name+':'+('/'.join(parents)))
                if header<total:visit(pos+header,pos+total,parents+([name] if name else []))
                pos+=total
        visit(0,len(data),[])
    if not result: raise ValueError(f'No Scrooby text references in {folder}')
    return result

def font_glyphs(folder):
    all_glyphs=[]
    for path in folder.rglob('font*.p3d'):
        data=path.read_bytes();base=12
        if len(data)<base+12:continue
        _,header,total=struct.unpack_from('<III',data,base);pos=base+header
        if total<header or base+total>len(data):continue
        glyphs=set()
        while pos+12<=base+total:
            kind,head,size=struct.unpack_from('<III',data,pos)
            if size<head or head<12 or pos+size>base+total:break
            if kind==0x22001:
                count=struct.unpack_from('<I',data,pos+12)[0]
                if pos+16+count*40>pos+size:raise ValueError(f'Invalid font {path}')
                glyphs={struct.unpack_from('<I',data,pos+16+i*40+36)[0] for i in range(count)}
            pos+=size
        if glyphs:all_glyphs.append(glyphs)
    if not all_glyphs:raise ValueError(f'No retail fonts in {folder}')
    return set.intersection(*all_glyphs),len(all_glyphs)

glyphs,font_count=font_glyphs(options.english_game/'art/frontend/scrooby2/resource')
inputs=[('E',options.english_game,'entries')]
if options.french_game:inputs.append(('F',options.french_game,'frenchEntries'))
if options.spanish_game:inputs.append(('S',options.spanish_game,'spanishEntries'))
report={'font_count':font_count,'languages':{}}
known_missing=set()
known_empty={'SPACE','ORDINAL_1ST','ORDINAL_2ND','ORDINAL_3RD','ORDINAL_4TH'}
for language,game,table in inputs:
    ui=game/'art/frontend/scrooby'
    refs=references(ui)
    bible,modulo=get_bible(ui/'resource/txtbible/srr2.p3d',language)
    replacements=entries(table)
    missing=[];empty=[];bad=[]
    for key,locations in refs.items():
        hashed=0
        for char in key:hashed=((64*hashed+ord(char)) & 0xffffffff)%modulo
        if hashed not in bible:
            missing.append({'key':key,'locations':locations});continue
        text=bible[hashed]
        if not text.strip():empty.append({'key':key,'locations':locations});continue
        text=replacements.get(key,text)
        if language=='F':text=corrections.get(text,text)
        text=keycaps.get(text,text)
        absent=sorted({c for c in text if ord(c) not in glyphs and c not in '\\\n\r'})
        if absent:bad.append({'key':key,'characters':''.join(absent),'locations':locations})
    report['languages'][language]={'references':len(refs),'bible_strings':len(bible),'missing':missing,'empty':empty,'missing_glyphs':bad}
    print(f'{language}: {len(refs)} project references; {len(missing)} missing, {len(empty)} empty, {len(bad)} unsupported glyph entries')
    for item in missing+bad:print(' ',item)
if options.report:
    options.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
for language,data in report['languages'].items():
    unexpected_missing=[x for x in data['missing'] if x['key'] not in known_missing]
    unexpected_empty=[x for x in data['empty'] if x['key'] not in known_empty]
    if unexpected_missing or unexpected_empty or data['missing_glyphs']:
        raise SystemExit(f'{language}: unresolved text references or font glyphs; see audit output')
print('PASS: actual Scrooby project references use available glyphs; known blank strings are reported for review.')
