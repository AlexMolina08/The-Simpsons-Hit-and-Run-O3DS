#!/usr/bin/env python3
"""Generate an immutable lookup index for the exact native texture bundle."""
import argparse,hashlib,json,struct,zlib,re
from pathlib import Path

def index_bundle(bundle,header,key_bytes=32,runtime_name="native143.bundle",allow_etc1=False):
 if key_bytes not in (16,32):raise ValueError("Native bundle keys must use 16 or 32 bytes")
 if not re.fullmatch(r"[A-Za-z0-9_-]{1,48}\.bundle",runtime_name):raise ValueError("Runtime bundle name must be a simple basename")
 entries=[]
 with bundle.open('rb') as f:
  magic,count=struct.unpack('<II',f.read(8))
  if magic!=0x314e4233 or not 0<count<=32768:raise ValueError('Unsupported native bundle')
  for _ in range(count):
   name=f.read(68).decode('ascii');size,crc=struct.unpack('<II',f.read(8))
   if len(name)!=68 or not name.endswith('.ntx') or any(c not in '0123456789abcdef' for c in name[:64]):raise ValueError('Invalid payload key')
   if not 108<=size<=8*1024*1024:raise ValueError('Invalid payload size')
   position=f.tell();body=f.read(size)
   if len(body)!=size:raise ValueError('Truncated payload')
   if zlib.crc32(body)!=crc:raise ValueError('Payload CRC mismatch')
   words=struct.unpack_from('<11I',body)
   exact=words[:2]==(0x3158544e,1)
   etc1=allow_etc1 and words[:2]==(0x3158544e,2) and words[7]==4
   if not exact and not etc1:raise ValueError('Unsupported payload header')
   if etc1:
    width,height,levels=words[5],words[6],words[8]
    if (not 8<=width<=1024 or width&(width-1) or not 8<=height<=1024 or height&(height-1) or
        levels>7 or min(width,height)>>levels<8):raise ValueError('Invalid ETC1 mip dimensions')
    expected=sum((width>>level)*(height>>level)//2 for level in range(levels+1))
    if words[10]!=expected or size!=108+expected:raise ValueError('Invalid ETC1 payload size')
   if hashlib.sha256(body[:76]).hexdigest()!=name[:64] or hashlib.sha256(body[108:]).digest()!=body[76:108]:raise ValueError('Payload content identity mismatch')
   entries.append((bytes.fromhex(name[:64]),position,size))
  if f.read(1):raise ValueError('Trailing bundle bytes')
 entries.sort()
 if len({r[0] for r in entries})!=count:raise ValueError('Duplicate payload key')
 if len({r[0][:key_bytes] for r in entries})!=count:raise ValueError('Ambiguous native key prefix')
 digest=hashlib.file_digest(bundle.open('rb'),'sha256').hexdigest()
 lines=['// Generated from a validated native bundle. Do not edit.',
        'namespace picaNativeBundle {',f'static const char BundlePath[]="{runtime_name}";',f'static const unsigned BundleBytes={bundle.stat().st_size}u;',
        f'static const char BundleSHA256[]="{digest}";', f'static const IndexEntry<{key_bytes}> Entries[]={{']
 for key,offset,size in entries:lines.append('    {{'+','.join(str(b) for b in key[:key_bytes])+'},'+str(offset)+'u,'+str(size)+'u},')
 lines+=['};',f'static const unsigned EntryCount={count}u;','}']
 header.parent.mkdir(parents=True,exist_ok=True);header.write_text('\n'.join(lines)+'\n')
 result={'bundle':str(bundle.resolve()),'runtime_name':runtime_name,'bytes':bundle.stat().st_size,'sha256':digest,'entries':count,'key_bytes':key_bytes,'index_bytes':count*(key_bytes+8),'header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'allow_etc1':allow_etc1}
 header.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n');return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('bundle',type=Path);p.add_argument('--header',type=Path,required=True);p.add_argument('--key-bytes',type=int,choices=(16,32),default=32);p.add_argument('--runtime-name',default='native143.bundle');p.add_argument('--allow-etc1',action='store_true',help='Explicitly accept version-2 opaque-world ETC1 candidate payloads');a=p.parse_args();print(json.dumps(index_bundle(a.bundle,a.header,a.key_bytes,a.runtime_name,a.allow_etc1)))
if __name__=='__main__':main()
