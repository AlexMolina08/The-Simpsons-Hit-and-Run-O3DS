#include <pddi/pica/occlusion.hpp>
#include <pddi/pica/occlusionmetrics.h>
#include <pddi/pica/context.hpp>
#include <pddi/pica/display.hpp>
#include <pddi/pica/nativecontext.h>
#include <pddi/pica/statecache.hpp>
#include <pddi/pica/nonormal.hpp>
#include <new>
#include <cstdlib>

unsigned g_picaVisibilityMode=0;
namespace {
constexpr unsigned GeometryBudget=512*1024;
unsigned geometryBytes,geometryCount;
picaVisibilityStats visibilityStats={};
struct State {
    picaOcclusion::Grid grid;
    picaOcclusion::Transform transform={};
    pddiMatrix modelView;
    C3D_Mtx projection;
    bool prepared=false,foreignState=false;
};
State* visibility=nullptr;
struct Timer {
    u64* destination;u64 start;
    explicit Timer(u64* target):destination(target),start(g_picaVisibilityMode==2?svcGetSystemTick():0){}
    ~Timer(){if(start)*destination+=svcGetSystemTick()-start;}
};
}
extern "C" void picaVisibilityGetStats(picaVisibilityStats* output)
{
    *output=visibilityStats;output->geometryBytes=__atomic_load_n(&geometryBytes,__ATOMIC_RELAXED);output->geometryCount=__atomic_load_n(&geometryCount,__ATOMIC_RELAXED);
}
extern "C" void picaVisibilityReset(bool newFrame)
{
    if(!visibility && newFrame && g_picaVisibilityMode)visibility=new(std::nothrow) State;
    if(visibility){visibility->grid.Clear(newFrame);visibility->prepared=false;if(newFrame)visibility->foreignState=false;++visibilityStats.resets;}
}
extern "C" void picaVisibilityForeignState()
{
    if(visibility && g_picaVisibilityMode){visibility->foreignState=true;visibility->grid.Clear();++visibilityStats.foreignBarriers;}
}
extern "C" void picaVisibilityShutdown(){delete visibility;visibility=nullptr;}
extern "C" void picaVisibilityObserved(u64 ticks){visibilityStats.hiddenDrawTicks+=ticks;}

