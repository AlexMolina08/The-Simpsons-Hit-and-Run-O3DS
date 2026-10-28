// Browser adapter for the production v1.2 occlusion baker. No game data lives here.
// Keep the native ray calculations unchanged; callers may sample bounded batches
// to report progress without rebuilding the BVH for each batch.
#define main srrNativeOcclusionMain
#include "../../tools/3ds/character_occlusion.cpp"
#undef main
#include <memory>
#include <numeric>
static_assert(sizeof(Vertex)==24 && sizeof(std::array<unsigned,3>)==12,
              "The browser baker must preserve the production wire format");

namespace {
struct BrowserOcclusion {
    std::vector<Vertex> vertices;
    std::unique_ptr<Occlusion> scene;
    unsigned samples;
    float radius;
    bool sun;
    Vector light;
    std::vector<unsigned> representative;
    mutable std::vector<unsigned char> values, computed;
};
}

extern "C" {
void* v12_occlusion_create(const unsigned char* request, unsigned bytes,
                           int sun, float lightX, float lightY, float lightZ)
{
    if(!request || bytes<16)return nullptr;
    std::uint32_t header[3];float radius;
    std::memcpy(header,request,12);std::memcpy(&radius,request+12,4);
    const unsigned vertices=header[0],triangles=header[1],samples=header[2];
    const std::uint64_t expected=16ull+std::uint64_t(vertices)*sizeof(Vertex)+
        std::uint64_t(triangles)*sizeof(std::array<unsigned,3>);
    if(!vertices || vertices>1000000 || triangles>2000000 || !samples ||
       samples>1024 || !(radius>0 && radius<100) || expected!=bytes)return nullptr;
    Vector light{lightX,lightY,lightZ};
    if(sun){
        if(!std::isfinite(lightX) || !std::isfinite(lightY) || !std::isfinite(lightZ) ||
           dot(light,light)<1e-10f)return nullptr;
        light=unit(light);
    }
    std::unique_ptr<BrowserOcclusion> result(new BrowserOcclusion);
    result->vertices.resize(vertices);
    std::vector<std::array<unsigned,3>> indices(triangles);
    std::memcpy(result->vertices.data(),request+16,vertices*sizeof(Vertex));
    if(triangles)std::memcpy(indices.data(),request+16+vertices*sizeof(Vertex),triangles*sizeof(indices[0]));
    for(const auto& vertex:result->vertices)for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))return nullptr;
    for(const auto& triangle:indices)for(unsigned index:triangle)if(index>=vertices)return nullptr;
    result->scene.reset(new Occlusion(result->vertices,indices));
    result->samples=samples;result->radius=radius;result->sun=sun!=0;result->light=light;
    // World colour lists repeat many identical position/normal pairs at UV and
    // material seams. Their rays see the same immutable BVH. Cache only exact
    // 24-byte matches; every distinct vertex keeps its original calculation.
    // Large projected-ground requests mostly contain unique pixel positions,
    // so avoid sorting those. Each handle is sampled serially by one worker.
    if(vertices<=300000){
        std::vector<unsigned> order(vertices);
        std::iota(order.begin(),order.end(),0u);
        std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){
            const int difference=std::memcmp(&result->vertices[a],&result->vertices[b],sizeof(Vertex));
            return difference?difference<0:a<b;
        });
        result->representative.resize(vertices);
        result->values.resize(vertices);result->computed.resize(vertices);
        unsigned first=order[0];
        for(unsigned index:order){
            if(std::memcmp(&result->vertices[first],&result->vertices[index],sizeof(Vertex)))first=index;
            result->representative[index]=first;
        }
    }
    return result.release();
}

unsigned v12_occlusion_count(const void* handle)
{
    return handle?unsigned(static_cast<const BrowserOcclusion*>(handle)->vertices.size()):0;
}

int v12_occlusion_sample(const void* handle, unsigned begin, unsigned count,
                         unsigned char* output, unsigned capacity)
{
    if(!handle || !output || count>capacity)return -1;
    const BrowserOcclusion& source=*static_cast<const BrowserOcclusion*>(handle);
    if(begin>source.vertices.size() || count>source.vertices.size()-begin)return -1;
    for(unsigned i=0;i<count;++i){
        const unsigned index=source.representative.empty()?begin+i:source.representative[begin+i];
        if(!source.representative.empty() && source.computed[index]){
            output[i]=source.values[index];
            continue;
        }
        const Vertex& vertex=source.vertices[index];
        const unsigned char value=source.sun?source.scene->sunlight(vertex,source.radius,source.samples,source.light):
            source.scene->shade(vertex,source.radius,source.samples);
        output[i]=value;
        if(!source.representative.empty()){
            source.values[index]=value;source.computed[index]=1;
        }
    }
    return int(count);
}

void v12_occlusion_destroy(void* handle)
{
    delete static_cast<BrowserOcclusion*>(handle);
}

int v12_occlusion_bake(const unsigned char* request, unsigned bytes, unsigned char* output,
                       unsigned capacity, int sun, float lightX, float lightY, float lightZ)
{
    void* handle=v12_occlusion_create(request,bytes,sun,lightX,lightY,lightZ);
    if(!handle)return -1;
    const int result=v12_occlusion_sample(handle,0,v12_occlusion_count(handle),output,capacity);
    v12_occlusion_destroy(handle);
    return result;
}
}
