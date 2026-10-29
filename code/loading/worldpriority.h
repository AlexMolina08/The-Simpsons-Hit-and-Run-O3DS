#ifndef SRR_WORLD_PRIORITY_H
#define SRR_WORLD_PRIORITY_H
#include <cstring>

namespace WorldPriority {
inline bool Exterior(const char* filename)
{
    if(!filename)return false;
    const char* name=filename;
    for(const char* p=filename;*p;++p)if(*p=='/' || *p=='\\')name=p+1;
    const unsigned length=unsigned(std::strlen(name));
    if(length<8 || (name[0]!='l' && name[0]!='L') || name[1]<'1' || name[1]>'7' ||
       (name[2]!='r' && name[2]!='R' && name[2]!='z' && name[2]!='Z') ||
       name[3]<'0' || name[3]>'9')return false;
    const char* suffix=name+length-4;
    return suffix[0]=='.' && (suffix[1]=='p' || suffix[1]=='P') && suffix[2]=='3' &&
           (suffix[3]=='d' || suffix[3]=='D');
}

// The active request's address is the loader callback identity. Never move it.
// Existing urgent files retain their authored order; other requests and callback
// barriers retain their relative order when the new world file passes them.
template<class Request,unsigned Capacity,class IsWorld>
unsigned Promote(Request (&requests)[Capacity],unsigned head,unsigned tail,
                 bool loading,IsWorld isWorld)
{
    const unsigned newest=(tail+Capacity-1)%Capacity;
    unsigned insertion=(head+unsigned(loading))%Capacity;
    while(insertion!=newest && isWorld(requests[insertion]))insertion=(insertion+1)%Capacity;
    Request incoming=requests[newest];unsigned moved=0;
    for(unsigned cursor=newest;cursor!=insertion;cursor=(cursor+Capacity-1)%Capacity) {
        requests[cursor]=requests[(cursor+Capacity-1)%Capacity];++moved;
    }
    requests[insertion]=incoming;
    return moved;
}
}
#endif
