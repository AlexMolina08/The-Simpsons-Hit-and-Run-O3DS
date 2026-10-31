#!/usr/bin/env python3
"""Execute the production legacy-option guard before long stress-plan parsing."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/main/commandlineoptions.cpp').read_text()
a=source.index('    char option[ 256 ];');b=source.index('    rReleasePrintf( "Commandline Option:',a)
code='''#include <cassert>
#include <cstring>
#include <string>
static bool accepted=false;
void guard(const char* optionIn){
'''+source[a:b]+'''accepted=true;assert(!strcmp(option,optionIn));}
int main(){
 for(const char* shortOption:{"L7","skipfe","bench_world_residency=4"}){accepted=false;guard(shortOption);assert(accepted);}
 for(unsigned size:{256u,273u,1024u}){accepted=false;std::string value(size,'x');guard(value.c_str());assert(!accepted && value.size()==size);}
 accepted=false;guard("");assert(!accepted);guard(nullptr);assert(!accepted);
}
'''
with tempfile.TemporaryDirectory(prefix='command-line-guard-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: short options preserved, long stress arguments left intact, empty/null inputs rejected before copying.')