void picaPrimBuffer::VisibilityInvalidate()
{
    if(!visibilityGeometry)return;
    std::free(visibilityGeometry);visibilityGeometry=nullptr;
    __atomic_fetch_sub(&geometryBytes,unsigned(sizeof(picaOcclusion::Geometry)),__ATOMIC_RELAXED);
    __atomic_fetch_sub(&geometryCount,1u,__ATOMIC_RELAXED);
}
void picaPrimBuffer::VisibilityBuild()
{
    VisibilityInvalidate();
    // Only a completed immutable upload creates metadata. Any later vertex or
    // index writer discards it; rebuilding never enters the frame hot path.
    if(transient || !buffer || !allocated || !indices || !IndicesInRange() ||
       (vertexType&PDDI_V_WEIGHTS) ||
       (primType!=PDDI_PRIM_TRIANGLES && primType!=PDDI_PRIM_TRISTRIP))return;
    const unsigned bytes=sizeof(picaOcclusion::Geometry);
    if(__atomic_fetch_add(&geometryBytes,bytes,__ATOMIC_RELAXED)+bytes>GeometryBudget) {
        __atomic_fetch_sub(&geometryBytes,bytes,__ATOMIC_RELAXED);return;
    }
    auto* g=static_cast<picaOcclusion::Geometry*>(std::malloc(bytes));
    if(!g){__atomic_fetch_sub(&geometryBytes,bytes,__ATOMIC_RELAXED);return;}
    unsigned stride=PICA_VERTEX_STRIDE;
#ifdef SRR2_OLD3DS_NO_NORMAL_FETCH
    if(noNormalPacked)stride=picaNoNormal::Stride;
#endif
    g->count=0;
    for(unsigned i=0;i<allocated;++i) {
        const float* v=reinterpret_cast<const float*>(buffer+i*stride);
        for(unsigned c=0;c<3;++c) {
            if(!std::isfinite(v[c]) || std::fabs(v[c])>1000000.f) {
                std::free(g);__atomic_fetch_sub(&geometryBytes,bytes,__ATOMIC_RELAXED);return;
            }
            if(!i || v[c]<g->low[c])g->low[c]=v[c];
            if(!i || v[c]>g->high[c])g->high[c]=v[c];
        }
    }
    float areas[8]={};
    const unsigned step=primType==PDDI_PRIM_TRIANGLES?3:1;
    for(unsigned i=0;i+2<indexCount;i+=step) {
        unsigned ids[]={indices[i],indices[i+1],indices[i+2]};
        if(step==1 && (i&1))std::swap(ids[0],ids[1]);
        picaOcclusion::Triangle t;
        for(unsigned j=0;j<3;++j)std::memcpy(t.vertex[j],buffer+ids[j]*stride,12);
        float a[3],b[3],cross[3];
        for(unsigned c=0;c<3;++c){a[c]=t.vertex[1][c]-t.vertex[0][c];b[c]=t.vertex[2][c]-t.vertex[0][c];}
        cross[0]=a[1]*b[2]-a[2]*b[1];cross[1]=a[2]*b[0]-a[0]*b[2];cross[2]=a[0]*b[1]-a[1]*b[0];
        const float area=cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2];
        if(!std::isfinite(area) || area<=.000001f)continue;
        unsigned slot=g->count;
        if(slot==8){slot=0;for(unsigned j=1;j<8;++j)if(areas[j]<areas[slot])slot=j;if(area<=areas[slot])continue;}
        else ++g->count;
        areas[slot]=area;g->triangles[slot]=t;
    }
    visibilityGeometry=g;__atomic_fetch_add(&geometryCount,1u,__ATOMIC_RELAXED);
}
const picaOcclusion::Geometry* picaPrimBuffer::VisibilityGeometry() const
{
    if(transient)return nullptr;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    if(skinDrawing || skinAllocation)return nullptr;
#endif
#ifdef SRR2_OLD3DS_SKIN_RING
    if(skinVertices)return nullptr;
#endif
#ifdef SRR2_OLD3DS_WHEEL_GPU_BATCH
    if(wheelPaletteBuffer)return nullptr;
#endif
    return visibilityGeometry;
}
bool picaContext::VisibilityPrepare()
{
    // The native framebuffer is the normal world target at 100% scale.
    // IsWorldTarget() describes only the optional lower-resolution offscreen pass.
    unsigned rejection=12;
    if(!visibility || !g_picaVisibilityMode || !g_picaInFrame)rejection=0;
    else if(visibility->foreignState)rejection=1;
    else if(!visibilityWorld || bottomPass)rejection=2;
    else if(display->StereoActive())rejection=3;
    else if(!picaNativeNormalProgramBound())rejection=4;
    else if(state.viewState->projectionMode!=PDDI_PROJECTION_PERSPECTIVE)rejection=5;
    else if(state.stencilState->enabled)rejection=6;
    else if(state.viewState->zBias!=0)rejection=7;
    else if(state.viewState->zRange[0]!=0 || state.viewState->zRange[1]!=1)rejection=8;
    else if(!state.renderState->zEnabled ||
            (state.renderState->zCompare!=PDDI_COMPARE_LESS && state.renderState->zCompare!=PDDI_COMPARE_LESSEQUAL))rejection=9;
    else {
        const auto& scissor=state.viewState->scissor;
        if(scissor.left>0 || scissor.top>0 || scissor.right<display->GetWidth() || scissor.bottom<display->GetHeight())rejection=10;
        else if(wantViewport[0]!=0 || wantViewport[1]!=0 ||
                wantViewport[2]!=int(display->WorldHeight()) || wantViewport[3]!=int(display->WorldWidth()))rejection=11;
    }
    if(rejection<12){if(g_picaVisibilityMode==2)++visibilityStats.rejected[rejection];return false;}
    const pddiMatrix* mv=state.matrixStack[PDDI_MATRIX_MODELVIEW]->Top();
    if(!visibility->prepared || std::memcmp(&visibility->modelView,mv,sizeof(*mv)) ||
       std::memcmp(&visibility->projection,&hwProjection,sizeof(hwProjection))) {
        float model[4][4],projection[4][4];
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
            model[r][c]=mv->Row4(c)[r];projection[r][c]=hwProjection.r[r].c[3-c];
        }
        visibility->transform.Prepare(model,projection);
        visibility->modelView=*mv;visibility->projection=hwProjection;visibility->prepared=true;
    }
    return visibility->transform.valid;
}
void picaContext::VisibilityBarrier()
{
    // A later depth write with a different comparison can replace nearer depth.
    // Clear evidence before every submission path, including streams and skins.
    if(state.renderState->zWrite && state.renderState->zEnabled &&
       state.renderState->zCompare!=PDDI_COMPARE_LESS && state.renderState->zCompare!=PDDI_COMPARE_LESSEQUAL)
        picaVisibilityReset(false);
}
bool picaContext::VisibilityQuery(picaPrimBuffer* buffer)
{
    Timer timer(&visibilityStats.queryTicks);
    const auto* g=buffer->VisibilityGeometry();if(!g || !VisibilityPrepare())return false;
    float center[3],extent[3];for(unsigned c=0;c<3;++c){center[c]=(g->low[c]+g->high[c])*.5f;extent[c]=(g->high[c]-g->low[c])*.5f;}
    const bool hidden=visibility->grid.Hidden(visibility->transform.Project(center,extent,.001f));
    ++visibilityStats.tested;if(hidden)++visibilityStats.hidden;
    if(hidden && g_picaVisibilityMode==1){++visibilityStats.skipped;visibilityStats.skippedVertices+=buffer->VisibilityVertexCount();}
    return hidden;
}
void picaContext::VisibilityRaster(picaPrimBuffer* buffer)
{
    Timer timer(&visibilityStats.rasterTicks);
    const auto* g=buffer->VisibilityGeometry();if(!g || !g->count || !VisibilityPrepare() ||
       !g_picaState.depthValid || !g_picaState.depthEnable || !(g_picaState.depthMask&GPU_WRITE_DEPTH) ||
       !g_picaState.alphaTestValid || (g_picaState.alphaTestEnable && g_picaState.alphaTestFunc!=GPU_ALWAYS) ||
       !g_picaState.cullValid || visibility->grid.visits>=picaOcclusion::CellBudget)return;
    const unsigned oldTriangles=visibility->grid.triangles,oldCells=visibility->grid.visits;
    const float zero[3]={};++visibilityStats.rasterCalls;
    for(unsigned i=0;i<g->count && visibility->grid.visits<picaOcclusion::CellBudget;++i) {
        picaOcclusion::Point p[3];
        for(unsigned j=0;j<3;++j)p[j]=visibility->transform.Project(g->triangles[i].vertex[j],zero,.001f);
        visibility->grid.Add(p[0],p[1],p[2],unsigned(g_picaState.cull));
    }
    visibilityStats.triangles+=visibility->grid.triangles-oldTriangles;visibilityStats.cells+=visibility->grid.visits-oldCells;
}
