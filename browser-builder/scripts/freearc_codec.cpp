#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include "Compression/Compression.h"
#include "Compression/GRZip/libGRZip.h"
int GRZip_CheckBlockSign(uint8*,sint32);
extern "C" int GRZip_DecompressBlock(uint8*,sint32,uint8*);
struct IO { FILE* in;FILE* out; uint64_t read=0,written=0; };
static int callback(const char* what,void* bytes,int length,void* userdata){
 IO& io=*static_cast<IO*>(userdata);
 if(!strcmp(what,"read")){int n=fread(bytes,1,length,io.in);io.read+=n;return n;}
 if(!strcmp(what,"write")){int n=fwrite(bytes,1,length,io.out);io.written+=n;return n==length?n:FREEARC_ERRCODE_WRITE;}
 return FREEARC_ERRCODE_NOT_IMPLEMENTED;
}
static bool readall(FILE* input,void* dst,size_t bytes){return fread(dst,1,bytes,input)==bytes;}
static int32_t number(const unsigned char* p){return int32_t(uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24);}
static int sequential4x4(const char* method,IO& io){
 unsigned char header[8];if(!readall(io.in,header,4)||number(header)!=0)return -7;
 std::unique_ptr<COMPRESSION_METHOD> codec(ParseCompressionMethod(const_cast<char*>(method)));
 if(!codec)return FREEARC_ERRCODE_INVALID_COMPRESSOR;
 void* state=nullptr;int result=0;
 while(true){
  size_t n=fread(header,1,8,io.in);if(!n)break;if(n!=8){result=-7;break;}
  int32_t output=number(header),input=number(header+4);
  if(input<0||input>256*1024*1024||output< -1||output>256*1024*1024){result=-7;break;}
  std::vector<unsigned char> source(size_t(input)+32),target(size_t(output<0?input:output)+32);
  if(!readall(io.in,source.data(),input)){result=-7;break;}
  if(output==-1){if(callback("write",source.data(),input,&io)<0){result=-11;break;}}
  else{int expected=output;result=codec->DeCompressMem(DECOMPRESS,source.data(),input,target.data(),&expected,callback,&io,&state);if(result<0||expected!=output){if(result>=0)result=-7;break;}if(callback("write",target.data(),output,&io)<0){result=-11;break;}}
 }
 if(state)codec->DeCompressMem(DECOMPRESS,nullptr,0,nullptr,nullptr,nullptr,nullptr,&state);
 return result;
}
static int sequentialGRZip(IO& io){
 unsigned char header[28];
 while(true){size_t n=fread(header,1,28,io.in);if(!n)return 0;if(n!=28)return -7;
  int32_t input=number(header+16),output=number(header);if(input<0||input>64*1024*1024||output<0||output>64*1024*1024)return -7;
  if(GRZip_CheckBlockSign(header,28)!=GRZ_NO_ERROR)return -7;
  std::vector<unsigned char> src(size_t(input)+28+32),dst(size_t(output)+32);memcpy(src.data(),header,28);
  if(!readall(io.in,src.data()+28,input))return -7;
  int count=GRZip_DecompressBlock(src.data(),input+28,dst.data());if(count!=output)return -7;
  if(callback("write",dst.data(),count,&io)<0)return -11;
 }
}
int main(int argc,char**argv){
 if(argc!=4){fprintf(stderr,"Usage: freearc-codec METHOD INPUT OUTPUT\n");return 2;}
 IO io{fopen(argv[2],"rb"),fopen(argv[3],"wb")};if(!io.in||!io.out)return 3;
 const char* method=argv[1];int result;
 if(!strncmp(method,"4x4:",4))result=sequential4x4(method+4,io);
 else if(!strncmp(method,"grzip",5))result=sequentialGRZip(io);
 else{std::unique_ptr<COMPRESSION_METHOD> codec(ParseCompressionMethod(const_cast<char*>(method)));result=codec?codec->decompress(callback,&io):FREEARC_ERRCODE_INVALID_COMPRESSOR;}
 fclose(io.in);if(fclose(io.out)&&result>=0)result=FREEARC_ERRCODE_WRITE;
 if(result<0)fprintf(stderr,"FreeArc decoding failed (%d)\n",result);
 return result<0?1:0;
}
