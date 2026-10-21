/* Adapted from Citro3D 1.7.1, upstream commit 9f21cf7 (fincs).
 * Alteration: optional sparse dirty scanning and exact repeated-value suppression.
 * Original license: citro3d_171/LICENSE. Constant loading, restore invalidation,
 * integer/bool uniforms and ever-dirty bookkeeping retain upstream behavior.
 */
#include <citro3d.h>
#include <pddi/pica/citro3d_171/internal.h>
#include <string.h>
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include "nativecommands.h"
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern bool nativeBenchNativeUniforms(void);
#endif
#include <c3d/uniforms.h>
#include "nativeuniformsnapshot.h"
_Static_assert(sizeof(bool)==1 && sizeof(unsigned)==4 && C3D_FVUNIF_COUNT%4==0,
               "Sparse uniform scan requires four-byte groups of bool flags");

C3D_FVec C3D_FVUnif[2][C3D_FVUNIF_COUNT];
C3D_IVec C3D_IVUnif[2][C3D_IVUNIF_COUNT];
u16      C3D_BoolUnifs[2];

bool C3D_FVUnifDirty[2][C3D_FVUNIF_COUNT];
bool C3D_IVUnifDirty[2][C3D_IVUNIF_COUNT];
bool C3D_BoolUnifsDirty[2];

static struct
{
	bool dirty;
	int count;
	float24Uniform_s* data;
} C3Di_ShaderFVecData[2];

static bool C3Di_FVUnifEverDirty[2][C3D_FVUNIF_COUNT];
static bool C3Di_IVUnifEverDirty[2][C3D_IVUNIF_COUNT];

