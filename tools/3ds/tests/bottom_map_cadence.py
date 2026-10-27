#!/usr/bin/env python3
"""Check presentation deadlines, missed-frame recovery and lifecycle invalidation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='srr-bottom-map-') as directory:
 p=Path(directory);source=p/'test.cpp'
 source.write_text('''#include <main/handheldhudcadence.h>
#include <cassert>
int main(){
 HandheldHudDeadline d;
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
 HandheldHudAdaptive a;
 for(unsigned i=0;i<7;++i)a.Observe(42000);assert(a.Hz()==15);
 a.Observe(42000);assert(a.Hz()==10);
 for(unsigned i=0;i<100;++i)a.Observe(i&1?34500:35500);
 assert(a.Hz()==10); // Hysteresis avoids repeated rate changes near the budget.
 for(unsigned i=0;i<100;++i)a.Observe(28000);assert(a.Hz()==15);
 a.Observe(10000000); // Bound a suspended/debug frame before filtering.
 a.Reset();assert(a.Hz()==15);
 a.SetMinimum(8);for(unsigned i=0;i<8;++i)a.Observe(42000);assert(a.Hz()==8);
 a.SetMinimum(1);assert(a.Hz()==8);
 a.Reset();for(unsigned i=0;i<8;++i)a.Observe(42000);assert(a.Hz()==8);
 a.SetMinimum(10);assert(a.Hz()==10);
}
''')
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'code'),str(source),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: absolute deadlines, missed-render retry, no catch-up burst, lifecycle reset and long suspension recovery.')

with tempfile.TemporaryDirectory(prefix='srr-bottom-map-policy-') as directory:
 p=Path(directory);(p/'main').mkdir()
 (p/'3ds.h').write_text('#pragma once\n#include <cstdint>\nextern uint64_t now;constexpr unsigned SYSCLOCK_ARM11=1000000;inline uint64_t svcGetSystemTick(){return now;}\n')
 (p/'main/graphics3ds.h').write_text('#pragma once\nnamespace Graphics3DS{extern bool newModel;inline bool IsNew(){return newModel;}}\n')
 (p/'test.cpp').write_text('''#include <main/handheldhudcadence.h>
#include <cassert>
uint64_t now=0;namespace Graphics3DS{bool newModel=false;}
int main(){using namespace HandheldHudCadence;
 cadenceMode=1;Reset();assert(Due());Rendered();
 now=99999;assert(!Due());ForceUpdate();assert(Due());Rendered();
 SetUrgent(true);ForceUpdate();assert(Due());Rendered();
 now+=66666;assert(Due());Rendered();
 SetUrgent(false);cadenceMode=0;Reset();assert(Due());Rendered();
 now+=66666;assert(Due());
 cadenceMode=1;Graphics3DS::newModel=true;Reset();assert(Due());Rendered();
 now+=66666;assert(Due());
 Graphics3DS::newModel=false;cadenceMode=2;ObserveFrame(0,false);
 for(unsigned i=0;i<8;++i)ObserveFrame(42000,true);
 Reset();assert(Due());Rendered();now+=99999;assert(!Due());
 ObserveFrame(0,false);Reset();assert(Due());Rendered();
 now+=66666;assert(Due());
 minimumHz=8;ObserveFrame(0,false);
 for(unsigned i=0;i<8;++i)ObserveFrame(42000,true);
 Reset();assert(Due());Rendered();now+=124999;assert(!Due());++now;assert(Due());
 SetUrgent(true);ForceUpdate();assert(Due());Rendered();now+=66666;assert(Due());
 boostPickupAnimations=false;ForceUpdate();assert(Due());Rendered();
 now+=66666;assert(!Due());ForceUpdate();assert(Due());Rendered();
 now+=124999;assert(!Due());++now;assert(Due());
 Graphics3DS::newModel=true;SetUrgent(false);Reset();assert(Due());Rendered();
 now+=66666;assert(Due());
}
''')
 subprocess.run(['c++','-std=c++17','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_BOTTOM_MAP_15HZ','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),str(root/'code/main/handheldhudcadence.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: diagnostic 10/15Hz control, immediate priority redraws, urgent-animation cadence, New3DS protection and adaptive lifecycle reset.')

with tempfile.TemporaryDirectory(prefix='srr-bottom-map-release-') as directory:
 p=Path(directory);(p/'main').mkdir()
 (p/'3ds.h').write_text('#pragma once\n#include <cstdint>\nextern uint64_t now;constexpr unsigned SYSCLOCK_ARM11=1000000;inline uint64_t svcGetSystemTick(){return now;}\n')
 (p/'main/graphics3ds.h').write_text('#pragma once\nnamespace Graphics3DS{extern bool newModel;inline bool IsNew(){return newModel;}}\n')
 (p/'test.cpp').write_text('''#include <main/handheldhudcadence.h>
#include <cassert>
uint64_t now=0;namespace Graphics3DS{bool newModel=false;}
int main(){using namespace HandheldHudCadence;
 assert(cadenceMode==2 && minimumHz==8);
 for(unsigned i=0;i<8;++i)ObserveFrame(42000,true);
 Reset();assert(Due());Rendered();now=124999;assert(!Due());++now;assert(Due());
 Graphics3DS::newModel=true;Reset();assert(Due());Rendered();now+=66666;assert(Due());
}
''')
 subprocess.run(['c++','-std=c++17','-DSRR2_OLD3DS_BOTTOM_MAP_15HZ','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),str(root/'code/main/handheldhudcadence.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: ordinary Old 3DS defaults to adaptive 8/15Hz; New 3DS retains 15Hz.')
