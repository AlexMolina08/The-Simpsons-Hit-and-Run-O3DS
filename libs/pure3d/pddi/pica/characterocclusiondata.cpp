#include <pddi/pica/characterocclusiondata.hpp>
#include <pddi/pica/characterocclusion.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace picaCharacterOcclusion {
const unsigned char* Data(unsigned& size)
{
    struct Prepared {
        unsigned char* bytes;
        unsigned size;
        Prepared():bytes(nullptr),size(0)
        {
            FILE* file=fopen("romfs:/character-occlusion.bin","rb");
            if(!file)return;
            if(fseek(file,0,SEEK_END)!=0){fclose(file);return;}
            const long length=ftell(file);
            if(length<16 || length>256*1024 || fseek(file,0,SEEK_SET)!=0){fclose(file);return;}
            unsigned char* loaded=static_cast<unsigned char*>(malloc(static_cast<size_t>(length)));
            if(loaded && fread(loaded,1,static_cast<size_t>(length),file)==static_cast<size_t>(length) &&
               !memcmp(loaded,"SRRAO001",8) && Word(loaded+12)==static_cast<unsigned>(length) &&
               Word(loaded+8)<=static_cast<unsigned>(length-16)/20)
            {
                bytes=loaded;
                size=static_cast<unsigned>(length);
            }
            else free(loaded);
            fclose(file);
        }
        ~Prepared(){free(bytes);}
    };
    static const Prepared prepared;
    size=prepared.size;
    return prepared.bytes;
}
}