static picaNativeUniformCache uniformCache;
#define uploaded uniformCache.values
#define uploadedValid uniformCache.valid
#define previousFast uniformCache.fast
#define avoidedVectors uniformCache.avoided
#define uploadedVectors uniformCache.sent
unsigned g_picaNativeUniformMask[2][3];
bool g_picaNativeTrackedFlush;
static int trackedNext(GPU_SHADER_TYPE type,int first)
{
    unsigned block=(unsigned)first>>5;
    if(block>=3)return C3D_FVUNIF_COUNT;
    unsigned bits=g_picaNativeUniformMask[type][block]&(~0u<<((unsigned)first&31));
    for(;;) {
        if(bits)return (int)(block*32+__builtin_ctz(bits));
        if(++block==3)return C3D_FVUNIF_COUNT;
        bits=g_picaNativeUniformMask[type][block];
    }
}
void picaNativeUniformInvalidate(GPU_SHADER_TYPE type,int first,int count)
{
    if ((unsigned)type>1 || first<0 || count<0 || first>C3D_FVUNIF_COUNT-count)
        return;
    memset(&uploadedValid[type][first],0,(size_t)count);
}
static bool nativeUniformsEnabled(void)
{
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    return nativeBenchNativeUniforms();
#else
    return true;
#endif
}
bool picaNativeUniformBatchReady(void)
{
    return nativeUniformsEnabled() &&
        C3Di_ShaderFVecData[0].count<=8 && C3Di_ShaderFVecData[1].count<=8;
}
static inline void uniformWrite(u32 reg,u32 value)
{
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    if(g_picaNativeCommandBlock) {
        picaNativeCommandAdd(g_picaNativeCommandBlock,GPUCMD_HEADER(0,15,reg),&value,1);return;
    }
#endif
    GPUCMD_AddWrite(reg,value);
}
static inline void uniformWrites(u32 reg,const u32* values,unsigned count,bool increment)
{
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
    if(g_picaNativeCommandBlock) {
        picaNativeCommandAdd(g_picaNativeCommandBlock,GPUCMD_HEADER(increment,15,reg),values,count);return;
    }
#endif
    if(increment)GPUCMD_AddIncrementalWrites(reg,values,count);
    else GPUCMD_AddWrites(reg,values,count);
}
static bool sameUploaded(GPU_SHADER_TYPE type,int id)
{
    return uploadedValid[type][id] &&
        memcmp(&uploaded[type][id],&C3D_FVUnif[type][id],sizeof(C3D_FVec))==0;
}
void picaNativeUniformStats(unsigned* avoided,unsigned* sent)
{
    *avoided=avoidedVectors;*sent=uploadedVectors;
}
static void ReferenceUpdateUniforms(GPU_SHADER_TYPE type)
{
	int offset = type == GPU_GEOMETRY_SHADER ? (GPUREG_GSH_BOOLUNIFORM-GPUREG_VSH_BOOLUNIFORM) : 0;
	int i = 0;

	// Update FVec uniforms that come from shader constants
	if (C3Di_ShaderFVecData[type].dirty)
	{
		while (i < C3Di_ShaderFVecData[type].count)
		{
			float24Uniform_s* u = &C3Di_ShaderFVecData[type].data[i++];
			GPUCMD_AddIncrementalWrites(GPUREG_VSH_FLOATUNIFORM_CONFIG+offset, (u32*)u, 4);
			C3D_FVUnifDirty[type][u->id] = false;
		}
		C3Di_ShaderFVecData[type].dirty = false;
		i = 0;
	}

	// Update FVec uniforms
	while (i < C3D_FVUNIF_COUNT)
	{
		if (!C3D_FVUnifDirty[type][i])
		{
			i ++;
			continue;
		}

		// Find the number of consecutive dirty uniforms
		int j;
		for (j = i; j < C3D_FVUNIF_COUNT && C3D_FVUnifDirty[type][j]; j ++);

		// Upload the uniforms
		GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG+offset, 0x80000000|i);
		GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA+offset, (u32*)&C3D_FVUnif[type][i], (j-i)*4);

		// Clear the dirty flag
		int k;
		for (k = i; k < j; k ++)
		{
			C3D_FVUnifDirty[type][k] = false;
			C3Di_FVUnifEverDirty[type][k] = true;
		}

		// Advance
		i = j;
	}

	// Update IVec uniforms
	for (i = 0; i < C3D_IVUNIF_COUNT; i ++)
	{
		if (!C3D_IVUnifDirty[type][i]) continue;

		GPUCMD_AddWrite(GPUREG_VSH_INTUNIFORM_I0+offset+i, C3D_IVUnif[type][i]);
		C3D_IVUnifDirty[type][i] = false;
		C3Di_IVUnifEverDirty[type][i] = false;
	}

	// Update bool uniforms
	if (C3D_BoolUnifsDirty[type])
	{
		GPUCMD_AddWrite(GPUREG_VSH_BOOLUNIFORM+offset, 0x7FFF0000 | C3D_BoolUnifs[type]);
		C3D_BoolUnifsDirty[type] = false;
	}
}


