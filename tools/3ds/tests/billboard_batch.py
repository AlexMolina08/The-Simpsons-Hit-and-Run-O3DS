#!/usr/bin/env python3
"""Compare production local batching against original ordered group callbacks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/p3d/billboardobject.cpp').read_text()
# Matrix edits during these CPU-only quad calculations must not flush or submit.
backend=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=backend.index('void picaContext::LoadHardwareMatrix(');a=backend.index('{',a);b=backend.index('}',a)
assert not backend[a+1:b].strip(), 'Re-audit matrix and stream ordering before batching on a changed backend'
def method(signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
original='void '+method('tBillboardQuadGroup::Display()')
batch=method('bool tBillboardQuadGroup::DisplayBatch(')
display_source=(root/'libs/pure3d/p3d/displaylist.cpp').read_text()
a=display_source.index('void DisplayList::Display(void)');b=display_source.index('\nvoid DisplayList::Add(',a);display=display_source[a:b]
fixture=r'''
#include <cassert>
#include <vector>
#include <array>
#include <typeinfo>
#include <p3d/billboardbatch.hpp>
#define O3_RECUR(x) ((void)0)
#define P3DASSERT assert
namespace p3dBillboardBatch {bool enabled=true;unsigned batches=0,groups=0,avoided=0,reservationFailures=0;}
struct Matrix{unsigned value=0;bool operator==(const Matrix& b)const{return value==b.value;}};
namespace rmt{using Matrix=::Matrix;}
struct Stack{Matrix current;std::vector<Matrix> saved;void Push(){saved.push_back(current);}void PushIdentity(){saved.push_back(current);current={};}void Load(const Matrix& m){current=m;}void LoadIdentity(){current={};}void Pop(){assert(!saved.empty());current=saved.back();saved.pop_back();}};
struct Camera{Matrix GetCameraToWorldMatrix(){return {101};}Matrix GetWorldToCameraMatrix(){return {202};}};
struct View{Camera camera;Camera* GetCamera(){return &camera;}};
struct Context{Stack* stack;View view;Matrix* GetWorldMatrix(){return &stack->current;}View* GetView(){return &view;}};
enum pddiCompareMode{PDDI_COMPARE_ALWAYS=0,PDDI_COMPARE_LESS=1};
struct Token{unsigned id,world,camera,inverse,calculated;float intensity;bool zwrite;pddiCompareMode compare;bool operator==(const Token& b)const{return id==b.id&&world==b.world&&camera==b.camera&&inverse==b.inverse&&calculated==b.calculated&&intensity==b.intensity&&zwrite==b.zwrite&&compare==b.compare;}};
struct pddiPrimStream{unsigned capacity=0;std::vector<Token> tokens;};
struct Device{bool write=true,rejectQuad=false;unsigned draws=0;pddiCompareMode compare=PDDI_COMPARE_LESS;pddiPrimStream stream;std::vector<Token> output;
 bool GetZWrite(){return write;}pddiCompareMode GetZCompare(){return compare;}void SetZWrite(bool x){write=x;}void SetZCompare(pddiCompareMode x){compare=x;}
 pddiPrimStream* BeginPrims(void*,int,int,unsigned n){stream={n,{}};return &stream;}void EndPrims(pddiPrimStream* s){assert(s==&stream);assert(s->tokens.size()<=s->capacity);++draws;output.insert(output.end(),s->tokens.begin(),s->tokens.end());}};
namespace p3d{Stack* stack;Context* context;Device* pddi;}
pddiPrimStream* picaBeginQuadStream(void* shader,unsigned n){return p3d::pddi->rejectQuad?nullptr:p3d::pddi->BeginPrims(shader,0,0,n);}
const int PDDI_PRIM_TRIANGLES=0,PDDI_V_CT=0;
struct tColour{unsigned alpha=255,red=255,green=128,blue=64;unsigned Alpha()const{return alpha;}unsigned Red()const{return red;}unsigned Green()const{return green;}unsigned Blue()const{return blue;}};
struct tBillboardQuad{bool visible=true;unsigned id=0,calculations=0,displays=0,calculated=0;tColour colour;virtual ~tBillboardQuad()=default;
 bool GetVisibility(){return visible;}tColour GetColour(){return colour;}
 void Calculate(){++calculations;calculated=p3d::stack->current.value;}
 virtual void Display(pddiPrimStream* s,const Matrix& w,const Matrix& c,const Matrix& inv,float bias){assert(s);assert(p3d::stack->current.value==0);++displays;s->tokens.push_back({id,w.value,c.value,inv.value,calculated,bias,p3d::pddi->write,p3d::pddi->compare});}};
struct SpecialQuad:tBillboardQuad{};
struct Shader{void* GetShader(){return this;}};
struct tName{static unsigned MakeUID(const char*){return 999;}};
struct tBillboardQuadGroup;
struct BillboardQuadManager{static bool sEnabled,sCollectPass;void Add(tBillboardQuadGroup*){assert(false);}};bool BillboardQuadManager::sEnabled=false,BillboardQuadManager::sCollectPass=false;
BillboardQuadManager* GetBillboardQuadManager(){static BillboardQuadManager m;return &m;}
struct Baked{void Display(pddiPrimStream*){assert(false);}};
struct DrawableBase{virtual ~DrawableBase()=default;virtual void Display()=0;};
struct tBillboardQuadGroup:DrawableBase{Shader* shader=nullptr;bool zTest=true,zWrite=false;int occlusionMode=0,numQuads=0;float intensityBias=1;std::vector<tBillboardQuad*> quads;struct{int mUseSize=0;Baked operator[](unsigned){return {};}}mTempQuads;
 Shader* GetShader(){return shader;}bool GetZTest(){return zTest;}bool GetZWrite(){return zWrite;}int GetOcclusion(){return occlusionMode;}unsigned GetUID(){return 0;}virtual ~tBillboardQuadGroup()=default;virtual void Display();static bool DisplayBatch(tBillboardQuadGroup* const*,const Matrix* const*,unsigned);};
struct SpecialGroup:tBillboardQuadGroup{};
struct DisplayList{struct Entry{DrawableBase* draw;Matrix objectToView;};Entry entries[64];Entry* toSort[64];unsigned nDrawablesEndPos=0,purged=0;void Sort(){}void Purge(){++purged;}void Display(void);};
struct Effect:DrawableBase{tBillboardQuad* target;unsigned id;void Display()override{target->visible=!target->visible;p3d::pddi->output.push_back({id,p3d::stack->current.value,0,0,0,0,p3d::pddi->write,p3d::pddi->compare});}};
ORIGINAL
BATCH
DISPLAY
int main(){
 Stack stack;Device device;Context context{&stack,{}};p3d::stack=&stack;p3d::pddi=&device;p3d::context=&context;Shader shader,other;
 unsigned seed=713,compared=0;
 for(unsigned trial=0;trial<4000;++trial){
  std::array<tBillboardQuadGroup,16> groups;std::array<std::array<tBillboardQuad,4>,16> quads;
  tBillboardQuadGroup* items[16];Matrix matrices[16];const Matrix* views[16];
  unsigned count=2+trial%15;
  for(unsigned i=0;i<count;++i){items[i]=&groups[i];matrices[i]={i+10};views[i]=&matrices[i];auto& g=groups[i];g.shader=&shader;g.numQuads=1+(i+trial)%4;g.zWrite=trial%2;g.zTest=trial%3;g.intensityBias=float(i+1)/16;
   for(int q=0;q<g.numQuads;++q){seed=seed*1664525u+1013904223u;auto& v=quads[i][q];v.id=i*4+q;v.visible=seed%5!=0;v.colour.alpha=seed%7?255:0;if(seed%11==0)v.colour.red=v.colour.green=v.colour.blue=0;g.quads.push_back(&v);}
  }
  device.rejectQuad=false;device.write=true;device.compare=PDDI_COMPARE_LESS;device.draws=0;device.output.clear();
  for(unsigned i=0;i<count;++i){stack.Load(matrices[i]);groups[i].Display();}
  auto expected=device.output;unsigned beforeDraws=device.draws;Matrix expectedMatrix=stack.current;
  for(auto& group:quads)for(auto& q:group)q.calculations=q.displays=0;
  device.output.clear();device.draws=0;stack.current={777};
  if(tBillboardQuadGroup::DisplayBatch(items,views,count)){
   assert(device.output==expected&&device.write&&device.compare==PDDI_COMPARE_LESS);
   assert(stack.current==expectedMatrix&&stack.saved.empty());assert(device.draws==1&&beforeDraws>=2);++compared;
   for(auto& group:quads)for(auto& q:group)assert(q.calculations==q.displays&&q.displays<=1);
  }else assert(device.output.empty()&&device.draws==0&&stack.current.value==777);
  // Every rejected preflight preserves original callbacks and graphics state.
  auto reject=[&](){device.output.clear();device.draws=0;stack.current={888};assert(!tBillboardQuadGroup::DisplayBatch(items,views,count));assert(device.output.empty()&&device.draws==0&&stack.current.value==888&&stack.saved.empty()&&device.write&&device.compare==PDDI_COMPARE_LESS);};
  groups[1].shader=&other;reject();groups[1].shader=&shader;
  groups[1].occlusionMode=1;reject();groups[1].occlusionMode=0;
  groups[1].zWrite=!groups[1].zWrite;reject();groups[1].zWrite=!groups[1].zWrite;
  const Matrix* savedView=views[0];views[0]=nullptr;reject();views[0]=savedView;
  SpecialQuad special;auto* saved=groups[0].quads[0];groups[0].quads[0]=&special;reject();groups[0].quads[0]=saved;
  SpecialGroup foreign;static_cast<tBillboardQuadGroup&>(foreign)=groups[0];auto* savedGroup=items[0];items[0]=&foreign;reject();items[0]=savedGroup;
  device.rejectQuad=true;reject();device.rejectQuad=false;
  p3dBillboardBatch::enabled=false;reject();p3dBillboardBatch::enabled=true;
 }
 for(unsigned trial=0;trial<100;++trial){
  tBillboardQuadGroup groups[40];tBillboardQuad quads[40];Effect effect;effect.target=&quads[24];effect.id=9999;
  DisplayList list;unsigned at=0;
  for(unsigned i=0;i<40;++i){
   if(i==20){list.entries[at]={&effect,{900}};list.toSort[at]=&list.entries[at];++at;}
   auto& g=groups[i];g.shader=(i%11==0)?&other:&shader;g.zWrite=trial%2;g.zTest=trial%3;g.numQuads=1;quads[i].id=i;g.quads.push_back(&quads[i]);
   list.entries[at]={&g,{i+10}};list.toSort[at]=&list.entries[at];++at;
  }
  list.nDrawablesEndPos=at;p3dBillboardBatch::enabled=false;device.output.clear();stack.current={777};quads[24].visible=true;list.Display();auto expected=device.output;
  assert(stack.current.value==777&&list.purged==1&&stack.saved.empty());
  p3dBillboardBatch::enabled=true;device.output.clear();quads[24].visible=true;list.Display();
  assert(device.output==expected&&stack.current.value==777&&stack.saved.empty()&&list.purged==2);
 }
 assert(compared>3000&&p3dBillboardBatch::reservationFailures>0);
}
'''.replace('ORIGINAL',original).replace('BATCH',batch).replace('DISPLAY',display)
with tempfile.TemporaryDirectory(prefix='srr-billboard-batch-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_QUAD_STREAM','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: 4000 ordered production group comparisons and 100 production DisplayList integrations with interleaved effect mutations; exact callback arguments, visibility/color filtering, depth and matrix restoration; foreign groups/quads, mixed state, disabled mode and reservation failure retain original behavior.')
