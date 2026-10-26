#!/usr/bin/env python3
"""Pack private UI sprite atlases while retaining every original fallback chunk."""
import argparse,copy,hashlib,io,json,struct
from pathlib import Path
from PIL import Image
from prepare_character_detail import Node,nodes
from prepare_vehicle_reflections import walk,pstring,string
ATLAS=0x03FFAD01
MAGIC=0x31544153

def power(n):return 1<<(n-1).bit_length()
def image(node):
 name,o=pstring(node.body,0);words=list(struct.unpack_from('<7I',node.body,o))
 if len(node.body)!=o+28 or words[0]!=14000 or words[3]!=32 or words[4]!=0 or words[6]!=1:return None
 if len(node.children)!=1 or node.children[0].kind!=0x19002:return None
 raw=node.children[0].body;size,=struct.unpack_from('<I',raw)
 if size!=len(raw)-4 or not raw[4:].startswith(b'\x89PNG\r\n\x1a\n'):return None
 png=raw[4:]
 if png[24]!=8 or png[25] not in (2,6):return None
 with Image.open(io.BytesIO(png)) as decoded:
  if decoded.size!=tuple(words[1:3]):return None
  pixels=decoded.convert('RGBA')
 return name,words,pixels

def prepare(source,destination,extra_budget):
 tree=nodes(source.read_bytes());assert not any(n.kind==ATLAS for n in walk(tree))
 options=[]
 for sprite in walk(tree):
  if sprite.kind!=0x19005:continue
  name,o=pstring(sprite.body,0);o+=8;shader,o=pstring(sprite.body,o)
  width,height,count,border=struct.unpack_from('<4I',sprite.body,o)
  if count<2 or count>64 or border!=1 or not width or not height:continue
  children=[n for n in sprite.children if n.kind==0x19001]
  if len(children)!=count:continue
  images=[image(n) for n in children]
  if any(x is None for x in images):continue
  descriptor=[images[0][1][i] for i in (0,3,4,5,6)]
  if any([x[1][i] for i in (0,3,4,5,6)]!=descriptor for x in images):continue
  # Match the original row-major constructor, including its padded edge quads.
  tiles=[];i=0;y=-border;valid=True
  while y<height-1 and i<count:
   x=-border;row_height=None
   while x<width-1 and i<count:
    _,words,pixels=images[i];tw,th=words[1:3]
    if min(tw,th)<4 or max(tw,th)>256 or tw&(tw-1) or th&(th-1):valid=False;break
    if row_height is not None and th!=row_height:valid=False;break
    row_height=th;tiles.append((x+border,y+border,tw,th,pixels));x+=tw-2*border;i+=1
   if not valid or row_height is None:break
   y+=row_height-2*border
  if not valid or i!=count or x<width-1 or y<height-1:continue
  # Pack complete original tiles, including every replicated border texel.
  rows=[];at=0
  while at<count:
   row=[];start=tiles[at][1]
   while at<count and tiles[at][1]==start:row.append(at);at+=1
   rows.append(row)
  aw=power(max(sum(tiles[i][2] for i in row) for row in rows));ah=power(sum(tiles[row[0]][3] for row in rows))
  if max(aw,ah)>256:continue
  old_pixels=sum(t[2]*t[3] for t in tiles);extra=max(0,aw*ah-old_pixels)*2
  priority=0 if name.lower() in ('radar.png','radartop.png') else 1
  options.append((extra>0,priority,-(count-1)/max(extra,1),name,sprite,o,images,tiles,rows,aw,ah,old_pixels,extra))
 report=[];used=0
 for _,_,_,name,sprite,o,images,tiles,rows,aw,ah,old_pixels,extra in sorted(options,key=lambda x:x[:4]):
  if extra and (name.lower() not in ('radar.png','radartop.png') or used+extra>extra_budget):continue
  canvas=Image.new('RGBA',(aw,ah));layout=[];ay=0
  for row in rows:
   ax=0
   for i in row:
    _,_,tw,th,pixels=tiles[i];canvas.paste(pixels,(ax,ay));layout.append((ax,ay,tw,th));ax+=tw
   ay+=tiles[row[0]][3]
  # Pixel equality, including padding, is checked before writing the candidate.
  for tile,(ax,ay,tw,th) in zip(tiles,layout):assert canvas.crop((ax,ay,ax+tw,ay+th)).tobytes()==tile[4].tobytes()
  packed=io.BytesIO();canvas.save(packed,format='PNG',compress_level=9)
  words=images[0][1][:];words[1:3]=[aw,ah]
  atlas_image=Node(0x19001,string(name[:80]+'.atlas')+struct.pack('<7I',*words),[Node(0x19002,struct.pack('<I',len(packed.getvalue()))+packed.getvalue(),[])])
  body=struct.pack('<5I',MAGIC,1,len(tiles),aw,ah)+b''.join(struct.pack('<4I',*t) for t in layout)
  sprite.children.append(Node(ATLAS,body,[atlas_image]));used+=extra
  report.append(dict(name=name,tiles=len(tiles),atlas=[aw,ah],old_pixels=old_pixels,new_pixels=aw*ah,extra_texture_bytes=extra,original_chunks_retained=True))
 destination.parent.mkdir(parents=True,exist_ok=True);assert not destination.exists();destination.write_bytes(b''.join(n.encode() for n in tree))
 result=dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),output=str(destination),output_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),sprites=len(report),potential_tile_draws_avoided=sum(x['tiles']-1 for x in report),extra_texture_bytes=used,extra_budget_bytes=extra_budget,records=report)
 destination.with_suffix('.atlas.json').write_text(json.dumps(result,indent=2)+'\n');return result

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--extra-texture-kib',type=int,default=0);a=p.parse_args()
 r=prepare(a.source,a.output,a.extra_texture_kib*1024);print(json.dumps({k:v for k,v in r.items() if k!='records'}))