void C3D_UpdateUniforms(GPU_SHADER_TYPE type)
{
	int offset = type == GPU_GEOMETRY_SHADER ? (GPUREG_GSH_BOOLUNIFORM-GPUREG_VSH_BOOLUNIFORM) : 0;
	int i = 0;
	const bool fast=nativeUniformsEnabled();
    if(previousFast[type]!=fast) {
        memset(uploadedValid[type],0,sizeof(uploadedValid[type]));
        previousFast[type]=fast;
    }
    if(!fast) {ReferenceUpdateUniforms(type);memset(g_picaNativeUniformMask[type],0,sizeof(g_picaNativeUniformMask[type]));return;}


	// Update FVec uniforms that come from shader constants
	if (C3Di_ShaderFVecData[type].dirty)
	{
		while (i < C3Di_ShaderFVecData[type].count)
		{
			float24Uniform_s* u = &C3Di_ShaderFVecData[type].data[i++];
			uniformWrites(GPUREG_VSH_FLOATUNIFORM_CONFIG+offset, (u32*)u, 4,true);
			C3D_FVUnifDirty[type][u->id] = false;
            uploadedValid[type][u->id]=false;
		}
		C3Di_ShaderFVecData[type].dirty = false;
		i = 0;
	}

	// Update FVec uniforms
	while (i < C3D_FVUNIF_COUNT)
	{
        if(g_picaNativeTrackedFlush) {
            i=trackedNext(type,i);
            if(i==C3D_FVUNIF_COUNT)break;
        } else if(fast && !(i&3)) {
            unsigned word;
            memcpy(&word,&C3D_FVUnifDirty[type][i],sizeof(word));
            if(!word) {i+=4;continue;}
        }
		if (!C3D_FVUnifDirty[type][i])
		{
			i ++;
			continue;
		}

        if(fast && sameUploaded(type,i)) {
            C3D_FVUnifDirty[type][i]=false;
            C3Di_FVUnifEverDirty[type][i]=true;
            ++avoidedVectors;++i;continue;
        }
		// Find the number of consecutive dirty uniforms
		int j;
		for (j = i; j < C3D_FVUNIF_COUNT && C3D_FVUnifDirty[type][j] &&
             (!fast || !sameUploaded(type,j)); j ++);

		// Upload the uniforms
		uniformWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG+offset, 0x80000000|i);
		uniformWrites(GPUREG_VSH_FLOATUNIFORM_DATA+offset, (u32*)&C3D_FVUnif[type][i], (j-i)*4,false);

		// Clear the dirty flag
		int k;
		for (k = i; k < j; k ++)
		{
			C3D_FVUnifDirty[type][k] = false;
			C3Di_FVUnifEverDirty[type][k] = true;
            uploaded[type][k]=C3D_FVUnif[type][k];
            uploadedValid[type][k]=true;
            ++uploadedVectors;
		}

		// Advance
		i = j;
	}

	// Update IVec uniforms
	for (i = 0; i < C3D_IVUNIF_COUNT; i ++)
	{
		if (!C3D_IVUnifDirty[type][i]) continue;

		uniformWrite(GPUREG_VSH_INTUNIFORM_I0+offset+i, C3D_IVUnif[type][i]);
		C3D_IVUnifDirty[type][i] = false;
		C3Di_IVUnifEverDirty[type][i] = false;
	}

	// Update bool uniforms
	if (C3D_BoolUnifsDirty[type])
	{
		uniformWrite(GPUREG_VSH_BOOLUNIFORM+offset, 0x7FFF0000 | C3D_BoolUnifs[type]);
		C3D_BoolUnifsDirty[type] = false;
	}
    memset(g_picaNativeUniformMask[type],0,sizeof(g_picaNativeUniformMask[type]));
}

void C3Di_DirtyUniforms(GPU_SHADER_TYPE type)
{
    memset(uploadedValid[type],0,sizeof(uploadedValid[type]));
	int i;
	C3D_BoolUnifsDirty[type] = true;
	if (C3Di_ShaderFVecData[type].count)
		C3Di_ShaderFVecData[type].dirty = true;
	for (i = 0; i < C3D_FVUNIF_COUNT; i ++) {
		C3D_FVUnifDirty[type][i] = C3D_FVUnifDirty[type][i] || C3Di_FVUnifEverDirty[type][i];
        if(C3D_FVUnifDirty[type][i])g_picaNativeUniformMask[type][i>>5]|=1u<<(i&31);
    }
	for (i = 0; i < C3D_IVUNIF_COUNT; i ++)
		C3D_IVUnifDirty[type][i] = C3D_IVUnifDirty[type][i] || C3Di_IVUnifEverDirty[type][i];
}

