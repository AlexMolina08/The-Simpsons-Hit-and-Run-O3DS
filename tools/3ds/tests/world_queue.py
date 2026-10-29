#!/usr/bin/env python3
"""Check stable world-request promotion and reentrant completion ownership."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/loading/loadingmanager.cpp').read_text()
start = source.index('void LoadingManager::OnLoadFileComplete(')
opening = source.index('{', start)
end, depth = opening + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
completion = source[start:end]
code = r'''
#include <cassert>
#include <vector>
#include <algorithm>
#include <loading/worldpriority.h>
#define rAssert(value) assert(value)
#define rReleasePrintf(...) ((void)0)
enum {CLO_NO_LOADING_SPEW};
struct CommandLineOptions {static bool Get(int){return true;}};
static unsigned radTimeGetMilliseconds(){return 0;}
bool gLoadingSpew=false;
struct FileHandler {unsigned releases=0;void Release(){++releases;}};
struct LoadingManager {
 struct ProcessRequestsCallback {virtual void OnProcessRequestsComplete(void*)=0;};
 struct LoadingRequest {char filename[128]={};unsigned startTime=0;FileHandler* pFileHandler=nullptr;ProcessRequestsCallback* pCallback=nullptr;void* pUserData=nullptr;};
 static constexpr unsigned MAX_REQUESTS=200;
 LoadingRequest mRequests[MAX_REQUESTS];unsigned mRequestHead=0;bool mLoading=true;unsigned processed=0;
 void ProcessNextRequest(){++processed;}
 void OnLoadFileComplete(void*);
};
''' + completion + r'''
struct Reentrant:LoadingManager::ProcessRequestsCallback {
 LoadingManager* manager;FileHandler* replacement;void* expected;
 void OnProcessRequestsComplete(void* data)override{
  assert(data==expected&&!manager->mLoading&&manager->mRequestHead==1);
  manager->mRequests[0].pFileHandler=replacement;
  manager->mRequests[0].pCallback=nullptr;manager->mRequests[0].pUserData=nullptr;
 }
};
struct Request {unsigned id;bool world;};
int main(){
 for(const char* name:{"art\\l2r2.p3d","art/l7z12.p3d","ART/L1R4A.P3D"})assert(WorldPriority::Exterior(name));
 for(const char* name:{"art/l2i03.p3d","art/l2_TERRA.p3d","art/chars/homer.p3d","l8r2.p3d","l2r.p3d","l2r2.p3dx"})assert(!WorldPriority::Exterior(name));
 for(unsigned head=0;head<200;++head)for(unsigned count=1;count<200;++count)for(unsigned active=0;active<2;++active){
  if(active&&count<2)continue;
  Request queue[200]={};std::vector<unsigned> expected;
  for(unsigned n=0;n<count;++n){queue[(head+n)%200]={n,n>0&&n<3};expected.push_back(n);}
  queue[(head+count-1)%200].world=true;
  unsigned insert=active;
  while(insert<count-1&&queue[(head+insert)%200].world)++insert;
  expected.pop_back();expected.insert(expected.begin()+insert,count-1);
  Request* identity=&queue[head];
  WorldPriority::Promote(queue,head,(head+count)%200,active,[](const Request& r){return r.world;});
  for(unsigned n=0;n<count;++n)assert(queue[(head+n)%200].id==expected[n]);
  if(active)assert(identity==&queue[head]&&identity->id==0);
 }
 LoadingManager manager;FileHandler completed,replacement;int token=1;
 Reentrant callback;callback.manager=&manager;callback.replacement=&replacement;callback.expected=&token;
 manager.mRequests[0].pFileHandler=&completed;manager.mRequests[0].pCallback=&callback;manager.mRequests[0].pUserData=&token;
 manager.OnLoadFileComplete(&manager.mRequests[0]);
 assert(completed.releases==1&&replacement.releases==0&&manager.processed==1);
}
'''
with tempfile.TemporaryDirectory(prefix='world-queue-') as directory:
    p = Path(directory)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-O1', '-fsanitize=address,undefined',
                    '-I' + str(root / 'code'), str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: world classification, all 200-slot wrap positions, stable urgent/background/callback ordering, active callback identity and reentrant handler release.')
