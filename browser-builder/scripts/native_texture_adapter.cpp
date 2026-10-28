// C ABI adapter for the existing production texture conversion functions.
// The build script generates the surrounding translation unit from current port sources.
extern "C" int convert_texture(unsigned w,unsigned h,unsigned kind,unsigned dw,unsigned dh,
 unsigned format,unsigned levels,const unsigned char* source,unsigned size,
 unsigned char* destination,unsigned capacity) {
 if(!w || w>1024 || !h || h>1024 || kind>5 || dw<8 || dw>1024 ||
    dh<8 || dh>1024 || (dw&(dw-1)) || (dh&(dh-1)) || levels>7 ||
    (dw>>levels)<8 || (dh>>levels)<8 || format<1 || format>3 ||
    !source || !destination ||
    size!=(kind?((w+3)/4)*((h+3)/4)*(kind==1?8:16):w*h*4))return -1;
 size_t bytes=0;for(unsigned i=0;i<=levels;++i)bytes+=(dw>>i)*(dh>>i)*2;
 if(capacity!=bytes)return -2;
 std::vector<unsigned> input((size+3)/4);
 memcpy(input.data(),source,size);
 std::vector<unsigned char> output(bytes+32,0xa5);
 const int gpu=format==1?GPU_RGB565:format==2?GPU_RGBA5551:GPU_RGBA4;
 picaTexture texture{int(w),int(h),int(kind),gpu,{int(dw),int(dh),int(levels),output.data()},
    reinterpret_cast<unsigned char*>(input.data()),false};
 texture.UploadLevel(0);if(!texture.valid)return -3;
 for(size_t i=bytes;i<output.size();++i)if(output[i]!=0xa5)return -4;
 memcpy(destination,output.data(),bytes);
 return int(bytes);
}
