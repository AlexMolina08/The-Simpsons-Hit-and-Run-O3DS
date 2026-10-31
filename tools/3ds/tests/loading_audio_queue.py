#!/usr/bin/env python3
"""Exercise production audio priority, pending-open handling and drive queue order."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


drive = (root/'libs/radcore/src/radfile/common/drivethread.cpp').read_text()
requests = (root/'libs/radcore/src/radfile/common/requests.cpp').read_text()
audio = (root/'libs/radsound/src/radsound/rsdfiledatasource.cpp').read_text()
files = (root/'libs/radcore/src/radfile/common/file.cpp').read_text()
start = audio.index('::radFileOpen(')
audio_open = audio[start:audio.index(');', start)+2]
start = drive.index('case radDrive::MoreToDo:')
requeue = drive[start:drive.index('break;', start)+6]
program = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
enum radFilePriority {HighPriority,NormalPriority,LowPriority,NumPriorities};
struct radRequest {radRequest* m_Next=nullptr;};
struct Semaphore {void Signal(){}};
struct radDriveThread {
 radRequest* m_RequestHead[NumPriorities]={};radRequest* m_RequestTail[NumPriorities]={};
 unsigned m_OutstandingRequests=0;Semaphore semaphore;Semaphore* m_pSema=&semaphore;
 void Lock(){}void Unlock(){}void SetCurrentRequest(void*){}
 void QueueRequest(radRequest*,radFilePriority,bool);
 radRequest* NextRequest(radFilePriority*);
};
''' + function(drive, 'void radDriveThread::QueueRequest') + '\n' + function(drive, 'radRequest* radDriveThread::NextRequest') + r'''
enum {CreateAlways,OpenExisting,Success,radCementLibraryBeforeDrive,radCementLibraryAfterDrive};
unsigned archiveLookups=0;
struct radFile;
struct radCementLibrary {radFile* OpenFile(const char*,unsigned*,unsigned*){return nullptr;}};
struct radCementLibraryList {
 int phase=0;void BeginIteration(int value){phase=value;}
 radCementLibrary* GetNext(){if(phase==radCementLibraryAfterDrive)++archiveLookups;return nullptr;}
};
struct radDrive {
 enum CompletionStatus {Complete,Error,MoreToDo};
 CompletionStatus result=MoreToDo;radCementLibraryList libraries;
 struct Media {unsigned m_SectorSize=1;} media;
 radCementLibraryList* GetLibraryList(){return &libraries;}
 CompletionStatus OpenFile(const char*,int,bool,int*,unsigned*){return result;}
 Media* GetMediaInfo(){return &media;}int GetLastError(){return Success;}
 radDriveThread* queue=nullptr;
 void QueueRequest(radRequest* request,radFilePriority priority){assert(queue);queue->QueueRequest(request,priority,false);}
};
using radMemorySpace=int;
constexpr int radMemorySpace_Null=-1;
struct radFile {
 radDrive* m_pDrive;const char* m_Filename="art/l2_TERRA.p3d";bool m_WriteAccess=false,m_IsOpen=false;
 radFile* m_pOriginalFile=nullptr;int m_Handle=0;unsigned m_Size=0,m_BaseOffset=0,m_OptimalSize=0;
 struct Cache {unsigned initialized=0;void DoInit(unsigned,const char*){++initialized;}} m_FileCache;
 void AddRef(){}
 radFilePriority m_Priority=NormalPriority;
 void ReadAsync(void*,unsigned,radMemorySpace);
};
struct FileReadRequest:radRequest {
 radFile* file;void* target;unsigned bytes;radMemorySpace space;
 FileReadRequest(radFile* f,void* t,unsigned b,radMemorySpace s):file(f),target(t),bytes(b),space(s){}
};
#define rAssert assert
''' + function(files, 'void radFile::ReadAsync') + r'''
bool IsStringKey(const char*){return false;}
#define rAssertMsg(condition,message) assert(condition)
struct FileOpenRequest {
 radFile* m_pFile;int m_Flags=OpenExisting;bool m_WriteAccess=false;
 radDrive::CompletionStatus DoRequest();
 radDrive::CompletionStatus handleError(radDrive::CompletionStatus status,int){return status;}
};
''' + function(requests, 'radDrive::CompletionStatus FileOpenRequest::DoRequest') + r'''
radFilePriority capturedPriority=LowPriority;
void radFileOpen(void**,const char*,bool,int,radFilePriority priority,int,int,int){capturedPriority=priority;}
void AudioOpen(bool high){
 void* m_refIRadFile=nullptr;const char* fileName="music.rsd";
 struct {struct {bool m_HighPriority;}m_InitializedInfo;}m_StateInfo{{high}};
 const int RADMEMORY_ALLOC_TEMP=0,radMemorySpace_Local=0;
''' + audio_open + r'''
}
int main(){
 for(bool high:{false,true}){
  AudioOpen(high);
#ifdef SRR2_OLD_3DS
  assert(capturedPriority==(high?HighPriority:NormalPriority));
#else
  assert(capturedPriority==NormalPriority);
#endif
 }
#ifdef SRR2_OLD_3DS
 radDrive disk;radFile file{&disk};FileOpenRequest open{&file};
 assert(open.DoRequest()==radDrive::MoreToDo);
 assert(!file.m_IsOpen && !archiveLookups && !file.m_FileCache.initialized);
 disk.result=radDrive::Complete;assert(open.DoRequest()==radDrive::Complete);
 assert(file.m_IsOpen && file.m_FileCache.initialized==1 && !archiveLookups);
#endif
 radDriveThread queue;radRequest scenery,music;queue.QueueRequest(&scenery,NormalPriority,false);
 radFilePriority priority;auto* request=queue.NextRequest(&priority);auto* parent=&queue;
 assert(request==&scenery && priority==NormalPriority);
 switch(radDrive::MoreToDo){
''' + requeue + r'''
 default:assert(false);
 }
 AudioOpen(true);queue.QueueRequest(&music,capturedPriority,false);
#ifdef SRR2_OLD_3DS
 assert(queue.NextRequest(&priority)==&music && priority==HighPriority);
 assert(queue.NextRequest(&priority)==&scenery && priority==NormalPriority);
#else
 assert(queue.NextRequest(&priority)==&scenery && priority==NormalPriority);
 assert(queue.NextRequest(&priority)==&music && priority==NormalPriority);
#endif
 assert(!queue.NextRequest(nullptr));
 // Subsequent stream refills retain priority, rather than only the initial open.
 radDrive streamingDrive;streamingDrive.queue=&queue;radFile stream{&streamingDrive};
 AudioOpen(true);stream.m_Priority=capturedPriority;unsigned char data[32768];
 for(unsigned refill=1;refill<=64;++refill){
  queue.QueueRequest(&scenery,NormalPriority,false);
  stream.ReadAsync(data,refill*256,0);
#ifdef SRR2_OLD_3DS
  auto* read=static_cast<FileReadRequest*>(queue.NextRequest(&priority));
  assert(priority==HighPriority);
#else
  assert(queue.NextRequest(&priority)==&scenery && priority==NormalPriority);
  auto* read=static_cast<FileReadRequest*>(queue.NextRequest(&priority));
  assert(priority==NormalPriority);
#endif
  assert(read->file==&stream && read->target==data && read->bytes==refill*256 && read->space==0);
  delete read;
#ifdef SRR2_OLD_3DS
  assert(queue.NextRequest(&priority)==&scenery && priority==NormalPriority);
#endif
  assert(!queue.NextRequest(nullptr));
 }
 puts("PASS: actual priority argument, pending-open boundary and requeued drive scheduling.");
}
'''
with tempfile.TemporaryDirectory(prefix='loading-audio-queue-') as directory:
    folder = Path(directory)
    (folder/'test.cpp').write_text(program)
    for flags in ([], ['-DSRR2_OLD_3DS', '-DRAD_3DS', '-DSRR2_OLD3DS_WORLD_RESIDENCY']):
        subprocess.run(['clang++', '-std=c++17', '-include', 'initializer_list',
                        '-fsanitize=address,undefined', *flags, str(folder/'test.cpp'),
                        '-o', str(folder/'test')], check=True)
        subprocess.run([str(folder/'test')], check=True)
