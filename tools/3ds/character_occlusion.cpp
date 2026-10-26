// Offline character occlusion preparation. This executable never runs on 3DS.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

struct Vector {
    float x, y, z;
    float operator[](unsigned axis) const { return axis == 0 ? x : axis == 1 ? y : z; }
    Vector operator+(Vector b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vector operator-(Vector b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vector operator*(float scale) const { return {x*scale,y*scale,z*scale}; }
};
static float dot(Vector a, Vector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static Vector cross(Vector a, Vector b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static Vector unit(Vector a) { const float length=std::sqrt(dot(a,a));return length>1e-12f?a*(1/length):Vector{0,1,0}; }
struct Vertex { Vector position, normal; };
struct Triangle { Vector a, edge1, edge2, low, high, center; };
struct Node { Vector low, high; unsigned begin, end, left, right; };
static Vector minimum(Vector a, Vector b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
static Vector maximum(Vector a, Vector b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }

class Occlusion {
    std::vector<Triangle> triangles;
    std::vector<Node> nodes;
    unsigned build(unsigned begin, unsigned end) {
        Vector low=triangles[begin].low,high=triangles[begin].high;
        Vector centerLow=triangles[begin].center,centerHigh=centerLow;
        for(unsigned i=begin+1;i<end;++i) {
            low=minimum(low,triangles[i].low);high=maximum(high,triangles[i].high);
            centerLow=minimum(centerLow,triangles[i].center);centerHigh=maximum(centerHigh,triangles[i].center);
        }
        const unsigned index=unsigned(nodes.size());nodes.push_back({low,high,begin,end,0,0});
        if(end-begin>8) {
            const Vector span=centerHigh-centerLow;unsigned axis=span.y>span.x?1:0;
            if(span.z>span[axis])axis=2;
            const unsigned middle=(begin+end)/2;
            std::nth_element(triangles.begin()+begin,triangles.begin()+middle,triangles.begin()+end,
                [axis](const Triangle& a,const Triangle& b){return a.center[axis]<b.center[axis];});
            const unsigned left=build(begin,middle),right=build(middle,end);
            nodes[index].left=left;nodes[index].right=right;
        }
        return index;
    }
    bool hit(unsigned index, Vector origin, Vector direction, Vector inverse, float distance) const {
        const Node& node=nodes[index];float near=0,far=distance;
        for(unsigned axis=0;axis<3;++axis) {
            float a=(node.low[axis]-origin[axis])*inverse[axis];
            float b=(node.high[axis]-origin[axis])*inverse[axis];
            if(a>b)std::swap(a,b);near=std::max(near,a);far=std::min(far,b);
            if(near>far)return false;
        }
        if(node.left)return hit(node.left,origin,direction,inverse,distance) || hit(node.right,origin,direction,inverse,distance);
        for(unsigned i=node.begin;i<node.end;++i) {
            const Triangle& triangle=triangles[i];
            const Vector p=cross(direction,triangle.edge2);const float determinant=dot(triangle.edge1,p);
            if(std::fabs(determinant)<1e-9f)continue;
            const float reciprocal=1/determinant;const Vector t=origin-triangle.a;
            const float u=dot(t,p)*reciprocal;if(u<0 || u>1)continue;
            const Vector q=cross(t,triangle.edge1);const float v=dot(direction,q)*reciprocal;
            if(v<0 || u+v>1)continue;
            const float travel=dot(triangle.edge2,q)*reciprocal;
            if(travel>1e-5f && travel<distance)return true;
        }
        return false;
    }
public:
    Occlusion(const std::vector<Vertex>& vertices,const std::vector<std::array<unsigned,3>>& indices) {
        triangles.reserve(indices.size());nodes.reserve(indices.size()*2);
        for(const auto& index:indices) {
            const Vector a=vertices[index[0]].position,b=vertices[index[1]].position,c=vertices[index[2]].position;
            if(dot(cross(b-a,c-a),cross(b-a,c-a))<1e-16f)continue;
            triangles.push_back({a,b-a,c-a,minimum(a,minimum(b,c)),maximum(a,maximum(b,c)),(a+b+c)*(1.0f/3)});
        }
        if(!triangles.empty())build(0,unsigned(triangles.size()));
    }
    unsigned char shade(const Vertex& vertex,float radius,unsigned samples) const {
        if(nodes.empty() || dot(vertex.normal,vertex.normal)<1e-10f)return 255;
        const Vector normal=unit(vertex.normal);
        const Vector tangent=unit(cross(normal,std::fabs(normal.z)<.9f?Vector{0,0,1}:Vector{0,1,0}));
        const Vector bitangent=cross(normal,tangent),origin=vertex.position+normal*(radius*.015f);
        unsigned visible=0;
        for(unsigned i=0;i<samples;++i) {
            const float radial=std::sqrt((float(i)+.5f)/samples);
            const float angle=float(i)*2.39996322972865332f;
            const Vector direction=tangent*(radial*std::cos(angle))+bitangent*(radial*std::sin(angle))+
                normal*std::sqrt(1-radial*radial);
            const auto reciprocal=[](float value){return std::fabs(value)>1e-12f?1/value:std::copysign(1e30f,value);};
            const Vector inverse{reciprocal(direction.x),reciprocal(direction.y),reciprocal(direction.z)};
            visible+=!hit(0,origin,direction,inverse,radius);
        }
        return static_cast<unsigned char>(255*(.64f+.36f*std::sqrt(float(visible)/samples))+.5f);
    }
    unsigned char sunlight(const Vertex& vertex,float distance,unsigned samples,Vector light) const {
        if(nodes.empty() || dot(vertex.normal,vertex.normal)<1e-10f)return 255;
        const Vector normal=unit(vertex.normal);
        if(dot(normal,light)<=0)return 255;
        const Vector tangent=unit(cross(light,std::fabs(light.z)<.9f?Vector{0,0,1}:Vector{0,1,0}));
        const Vector bitangent=cross(light,tangent),origin=vertex.position+normal*.025f;
        unsigned visible=0;
        for(unsigned i=0;i<samples;++i){
            const float radial=.015f*std::sqrt((float(i)+.5f)/samples);
            const float angle=float(i)*2.39996322972865332f;
            const Vector direction=unit(light+tangent*(radial*std::cos(angle))+bitangent*(radial*std::sin(angle)));
            const auto reciprocal=[](float value){return std::fabs(value)>1e-12f?1/value:std::copysign(1e30f,value);};
            const Vector inverse{reciprocal(direction.x),reciprocal(direction.y),reciprocal(direction.z)};
            visible+=!hit(0,origin,direction,inverse,distance);
        }
        return static_cast<unsigned char>((255*visible+samples/2)/samples);
    }
};

int main(int argc,char** argv) {
    const bool sun=argc==5 && !std::strcmp(argv[1],"--sun");
    if(argc!=1 && !sun)return 7;
    Vector light{0,1,0};
    if(sun){
        float values[3];
        for(unsigned i=0;i<3;++i){char* end=nullptr;values[i]=std::strtof(argv[i+2],&end);if(!end || *end || !std::isfinite(values[i]))return 7;}
        light={values[0],values[1],values[2]};if(dot(light,light)<1e-10f)return 7;light=unit(light);
    }
    std::uint32_t header[3];float radius;
    if(std::fread(header,sizeof(header),1,stdin)!=1 || std::fread(&radius,sizeof(radius),1,stdin)!=1)return 1;
    const unsigned vertices=header[0],triangles=header[1],samples=header[2];
    if(!vertices || vertices>1000000 || triangles>2000000 || !samples || samples>1024 || !(radius>0 && radius<100))return 2;
    std::vector<Vertex> source(vertices);std::vector<std::array<unsigned,3>> indices(triangles);
    if(std::fread(source.data(),sizeof(Vertex),vertices,stdin)!=vertices ||
       std::fread(indices.data(),sizeof(indices[0]),triangles,stdin)!=triangles)return 3;
    for(const auto& vertex:source)for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))return 4;
    for(const auto& triangle:indices)for(unsigned index:triangle)if(index>=vertices)return 5;
    const Occlusion occlusion(source,indices);std::vector<unsigned char> output(vertices);
    for(unsigned i=0;i<vertices;++i)output[i]=sun?occlusion.sunlight(source[i],radius,samples,light):occlusion.shade(source[i],radius,samples);
    return std::fwrite(output.data(),1,output.size(),stdout)==output.size()?0:6;
}
