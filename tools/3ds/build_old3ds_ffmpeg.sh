#!/usr/bin/env bash
# Build only the ARM11 MPEG-1 decoder and its parser; no desktop media stack.
set -euo pipefail
source_root=${1:?Pass the shell-safe source alias from build_old3ds.sh}
jobs=${2:-8}
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
deps="$source_root/out/deps"
source_dir="$deps/ffmpeg"
build_dir="$deps/old3ds-ffmpeg-build"
prefix="$deps/old3ds-prefix"
revision=db69d06eeeab4f46da15030a80d539efb4503ca8

if [[ ! -d "$source_dir/.git" ]]; then
    git clone --depth 1 --branch n7.1.1 https://github.com/FFmpeg/FFmpeg.git "$source_dir"
fi
[[ "$(git -C "$source_dir" rev-parse HEAD)" == "$revision" ]] || {
    echo "Unexpected FFmpeg dependency revision in $source_dir" >&2; exit 1;
}
patch="$source_root/tools/3ds/ffmpeg-newlib.patch"
if git -C "$source_dir" apply --check "$patch" 2>/dev/null; then
    git -C "$source_dir" apply "$patch"
else
    git -C "$source_dir" apply --reverse --check "$patch"
fi

mkdir -p "$build_dir"
cd "$build_dir"
CPPFLAGS= CFLAGS= LDFLAGS= "$source_dir/configure" \
    "--prefix=$prefix" --arch=arm --cpu=mpcore --target-os=none \
    --enable-cross-compile "--cross-prefix=$DEVKITPRO/devkitARM/bin/arm-none-eabi-" \
    --disable-everything --disable-autodetect --disable-programs --disable-doc \
    --disable-network --disable-avdevice --disable-avformat --disable-avfilter \
    --disable-swresample --disable-swscale --disable-postproc --disable-debug \
    --disable-pthreads --disable-w32threads --disable-os2threads \
    --disable-runtime-cpudetect --disable-neon --disable-armv6t2 --disable-vfpv3 \
    --enable-decoder=mpeg1video --enable-parser=mpegvideo --enable-static --disable-shared \
    --optflags='-O3 -fomit-frame-pointer' \
    "--extra-cflags=-march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp -mtp=soft -fno-short-enums -ffunction-sections -fdata-sections -I$DEVKITPRO/libctru/include -D_3DS -D__3DS__" \
    "--extra-ldflags=-march=armv6k -mfloat-abi=hard -mfpu=vfp -specs=3dsx.specs -L$DEVKITPRO/libctru/lib" \
    --extra-libs='-lctru -lm'
make -j "$jobs"
make install
