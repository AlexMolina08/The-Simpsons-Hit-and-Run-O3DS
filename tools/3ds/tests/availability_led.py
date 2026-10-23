#!/usr/bin/env python3
"""Exercise the production availability LED path without changing display mode."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
display = (root / 'libs/pure3d/pddi/pica/display.cpp').read_text()
begin = display.index('static bool s_indicatorPending')
helper = display[begin:display.index('\n#endif\n\n#ifdef SRR2_OLD3DS_NATIVE_BENCH', begin)]
begin = display.index('void picaDisplay::EndFrame(void)')
end_frame = display[begin:display.index('\nvoid picaDisplay::SwapBuffers', begin)]
context = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()
begin = context.index('static volatile bool s_picaResumed')
resume = context[begin:context.index('\nstatic picaContext*', begin)]
fixture = r'''
#include <cassert>
#include <cstddef>
using u8=unsigned char;using u64=unsigned long long;using Result=int;
#define R_SUCCEEDED(value) ((value)>=0)
#define R_FAILED(value) ((value)<0)
namespace Graphics3DS {
 enum Model {Old3DS,Old3DSXL,New3DS,Old2DS,New3DSXL,New2DSXL,Unknown};
 Model model=Old3DS;Model SystemModel(){return model;}
 bool Stereo();
}
bool wide=true,stereo=false,modeSubmitted=false,modeConsumed=false,queueDone=false;
bool Graphics3DS::Stereo(){return stereo;}
unsigned initializes=0,closes=0,writes=0,waits=0,lcdOpens=0,lcdCloses=0;
Result initResult=0,writeResult=0;
struct SharedConfig {unsigned char led_3d=0;} sharedConfig;
auto* OS_SharedConfig=&sharedConfig;
Result gspLcdInit(){++lcdOpens;return 0;}
void gspLcdExit(){++lcdCloses;}
Result GSPLCD_SetLedForceOff(bool disabled){sharedConfig.led_3d=disabled;return 0;}
void C3Di_RenderQueueWaitDone(){assert(modeSubmitted);queueDone=true;++waits;}
void C3D_FrameSync(){assert(queueDone);modeConsumed=true;}
Result mcuHwcInit(){assert(modeSubmitted);++initializes;return initResult;}
void mcuHwcExit(){++closes;}
Result MCUHWC_WriteRegister(u8 address,const void* value,unsigned size){
 assert(address==0x2c&&size==1&&*static_cast<const u8*>(value)==1);
 assert(wide&&!stereo&&sharedConfig.led_3d);++writes;return writeResult;
}
int g_picaInFrame=1;unsigned g_picaPresentedFrames=0,g_old3dsFramePresented=0;
double g_picaFrameSubmitMs=0;
const unsigned GX_CMDLIST_FLUSH=1,SYSCLOCK_ARM11=268123480;
namespace Old3dsCadence {void Submit(unsigned){}}
u64 fakeTick=0;u64 svcGetSystemTick(){return fakeTick;}
void C3D_FrameEnd(unsigned flags){assert(flags==GX_CMDLIST_FLUSH);modeSubmitted=true;}
struct picaDisplay {
 bool inFrame=true,stereoThisFrame=false;
 void FinishTopSegment(){}void EndFrame();
 void Frame(){fakeTick+=SYSCLOCK_ARM11/10+1;inFrame=true;g_picaInFrame=1;EndFrame();}
 void Apply(){Frame();Frame();}
};
struct aptHookCookie {};
enum APT_HookType {APTHOOK_ONRESTORE,APTHOOK_ONWAKEUP,APTHOOK_ONSUSPEND,APTHOOK_ONSLEEP,APTHOOK_ONEXIT};
'''
main = r'''
int main(){
 (void)s_picaAptCookie;
 picaDisplay display;
 picaRequest3DIndicator();assert(initializes==0&&writes==0);
 display.inFrame=false;display.EndFrame();assert(initializes==0);
 display.Frame();assert(writes==0&&sharedConfig.led_3d);
 display.Frame();assert(writes==1&&initializes==1&&closes==1&&waits==0);
 assert(wide&&!stereo);
 for(unsigned i=0;i<10000;++i)display.Frame();
 assert(writes==1&&initializes==1&&waits==0); // No steady-frame IPC or extra waits.
 picaAptHook(APTHOOK_ONRESTORE,nullptr);display.Frame();assert(writes==2);
 picaAptHook(APTHOOK_ONWAKEUP,nullptr);display.Frame();assert(writes==3);
 picaAptHook(APTHOOK_ONSUSPEND,nullptr);display.Frame();assert(writes==3&&!sharedConfig.led_3d);
 initResult=-1;picaRequest3DIndicator();display.Apply();
 assert(writes==3&&closes==3&&wide&&!stereo);
 display.Frame();assert(initializes==4); // Retry only at the next lifecycle event.
 initResult=0;writeResult=-1;picaRequest3DIndicator();display.Apply();
 assert(writes==4&&closes==4&&wide&&!stereo);
 for(auto model:{Graphics3DS::New3DS,Graphics3DS::New3DSXL,Graphics3DS::Old2DS,
                 Graphics3DS::New2DSXL,Graphics3DS::Unknown}){
  Graphics3DS::model=model;picaRequest3DIndicator();display.Apply();
  assert(writes==4&&initializes==5);
 }
 Graphics3DS::model=Graphics3DS::Old3DSXL;picaRequest3DIndicator();display.Apply();
 assert(writes==5&&closes==5&&wide&&!stereo);
 picaRelease3DIndicator();assert(!sharedConfig.led_3d);
 writeResult=0;
 stereo=true;wide=false;
 const unsigned stereoLcdOpens=lcdOpens;
 picaRequest3DIndicator();display.Apply();
 assert(!sharedConfig.led_3d && writes==5 && lcdOpens==stereoLcdOpens);
 picaAptHook(APTHOOK_ONSUSPEND,nullptr);
 picaAptHook(APTHOOK_ONRESTORE,nullptr);display.Apply();
 picaAptHook(APTHOOK_ONWAKEUP,nullptr);display.Apply();
 assert(!sharedConfig.led_3d && writes==5 && lcdOpens==stereoLcdOpens);
 stereo=false;wide=true;picaRequest3DIndicator();display.Frame();
 assert(sharedConfig.led_3d && writes==5);
 stereo=true;wide=false;picaRequest3DIndicator();
 assert(!sharedConfig.led_3d && !s_indicatorPending && !s_indicatorOwnsPolicy);
 display.Apply();assert(writes==5);
 stereo=false;wide=true;picaRequest3DIndicator();display.Apply();
 assert(sharedConfig.led_3d && writes==6 && s_indicatorOwnsPolicy);
 stereo=true;wide=false;picaRequest3DIndicator();display.Apply();
 assert(!sharedConfig.led_3d && writes==6 && !s_indicatorOwnsPolicy);
 assert(lcdOpens==lcdCloses);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-led-') as directory:
    folder = Path(directory)
    source = folder / 'test.cpp'
    source.write_text('#include <initializer_list>\n' + fixture + helper + resume + end_frame + main)
    binary = folder / 'test'
    subprocess.run(['clang++', '-std=c++17', '-DSRR2_OLD_3DS', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
rsf = (root / 'tools/3ds/cia/game.rsf').read_text()
services = [line.strip()[2:] for line in rsf.split('ServiceAccessControl:', 1)[1]
            .split('SystemControlInfo:', 1)[0].splitlines() if line.strip().startswith('- ')]
assert 'mcu::HWC' in services and 'gsp::Lcd' in services and len(services) <= 32
print('PASS: mono LED applies after presentation, 10000 steady frames have no LED IPC/waits, stereo and HOME/wake restore system control without MCU writes, pending writes cancel on stereo entry, Quality returns, CIA permission fits.')
