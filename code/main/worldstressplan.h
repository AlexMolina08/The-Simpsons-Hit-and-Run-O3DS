#ifndef SRR_WORLD_STRESS_PLAN_H
#define SRR_WORLD_STRESS_PLAN_H
#include <cstring>

namespace WorldStress {
// Diagnostic-only mission-select sequence. Story mission numbers are 1..7.
// Level 1 also accepts mission 0 for its tutorial.
// Level 1 has an extra tutorial slot before its seven story missions.
// Parsing is transactional: malformed input cannot leave a partial itinerary.
struct Plan {
    static constexpr unsigned Capacity=64;
    unsigned levels[Capacity]={0,1},missions[Capacity]={1,1};
    unsigned count=2,dwellMs=10000;
    bool custom=false,resetMission=false,teleport=false,interior=false,activeMission=false;
    bool Parse(const char* text) {
        unsigned nextLevels[Capacity]={},nextMissions[Capacity]={},size=0;
        if(!text || !*text)return false;
        for(;;){
            if(size==Capacity || text[0]<'1' || text[0]>'7' || text[1]!=':' || text[2]<(text[0]=='1'?'0':'1') || text[2]>'7')return false;
            nextLevels[size]=text[0]-'1';nextMissions[size]=text[2]-'1'+(text[0]=='1'?1:0);++size;text+=3;
            if(!*text)break;
            if(*text++!=',' || !*text)return false;
        }
        std::memcpy(levels,nextLevels,sizeof(levels));std::memcpy(missions,nextMissions,sizeof(missions));count=size;custom=true;return true;
    }
    unsigned MissionNumber(unsigned i) const {return missions[i]+1-(levels[i]==0?1:0);}
    unsigned WatchdogSeconds() const {return custom?180+120*count:300;}
};
// Bound one blocked destination independently of the complete itinerary.
// A long mission matrix must not spend its entire budget on one retry screen.
struct PhaseWatchdog {
    static constexpr unsigned Seconds=300;
    unsigned observed=~0u;
    unsigned long long started=0;
    bool Expired(unsigned phase,unsigned long long now,unsigned frequency) {
        if(observed!=phase){observed=phase;started=now;return false;}
        return frequency && now-started>static_cast<unsigned long long>(Seconds)*frequency;
    }
};
// ZoneEventLocator::SetZone splits its input in place. GetZone() therefore
// exposes only the first token after parsing. Rebuild all exterior operations
// through the public arrays before cloning a locator for queued callbacks.
template<class Zone> bool ExteriorZoneText(Zone* zone,char (&text)[254]) {
    unsigned used=0;text[0]=0;
    auto append=[&](const char* name,char operation){
        if(!name || !*name)return false;
        const unsigned length=unsigned(std::strlen(name));
        if(used+length+1>=sizeof(text))return false;
        std::memcpy(text+used,name,length);used+=length;
        text[used++]=operation;text[used]=0;return true;
    };
    if(!zone || zone->IsInteriorLoad() || zone->IsInteriorDump())return false;
    for(int i=0;i<zone->GetNumLoadZones();++i)if(!append(zone->GetLoadZone(i),';'))return false;
    for(int i=0;i<zone->GetNumDumpZones();++i)if(!append(zone->GetDumpZone(i),':'))return false;
    for(int i=0;i<zone->GetNumLWSActivates();++i)if(!append(zone->GetLWSActivates(i),'*'))return false;
    for(int i=0;i<zone->GetNumLWSDeactivates();++i)if(!append(zone->GetLWSDeactivates(i),'&'))return false;
    return used!=0;
}
struct FrameStats {
    unsigned long long period=0,work=0;
    unsigned samples=0,over50=0,maximumUs=0,linearMinimum=~0u;
    unsigned histogram[128]={};
    unsigned resets=0,teleports=0,interiors=0;
    unsigned missionActive=0,streamingFrames=0,vehiclesPeak=0,stereoFrames=0,stereoSegments=0;
    int stageFirst=-1,stageLast=-1,stageMaximum=-1;
    unsigned residentExpected=0,residentBound=0,residentBad=0,residentWanted=0;
    void Sample(unsigned long long elapsedTicks,unsigned long long workTicks,unsigned frequency,unsigned linear,unsigned requests=0,unsigned vehicles=0,int stage=-1) {
        if(!elapsedTicks || !frequency)return;
        const unsigned us=unsigned(elapsedTicks*1000000/frequency);
        period+=elapsedTicks;work+=workTicks;++samples;over50+=us>50000;
        streamingFrames+=requests!=0;if(vehicles>vehiclesPeak)vehiclesPeak=vehicles;
        if(stage>=0){if(stageFirst<0)stageFirst=stage;stageLast=stage;if(stage>stageMaximum)stageMaximum=stage;}
        if(us>maximumUs)maximumUs=us;
        if(linear<linearMinimum)linearMinimum=linear;
        const unsigned bin=us/2000; ++histogram[bin<128?bin:127];
    }
    unsigned PercentileUs(unsigned percentile) const {
        if(!samples)return 0;
        const unsigned target=(samples*percentile+99)/100;unsigned found=0;
        for(unsigned i=0;i<128;++i)if((found+=histogram[i])>=target)return i==127?maximumUs:(i+1)*2000;
        return maximumUs;
    }
};
}
#endif
