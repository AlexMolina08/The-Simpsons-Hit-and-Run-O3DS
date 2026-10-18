#ifndef PICA_NATIVE_COMMANDS_H
#define PICA_NATIVE_COMMANDS_H
#include <3ds.h>
#include <string.h>
// One main-thread producer owns the SDK command buffer. The native context
// reserves worst-case room before changing state, then commits its offset once.
typedef struct {u32* data;unsigned count;} picaNativeCommandBlock;
extern picaNativeCommandBlock* g_picaNativeCommandBlock;
static inline void picaNativeCommandAdd(picaNativeCommandBlock* p,u32 header,const u32* values,unsigned count)
{
    if(!count)count=1;
    while(count) {
        const unsigned n=count>256?256:count;
        p->data[p->count++]=values[0];
        p->data[p->count++]=header|((n-1)<<20);
        memcpy(p->data+p->count,values+1,(n-1)*sizeof(u32));
        p->count+=n-1;
        if(p->count&1)p->data[p->count++]=0;
        count-=n;values+=n;
        if(header&BIT(31))header+=n;
    }
}
#endif
