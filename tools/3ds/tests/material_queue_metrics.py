#!/usr/bin/env python3
"""Run the production capture/dump FIFO, including partial-mode correctness."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
start=source.index('#ifdef SRR2_OLD3DS_QUEUE_METRICS\n#include <stdio.h>')
production=source[start:]
harness=r'''
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
#include "libs/pure3d/pddi/pica/materialqueue.hpp"
#include "libs/pure3d/pddi/pica/queuemetrics.hpp"
const char* output;
FILE* captureOpen(const char*,const char* mode) { return std::fopen(output,mode); }
#define fopen captureOpen
'''
test=r'''
#undef fopen
int main(int argc,char** argv) {
    assert(argc==2); output=argv[1];
    for(unsigned i=0;i<120;++i) picaQueueCapture();
    assert(queueRecordCount==1 && queueRecords[0].metrics.queueMode==0 && picaQueue::enabled);
    for(unsigned i=1;i<=130;++i) {
        g_picaQueueMetrics.frames=119; g_picaQueueMetrics.draws=i;
        picaQueueCapture();
    }
    assert(queueRecordCount==128 && queueRecordOverwritten==3);
    assert(queueRecords[queueRecordFirst].metrics.draws==3);
    assert(queueRecords[(queueRecordFirst+127)%128].metrics.draws==130);
    picaQueue::enabled=true;
    g_picaQueueMetrics.frames=2; g_picaQueueMetrics.draws=999;
    picaQueueDump();
    FILE* file=std::fopen(output,"r"); assert(file);
    std::string contents; char buffer[2048];
    while(std::fgets(buffer,sizeof(buffer),file)) contents+=buffer;
    std::fclose(file);
    assert(contents.find("overwritten=3")!=std::string::npos);
    assert(contents.find("queue_enabled=1 partial=1")!=std::string::npos);
    assert(contents.find("draws=999")!=std::string::npos);
    std::puts("PASS production metrics FIFO: alternating modes, newest-128 retention, overflow count, complete records, current mode on partial dump");
}
'''
with tempfile.TemporaryDirectory(prefix='srr2-queue-metrics-') as directory:
    p=Path(directory)
    (p/'3ds.h').write_text('#pragma once\n#include <stdint.h>\ntypedef uint64_t u64;\ninline u64 svcGetSystemTick(){static u64 tick=0;return ++tick;}\n')
    (p/'test.cpp').write_text(harness+'\n'+production+'\n'+test)
    subprocess.run(['clang++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_QUEUE_METRICS','-DSRR2_OLD3DS_MATERIAL_QUEUE','-DSRR2_OLD3DS_MATERIAL_QUEUE_AB','-DSRR2_OLD3DS_BUILD_ID="queue-test"','-I',str(p),'-I',str(root),'-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p/'dump.log')],check=True)
