#!/usr/bin/env python3
"""Verify benchmark fixture copies never replace personal or earlier saves."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[3];text=(root/'code/main/nativebench.cpp').read_text();code=text[text.index('static unsigned saveFixture='):text.index('static char runIdentity[64]')]
with tempfile.TemporaryDirectory(prefix='srr-bench-saves-') as d:
 p=Path(d)
 test='''#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cassert>
#include <string>
unsigned long long osGetTime(){return 0x1234;}
'''+code+r'''
std::string read(const char* p){FILE* f=fopen(p,"rb");assert(f);std::string s;int c;while((c=fgetc(f))!=EOF)s+=char(c);fclose(f);return s;}
void write(const char* p,const char* bytes){FILE* f=fopen(p,"wb");assert(f);fwrite(bytes,1,strlen(bytes),f);fclose(f);}
int main(){
 write("Save1","personal bytes\n");write("Save4","");assert(PrepareSaveFixtures());
 char first[32],next[32];SaveFilename(first,sizeof first,0);assert(read(first)==read("Save1"));assert(fixtureBytes[0]==15);assert(fixtureBytes[3]==0);
 assert(read("Save1")=="personal bytes\n");write(first,"changed by test run");
 assert(PrepareSaveFixtures());SaveFilename(next,sizeof next,0);assert(strcmp(first,next));assert(read(first)=="changed by test run");assert(read(next)==read("Save1"));
 assert(read("Save1")=="personal bytes\n");assert(mkdir("Save3",0700)==0);assert(!PrepareSaveFixtures());assert(read("Save1")=="personal bytes\n");
 puts("PASS: byte-identical per-launch fixtures, collision avoidance, absent/empty slots, read-error abort and personal/prior-save preservation.");
}
'''
 (p/'test.cpp').write_text(test);subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=p,check=True)
