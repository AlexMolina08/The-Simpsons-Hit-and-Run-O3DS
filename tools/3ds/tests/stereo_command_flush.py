#!/usr/bin/env python3
"""Run production stereo submission with separate CPU and GPU command memory."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/display.cpp').read_text()
start = source.index('void picaDisplay::StartTopSegment(void)')
end = source.index('bool picaDisplay::BeginBottomTarget(void)', start)
methods = source[start:end]
fixture = r'''
#include <cassert>
#include <cstring>
#include <vector>
using u32 = unsigned;
const unsigned GX_CMDLIST_FLUSH = 2;
u32 cpu[4][4] = {}, gpu[4][4] = {};
unsigned current = 0, streamFlushes = 0;
struct Submission {u32* data; unsigned bytes, flags;};
std::vector<Submission> queue;
void GX_ProcessCommandList(u32* data, unsigned bytes, unsigned flags) {
    queue.push_back({data, bytes, flags});
}
bool C3Di_SplitFrame(u32** data, u32* words) {
    assert(current < 4); *data = cpu[current++]; *words = 4; return true;
}
void C3D_FrameSplit(unsigned flags) {
    u32* data; u32 words; assert(C3Di_SplitFrame(&data, &words));
    GX_ProcessCommandList(data, words * 4, flags);
}
void C3Di_UpdateContext() {cpu[current][1] = 123;}
void picaWriteEye(float shift) {cpu[current][0] = shift < 0 ? 10 : shift > 0 ? 20 : 0;}
void C3D_FrameDrawOn(void*) {cpu[current][2] = 456;}
struct Context {void FlushStreams() {++streamFlushes;}};
struct picaDisplay {
    bool stereoThisFrame = true;
    u32* earlyWorldCommands=nullptr;u32 earlyWorldWords=0;
    float eyeShift = 0.045f;
    Context storage, *context = &storage;
    void* targetRight = this;
    void StartTopSegment();
    void SubmitWorldEarly();
    void FinishTopSegment();
};
'''
main = r'''
int main() {
    picaDisplay display;
    display.StartTopSegment();
    cpu[current][0] = 99; // CPU records the scene once.
    display.FinishTopSegment();
    assert(queue.size() == 4 && streamFlushes == 2);
    assert(queue[1].data == queue[3].data); // Both eyes replay the same scene.
    assert(queue[0].data[0] == 10 && queue[2].data[0] == 20);
    for(const auto& submission : queue) {
        unsigned index = 0;
        while(index < 4 && submission.data != cpu[index]) ++index;
        assert(index < 4);
        if(submission.flags & GX_CMDLIST_FLUSH)
            std::memcpy(gpu[index], submission.data, submission.bytes);
        if(std::memcmp(gpu[index], submission.data, submission.bytes)) return 77;
    }
    display.stereoThisFrame = false;
    const auto previous = queue.size();
    display.StartTopSegment(); display.FinishTopSegment();
    assert(queue.size() == previous); // Mono does not add split submissions.
}
'''
with tempfile.TemporaryDirectory(prefix='srr-stereo-flush-') as directory:
    path = Path(directory)
    for name, body in [('fixed', methods), ('unflushed', methods.replace('GX_CMDLIST_FLUSH', '0'))]:
        cpp = path / (name + '.cpp')
        binary = path / name
        cpp.write_text(fixture + body + main)
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(cpp), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)])
        assert result.returncode == (0 if name == 'fixed' else 77), (name, result.returncode)
print('PASS: stereo setup and scene commands reach GPU memory before either eye; unflushed control fails.')