void C3Di_LoadShaderUniforms(shaderInstance_s* si)
{
	GPU_SHADER_TYPE type = si->dvle->type;
	if (si->boolUniformMask)
	{
		C3D_BoolUnifs[type] &= ~si->boolUniformMask;
		C3D_BoolUnifs[type] |= si->boolUniforms;
	}

	if (type == GPU_GEOMETRY_SHADER)
		C3D_BoolUnifs[type] &= ~BIT(15);
	C3D_BoolUnifsDirty[type] = true;

	if (si->intUniformMask)
	{
		int i;
		for (i = 0; i < 4; i ++)
		{
			if (si->intUniformMask & BIT(i))
			{
				C3D_IVUnif[type][i] = si->intUniforms[i];
				C3D_IVUnifDirty[type][i] = true;
			}
		}
	}
	C3Di_ShaderFVecData[type].dirty = true;
	C3Di_ShaderFVecData[type].count = si->numFloat24Uniforms;
	C3Di_ShaderFVecData[type].data = si->float24Uniforms;
}

void C3Di_ClearShaderUniforms(GPU_SHADER_TYPE type)
{
	C3Di_ShaderFVecData[type].dirty = false;
	C3Di_ShaderFVecData[type].count = 0;
	C3Di_ShaderFVecData[type].data = NULL;
}

