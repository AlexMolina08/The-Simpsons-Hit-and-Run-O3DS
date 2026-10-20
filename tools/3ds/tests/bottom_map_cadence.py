#!/usr/bin/env python3
"""Check presentation deadlines, missed-frame recovery and lifecycle invalidation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='srr-bottom-map-') as directory:
 p=Path(directory);source=p/'test.cpp'
 source.write_text('''#include <main/old3dsbottommap.h>
#include <cassert>
int main(){
 Old3dsBottomMapDeadline d;
 assert(d.Due(0));d.Rendered(0,10);
 for(unsigned t=1;t<10;++t)assert(!d.Due(t));
 assert(d.Due(10));d.Rendered(10,10);assert(!d.Due(19));
 // A missed render remains due until it succeeds. No catch-up render burst.
 assert(d.Due(95));assert(d.Due(96));d.Rendered(96,10);
 assert(!d.Due(99));assert(d.Due(100));
 d.Reset();assert(d.Due(97));d.Rendered(97,10);assert(!d.Due(106));
 // Menus/loading invalidate the old deadline, forcing the next map immediately.
 d.Reset();assert(d.Due(98));d.Rendered(98,10);assert(!d.Due(107));
 assert(d.Due(1000000));d.Rendered(1000000,10);assert(!d.Due(1000001));
}
''')
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'code'),str(source),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: absolute deadlines, missed-render retry, no catch-up burst, lifecycle reset and long suspension recovery.')
