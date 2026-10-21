#ifndef RADSOUND_CTR_LID_GREETING_HPP
#define RADSOUND_CTR_LID_GREETING_HPP
#include "lidgreetingstate.hpp"
#include <stdio.h>

namespace ctrLidGreeting {
static State state;
static aptHookCookie cookie;
static int channel=-1;
static void* pcm=NULL;
static ndspWaveBuf wave;

static void Stop()
{
    if(channel>=0) ndspChnWaveBufClear(channel);
    if(pcm) { linearFree(pcm);pcm=NULL; }
    memset(&wave,0,sizeof(wave));
}
static void Hook(APT_HookType type,void*)
{
    if(type==APTHOOK_ONSLEEP) state.Sleep();
    else if(type==APTHOOK_ONWAKEUP) state.Wake();
    else if(type==APTHOOK_ONEXIT) state.Clear();
}
static void Initialize()
{
    state.Clear();
    const u64 tick=svcGetSystemTick();state.Seed(unsigned(tick)^unsigned(tick>>32));
    // One shared-allocator channel is reserved, so a full effects mix cannot
    // steal a greeting or make it interrupt an existing game voice.
    channel=AllocChannel();
    aptHook(&cookie,Hook,NULL);
}
static void Shutdown()
{
    aptUnhook(&cookie);state.Clear();Stop();
    if(channel>=0) { FreeChannel(channel);channel=-1; }
}
static void Service(float volume)
{
    const bool wake=state.Consume();
    if(!g_NdspAvailable || channel<0) return;
    if(volume<0) volume=0;
    if(volume>1) volume=1;
    if(pcm && (wave.status==NDSP_WBUF_DONE || volume==0)) Stop();
    if(wake) {
        Stop();
        if(volume==0) return;
        const unsigned clip=state.Next();
        char path[96];snprintf(path,sizeof(path),"romfs:/sound/3ds-lid/%02u.bin",clip);
        FILE* file=fopen(path,"rb");
        if(!file) { snprintf(path,sizeof(path),"sound/3ds-lid/%02u.bin",clip);file=fopen(path,"rb"); }
        if(!file) return;
        struct Header { char magic[4];u32 rate,samples; } header;
        bool valid=fread(&header,sizeof(header),1,file)==1 && !memcmp(header.magic,"LID1",4) &&
            header.rate==24000 && header.samples>0 && header.samples<=72000;
        if(valid) {
            const unsigned bytes=header.samples*2;
            pcm=linearMemAlign(bytes,0x80);
            valid=pcm && fread(pcm,1,bytes,file)==bytes && fgetc(file)==EOF;
            if(valid) {
                DSP_FlushDataCache(pcm,bytes);
                ndspChnReset(channel);ndspChnSetInterp(channel,NDSP_INTERP_LINEAR);
                ndspChnSetRate(channel,float(header.rate));ndspChnSetFormat(channel,NDSP_FORMAT_MONO_PCM16);
                memset(&wave,0,sizeof(wave));wave.data_vaddr=pcm;wave.nsamples=header.samples;
                ndspChnSetPaused(channel,false);ndspChnWaveBufAdd(channel,&wave);
            }
        }
        fclose(file);
        if(!valid) Stop();
    }
    if(pcm) {
        float mix[12]={0};mix[0]=mix[1]=0.6f*volume*volume;
        ndspChnSetMix(channel,mix);
    }
}
}
#endif
