#ifndef RADMATH_OLD3DS_VECTOR_MATRIX_HPP
#define RADMATH_OLD3DS_VECTOR_MATRIX_HPP
namespace old3dsMatrix {
// Keep VFP callee-save traffic out of the ordinary scalar fallback.
__attribute__((noinline)) static bool TryMultiply(float* out,const float* a,const float* b)
{
 unsigned original, control, finalControl;
 __asm__ volatile("vmrs %0, fpscr":"=r"(original));
 // Short-vector NaN propagation differs from scalar mode without default NaNs.
 // Use only round-to-nearest in the application's ordinary run-fast mode.
 // Preserve every other
 // control bit and the accumulated exception flags; never call C++ with LEN set.
 if((original & 0x03f79f00u) != 0x03000000u || out == a || out == b) return false;
 control=(original&~0x00370000u)|0x00020000u;
 __asm__ volatile(
 "vmsr fpscr, %[control]\n"
 "vldr s8, [%[b], #0]\n vldr s9, [%[b], #4]\n vldr s10, [%[b], #8]\n"
 "vldr s12, [%[b], #16]\n vldr s13, [%[b], #20]\n vldr s14, [%[b], #24]\n"
 "vldr s16, [%[b], #32]\n vldr s17, [%[b], #36]\n vldr s18, [%[b], #40]\n"
 "vldr s0, [%[a], #0]\n vldr s1, [%[a], #4]\n vldr s2, [%[a], #8]\n"
 "vmul.f32 s24, s8, s0\n vmul.f32 s20, s12, s1\n vadd.f32 s24, s24, s20\n vmul.f32 s20, s16, s2\n vadd.f32 s24, s24, s20\n"
 "vstr s24, [%[out], #0]\n vstr s25, [%[out], #4]\n vstr s26, [%[out], #8]\n"
 "vldr s0, [%[a], #16]\n vldr s1, [%[a], #20]\n vldr s2, [%[a], #24]\n"
 "vmul.f32 s24, s8, s0\n vmul.f32 s20, s12, s1\n vadd.f32 s24, s24, s20\n vmul.f32 s20, s16, s2\n vadd.f32 s24, s24, s20\n"
 "vstr s24, [%[out], #16]\n vstr s25, [%[out], #20]\n vstr s26, [%[out], #24]\n"
 "vldr s0, [%[a], #32]\n vldr s1, [%[a], #36]\n vldr s2, [%[a], #40]\n"
 "vmul.f32 s24, s8, s0\n vmul.f32 s20, s12, s1\n vadd.f32 s24, s24, s20\n vmul.f32 s20, s16, s2\n vadd.f32 s24, s24, s20\n"
 "vstr s24, [%[out], #32]\n vstr s25, [%[out], #36]\n vstr s26, [%[out], #40]\n"
 "vldr s0, [%[a], #48]\n vldr s1, [%[a], #52]\n vldr s2, [%[a], #56]\n"
 "vmul.f32 s24, s8, s0\n vmul.f32 s20, s12, s1\n vadd.f32 s24, s24, s20\n vmul.f32 s20, s16, s2\n vadd.f32 s24, s24, s20\n"
 "vldr s20, [%[b], #48]\n vldr s21, [%[b], #52]\n vldr s22, [%[b], #56]\n vadd.f32 s24, s24, s20\n"
 "vstr s24, [%[out], #48]\n vstr s25, [%[out], #52]\n vstr s26, [%[out], #56]\n"
 "vmrs %[final], fpscr\n"
 : [final]"=r"(finalControl)
 :[out]"r"(out),[a]"r"(a),[b]"r"(b),[control]"r"(control)
 :"s0","s1","s2","s8","s9","s10","s12","s13","s14","s16","s17","s18","s20","s21","s22","s24","s25","s26","memory");
 finalControl=(finalControl&~0x00370000u)|(original&0x00370000u);
 __asm__ volatile("vmsr fpscr, %0"::"r"(finalControl):"memory");
 out[3]=out[7]=out[11]=0;out[15]=1;
 return true;
}
}
#endif
