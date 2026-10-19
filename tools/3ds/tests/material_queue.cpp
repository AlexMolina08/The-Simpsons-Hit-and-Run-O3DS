#include "libs/pure3d/pddi/pica/materialqueue.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

struct Submission { unsigned material, id; picaQueue::Rect rect; };
static float Float24(float value)
{
    uint32_t bits;
    std::memcpy(&bits,&value,4);
    bits = (bits + 64u) & ~127u;
    std::memcpy(&value,&bits,4);
    return value;
}
static void ProjectionTests(std::mt19937& random)
{
    for(unsigned test = 0; test < 10000; ++test)
    {
        float mv[4][4] = {}, projection[4][4] = {};
        for(unsigned i = 0; i < 4; ++i) mv[i][i] = 1;
        mv[0][3] = float(int(random()%20000)-10000);
        mv[1][3] = float(int(random()%20000)-10000);
        mv[2][3] = 20000;
        projection[0][0] = 1.5f; projection[1][1] = 2.5f;
        projection[2][2] = 1; projection[3][2] = 1;
        float low[3], high[3];
        for(unsigned a = 0; a < 3; ++a)
        {
            low[a] = float(int(random()%20000)-10000);
            high[a] = low[a] + random()%100;
        }
        const picaQueue::Rect rect = picaQueue::ProjectBounds(low,high,mv,projection);
        assert(rect.valid);
        for(unsigned point = 0; point < 16; ++point)
        {
            float p[4], eye[4] = {}, clip[4] = {};
            for(unsigned a = 0; a < 3; ++a) p[a] = Float24(low[a]+(high[a]-low[a])*float(random()%10001)/10000);
            p[3] = 1;
            for(unsigned a = 0; a < 4; ++a)
                for(unsigned b = 0; b < 4; ++b)
                    eye[a] = Float24(eye[a]+Float24(Float24(mv[a][b])*p[b]));
            for(unsigned a = 0; a < 4; ++a)
                for(unsigned b = 0; b < 4; ++b)
                    clip[a] = Float24(clip[a]+Float24(Float24(projection[a][b])*eye[b]));
            for(unsigned a = 0; a < 2; ++a)
            {
                const float value = clip[a]/clip[3];
                assert(value >= rect.low[a] && value <= rect.high[a]);
            }
        }
        mv[2][3] = 0; low[2] = -1; high[2] = 1;
        assert(!picaQueue::ProjectBounds(low,high,mv,projection).valid);
    }
}
static unsigned Changes(const std::vector<Submission>& a)
{
    unsigned result = 0;
    for(unsigned i = 0; i < a.size(); ++i) result += !i || a[i].material != a[i-1].material;
    return result;
}
// A tiny raster reference includes equal-depth ties and cutout holes. The
// scheduler must preserve exact color/depth results, not just the draw count.
static std::vector<unsigned> Raster(const std::vector<Submission>& a)
{
    std::vector<unsigned> pixels(32*32, 0), depth(32*32, 100);
    for(const Submission& e : a)
        for(unsigned y = 0; y < 32; ++y)
            for(unsigned x = 0; x < 32; ++x)
            {
                if(x < e.rect.low[0] || x > e.rect.high[0] || y < e.rect.low[1] || y > e.rect.high[1]) continue;
                if((x+y+e.id)%7 == 0) continue;
                const unsigned z = e.id%5;
                if(z <= depth[y*32+x]) { depth[y*32+x] = z; pixels[y*32+x] = e.id+1; }
            }
    pixels.insert(pixels.end(), depth.begin(), depth.end());
    return pixels;
}
int main()
{
    std::mt19937 random(0x2003d);
    ProjectionTests(random);
    unsigned before = 0, after = 0, moved = 0;
    for(unsigned test = 0; test < 20000; ++test)
    {
        std::vector<Submission> original;
        const unsigned count = random() % (picaQueue::Capacity + 1);
        for(unsigned i = 0; i < count; ++i)
        {
            const float x = random()%28, y = random()%28;
            original.push_back({random()%5, i, {{x,y},{x+float(random()%5),y+float(random()%5)}, (random()%10)!=0}});
        }
        bool opportunity = false;
        for(unsigned i = 0; i < count; ++i)
            for(unsigned j = 0; j+1 < i; ++j)
                if(original[i].material == original[j].material && original[i].material != original[i-1].material) opportunity = true;
        assert(picaQueue::HasMaterialOpportunity(original.data(),count) == opportunity);
        std::vector<Submission> sorted = original;
        moved += picaQueue::Schedule(sorted.data(), sorted.size());
        assert(sorted.size() == original.size());
        std::vector<unsigned> positions(count);
        for(unsigned i = 0; i < count; ++i) positions[sorted[i].id] = i;
        for(unsigned i = 0; i < count; ++i)
            for(unsigned j = i+1; j < count; ++j)
                if(!picaQueue::Disjoint(original[i].rect, original[j].rect)) assert(positions[i] < positions[j]);
        assert(Raster(original) == Raster(sorted));
        before += Changes(original); after += Changes(sorted);
    }
    // All-overlapping/equal-depth submissions cannot move, even if material
    // sorting would otherwise look profitable. Invalid bounds are barriers too.
    Submission tied[] = {{0,0,{{0,0},{2,2},true}}, {1,1,{{0,0},{2,2},true}}, {0,2,{{0,0},{2,2},true}}};
    assert(picaQueue::Schedule(tied,3) == 0);
    assert(!picaQueue::Disjoint({{0,0},{1,1},false}, {{2,2},{3,3},true}));
    assert(!picaQueue::Disjoint({{0,0},{1,1},true}, {{1,1},{2,2},true}));
    assert(after < before && moved);
    std::printf("PASS 20000 production scheduler cases; exact raster/depth, overlap order, cutouts, capacity; material transitions %u -> %u; moves=%u\n", before,after,moved);
}
