#include "libs/pure3d/pddi/pica/skincore.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
int main()
{
    using namespace picaSkin;
    assert(Fits(1024, 0, Reserve + 1024));
    assert(!Fits(1024, 0, Reserve + 1023));
    assert(!Fits(1024, Budget, Reserve + 1024));
    assert(!Fits(~0u, 0, ~0u));
    std::mt19937 rng(73129);
    std::uniform_real_distribution<float> value(-2, 2), weight(0, 1);
    unsigned char original[36];
    for (unsigned i = 0; i < 36; ++i)
        original[i] = i;
    double worst = 0;
    for (unsigned trial = 0; trial < 20000; ++trial)
    {
        float matrices[23][4][4], packed[23][3][4];
        for (unsigned b = 0; b < 23; ++b)
        {
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c)
                    matrices[b][r][c] = value(rng);
            assert(PackMatrix(packed[b], matrices[b]));
        }
        Source src = {};
        for (unsigned i = 0; i < 3; ++i)
        {
            src.position[i] = value(rng);
            src.normal[i] = value(rng);
        }
        for (unsigned i = 0; i < 4; ++i)
            src.indices[i] = (trial % 5 == 0 ? 22 : rng() % 23);
        unsigned influence = 1 + trial % 4;
        if (influence == 1)
            src.weights[0] = 1;
        else
        {
            src.weights[0] = weight(rng) * .4f;
            src.weights[1] = 1 - src.weights[0];
            if (influence >= 3)
            {
                src.weights[1] *= .5f;
                src.weights[2] = 1 - (src.weights[1] + src.weights[0]);
            }
            if (influence == 4)
                src.weights[2] *= .5f;
        }
        Vertex v;
        assert(PackVertex(v, src, original, 23));
        assert(!std::memcmp(v.uv, original + 24, 12));
        assert((v.offsets[3] < 0) == (src.weights[0] == 1));
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            float cpu = 0;
            double gpu = 0;
            for (unsigned j = 0; j < (influence == 1 ? 1u : 4u); ++j)
            {
                unsigned b = src.indices[j];
                const float *p = src.position;
                float transformed = matrices[b][0][axis] * p[0] + matrices[b][1][axis] * p[1] +
                                    matrices[b][2][axis] * p[2] + matrices[b][3][axis];
                float w = j < 3 ? src.weights[j]
                                : 1.0f - (src.weights[2] + src.weights[1] + src.weights[0]);
                cpu += transformed * w;
                unsigned index = v.offsets[j] / 3;
                double dot = packed[index][axis][3];
                for (unsigned k = 0; k < 3; ++k)
                    dot += double(packed[index][axis][k]) * v.position[k];
                gpu += dot * v.weights[j];
            }
            double error = std::fabs(gpu - cpu);
            if (error > worst)
                worst = error;
            assert(error < 4e-6);
            float a = 0, b = 0;
            for (unsigned k = 0; k < 3; ++k)
            {
                a += matrices[src.indices[0]][k][axis] * src.normal[k];
                b += packed[v.offsets[0] / 3][axis][k] * v.normal[k];
            }
            assert(a == b);
        }
    }
    Source src = {};
    src.weights[0] = 1;
    src.indices[0] = 22;
    Vertex v;
    assert(PackVertex(v, src, original, 23));
    assert(!PackVertex(v, src, original, 22));
    assert(!PackVertex(v, src, original, 24));
    src.weights[0] = std::numeric_limits<float>::quiet_NaN();
    assert(!PackVertex(v, src, original, 23));
    float m[4][4] = {}, out[3][4];
    m[1][2] = std::numeric_limits<float>::infinity();
    assert(!PackMatrix(out, m));
    std::printf("20000 matrix/weight/normal cases passed; float32-v-double max error %.9g\n",
                worst);
}
