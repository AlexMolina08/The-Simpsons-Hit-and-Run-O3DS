// Compare every ordered triangle and colour with the original two shadow streams.
#include <pddi/pica/blobtopology.hpp>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#include <algorithm>
struct Vertex { float x,y,z; unsigned colour; };
int main()
{
    unsigned short indices[picaBlob::IndexCount];
    picaBlob::Indices(indices);
    std::mt19937 random(0x3d5);
    std::uniform_real_distribution<float> value(-4.0f,4.0f);
    const float points[6][2]={{1.0f,2.2f},{1.2f,2.0f},{1.2f,.2f},{1.2f,-.2f},{1.2f,-2.0f},{1.0f,-2.2f}};
    const float fades[6][2]={{.5f*.4f,.866f*.4f},{.866f*.4f,.5f*.4f},{.866f*.4f,.5f*.4f},
                             {.866f*.4f,.5f*.4f},{.866f*.4f,-.5f*.4f},{.5f*.4f,-.866f*.4f}};
    for(unsigned test=0;test<10000;++test)
    {
        float adjustments[4][2];
        for(auto& a:adjustments) for(float& f:a) f=value(random);
        const float scale=value(random);
        const unsigned inside=random(), outside=random();
        auto inner = [&](int i) {
            int mirror=int(i*(1.0f/6))%2;
            int index=std::abs(mirror*5-i%6);
            int adjust=std::max(0,std::min(index-1,3));
            float x=(points[index][0]+adjustments[adjust][0])*scale;
            float y=(points[index][1]+adjustments[adjust][1])*scale;
            return Vertex{x*(1-mirror*2),y,0,inside};
        };
        auto outer = [&](int i) {
            Vertex v=inner(i);
            int mirror=int(i*(1.0f/6))%2;
            int index=std::abs(mirror*5-i%6);
            v.x+=fades[index][0]*(1-mirror*2)*scale;
            v.y+=fades[index][1]*scale;
            v.colour=outside; return v;
        };
        std::array<Vertex,picaBlob::VertexCount> mesh;
        mesh[0]={0,0,0,inside};
        for(int i=0;i<12;++i) { mesh[1+2*i]=inner(i);mesh[2+2*i]=outer(i); }
        std::vector<Vertex> reference,strip;
        for(int i=0;i<12;++i)
        {
            reference.push_back(mesh[0]);
            reference.push_back(inner((i+1)%12));
            reference.push_back(inner(i));
        }
        for(int i=0;i<=12;++i) { strip.push_back(outer(i));strip.push_back(inner(i)); }
        for(unsigned i=0;i+2<strip.size();++i)
        {
            reference.push_back(strip[i+(i&1)]);
            reference.push_back(strip[i+1-(i&1)]);
            reference.push_back(strip[i+2]);
        }
        assert(reference.size()==picaBlob::IndexCount);
        for(unsigned i=0;i<picaBlob::IndexCount;++i)
        {
            assert(indices[i]<mesh.size());
            assert(std::memcmp(&reference[i],&mesh[indices[i]],sizeof(Vertex))==0);
        }
    }
    std::puts("10000 shadows: all 36 ordered triangles and colours match bit-for-bit");
}