#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
void picaNativeSaveUniformProducer(picaNativeUniformProducerState* s)
{
    memcpy(s->floatDirty,C3D_FVUnifDirty,sizeof(s->floatDirty));memcpy(s->floatEver,C3Di_FVUnifEverDirty,sizeof(s->floatEver));
    memcpy(s->intDirty,C3D_IVUnifDirty,sizeof(s->intDirty));memcpy(s->intEver,C3Di_IVUnifEverDirty,sizeof(s->intEver));
    memcpy(s->boolDirty,C3D_BoolUnifsDirty,sizeof(s->boolDirty));memcpy(s->masks,g_picaNativeUniformMask,sizeof(s->masks));
    for(unsigned i=0;i<2;++i)s->constantDirty[i]=C3Di_ShaderFVecData[i].dirty;
}
void picaNativeRestoreUniformProducer(const picaNativeUniformProducerState* s)
{
    memcpy(C3D_FVUnifDirty,s->floatDirty,sizeof(s->floatDirty));memcpy(C3Di_FVUnifEverDirty,s->floatEver,sizeof(s->floatEver));
    memcpy(C3D_IVUnifDirty,s->intDirty,sizeof(s->intDirty));memcpy(C3Di_IVUnifEverDirty,s->intEver,sizeof(s->intEver));
    memcpy(C3D_BoolUnifsDirty,s->boolDirty,sizeof(s->boolDirty));memcpy(g_picaNativeUniformMask,s->masks,sizeof(s->masks));
    for(unsigned i=0;i<2;++i)C3Di_ShaderFVecData[i].dirty=s->constantDirty[i];
}
bool picaNativeSameUniformProducer(const picaNativeUniformProducerState* a,const picaNativeUniformProducerState* b)
{
    return !memcmp(a->floatDirty,b->floatDirty,sizeof(a->floatDirty)) && !memcmp(a->floatEver,b->floatEver,sizeof(a->floatEver)) &&
        !memcmp(a->intDirty,b->intDirty,sizeof(a->intDirty)) && !memcmp(a->intEver,b->intEver,sizeof(a->intEver)) &&
        !memcmp(a->boolDirty,b->boolDirty,sizeof(a->boolDirty)) && !memcmp(a->masks,b->masks,sizeof(a->masks)) &&
        !memcmp(a->constantDirty,b->constantDirty,sizeof(a->constantDirty));
}
picaNativeUniformCache* picaNativeSharedUniformCache(void){return &uniformCache;}
void picaNativeCaptureUniforms(GPU_SHADER_TYPE type,picaNativeUniformSnapshot* s)
{
    s->count=s->constantCount=s->intMask=0;
    s->boolDirty=C3D_BoolUnifsDirty[type];s->boolValue=C3D_BoolUnifs[type];
    if(C3Di_ShaderFVecData[type].dirty) {
        s->constantCount=(unsigned)C3Di_ShaderFVecData[type].count;
        for(unsigned i=0;i<s->constantCount;++i) {
            s->constants[i]=C3Di_ShaderFVecData[type].data[i];
            C3D_FVUnifDirty[type][s->constants[i].id]=false;
        }
        C3Di_ShaderFVecData[type].dirty=false;
    }
    for(int i=trackedNext(type,0);i<C3D_FVUNIF_COUNT;i=trackedNext(type,i+1)) {
        if(!C3D_FVUnifDirty[type][i])continue;
        s->ids[s->count]=(unsigned char)i;s->values[s->count++]=C3D_FVUnif[type][i];
        C3D_FVUnifDirty[type][i]=false;C3Di_FVUnifEverDirty[type][i]=true;
    }
    for(unsigned i=0;i<C3D_IVUNIF_COUNT;++i)if(C3D_IVUnifDirty[type][i]) {
        s->intMask|=1u<<i;s->integers[i]=C3D_IVUnif[type][i];
        C3D_IVUnifDirty[type][i]=false;C3Di_IVUnifEverDirty[type][i]=false;
    }
    C3D_BoolUnifsDirty[type]=false;
    memset(g_picaNativeUniformMask[type],0,sizeof(g_picaNativeUniformMask[type]));
}
static void snapshotWrite(picaNativeCommandBlock* p,u32 reg,u32 value)
{picaNativeCommandAdd(p,GPUCMD_HEADER(0,15,reg),&value,1);}
static bool snapshotSame(const picaNativeUniformSnapshot* s,unsigned index,
                         const picaNativeUniformCache* c,GPU_SHADER_TYPE type)
{
    const unsigned id=s->ids[index];
    return c->valid[type][id] && !memcmp(&c->values[type][id],&s->values[index],sizeof(C3D_FVec));
}
void picaNativeCompileUniforms(picaNativeCommandBlock* p,GPU_SHADER_TYPE type,
    const picaNativeUniformSnapshot* s,picaNativeUniformCache* c)
{
    const unsigned offset=type==GPU_GEOMETRY_SHADER ? GPUREG_GSH_BOOLUNIFORM-GPUREG_VSH_BOOLUNIFORM : 0;
    if(!c->fast[type]){memset(c->valid[type],0,sizeof(c->valid[type]));c->fast[type]=true;}
    for(unsigned i=0;i<s->constantCount;++i) {
        const float24Uniform_s* value=&s->constants[i];
        picaNativeCommandAdd(p,GPUCMD_HEADER(1,15,GPUREG_VSH_FLOATUNIFORM_CONFIG+offset),(const u32*)value,4);
        c->valid[type][value->id]=false;
    }
    for(unsigned i=0;i<s->count;) {
        if(snapshotSame(s,i,c,type)){++c->avoided;++i;continue;}
        unsigned end=i+1;
        while(end<s->count && s->ids[end]==s->ids[end-1]+1 && !snapshotSame(s,end,c,type))++end;
        snapshotWrite(p,GPUREG_VSH_FLOATUNIFORM_CONFIG+offset,0x80000000u|s->ids[i]);
        picaNativeCommandAdd(p,GPUCMD_HEADER(0,15,GPUREG_VSH_FLOATUNIFORM_DATA+offset),
            (const u32*)&s->values[i],(end-i)*4);
        for(;i<end;++i){const unsigned id=s->ids[i];c->values[type][id]=s->values[i];c->valid[type][id]=true;++c->sent;}
    }
    for(unsigned i=0;i<C3D_IVUNIF_COUNT;++i)if(s->intMask&(1u<<i))
        snapshotWrite(p,GPUREG_VSH_INTUNIFORM_I0+offset+i,s->integers[i]);
    if(s->boolDirty)snapshotWrite(p,GPUREG_VSH_BOOLUNIFORM+offset,0x7fff0000u|s->boolValue);
}
#endif
#undef uploaded
#undef uploadedValid
#undef previousFast
#undef avoidedVectors
#undef uploadedVectors
