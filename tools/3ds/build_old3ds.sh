#!/usr/bin/env bash
# Build from source without installing packages into the system toolchain.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
if [[ ! -s "$repo/romfs/art/frontend/scrooby/bootup.p3d" ]]; then
    echo 'Missing boot assets. Run tools/3ds/setup_sd.py with your PC game and --old3ds first.' >&2
    exit 1
fi
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
jobs=${JOBS:-8}

# Some upstream custom commands do not quote shell metacharacters in paths.
# Use a stable, safe alias while retaining all files in the checkout.
alias_root="${TMPDIR:-/tmp}/srr2-old3ds-$(id -u)"
mkdir -p "$alias_root" "$repo/out/deps"
if [[ -L "$alias_root/source" ]]; then
    [[ "$(readlink "$alias_root/source")" == "$repo" ]] || {
        echo "Build alias belongs to another checkout: $alias_root/source" >&2; exit 1;
    }
elif [[ -e "$alias_root/source" ]]; then
    echo "Build alias already exists: $alias_root/source" >&2; exit 1
else
    ln -s "$repo" "$alias_root/source"
fi
source_root="$alias_root/source"
deps="$source_root/out/deps"
prefix="$deps/old3ds-prefix"
toolchain="$DEVKITPRO/cmake/3DS.cmake"

fetch() {
    if [[ ! -d "$deps/$1/.git" ]]; then
        git clone --depth 1 --branch "$2" "$3" "$deps/$1"
    fi
}
fetch zlib v1.3.1 https://github.com/madler/zlib.git
fetch libpng v1.6.53 https://github.com/pnggroup/libpng.git
fetch SDL release-2.32.10 https://github.com/libsdl-org/SDL.git

common=(-G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=$prefix")
cmake -S "$deps/zlib" -B "$deps/old3ds-zlib-build" "${common[@]}" -DZLIB_BUILD_EXAMPLES=OFF
cmake --build "$deps/old3ds-zlib-build" -j "$jobs"
cmake --install "$deps/old3ds-zlib-build"
zlib="$prefix/lib/libzlibstatic.a"
cmake -S "$deps/libpng" -B "$deps/old3ds-png-build" "${common[@]}" \
    "-DZLIB_LIBRARY=$zlib" "-DZLIB_INCLUDE_DIR=$prefix/include" \
    -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_ARM_NEON=off
cmake --build "$deps/old3ds-png-build" -j "$jobs"
cmake --install "$deps/old3ds-png-build"
cmake -S "$deps/SDL" -B "$deps/old3ds-sdl-build" "${common[@]}" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
cmake --build "$deps/old3ds-sdl-build" -j "$jobs"
cmake --install "$deps/old3ds-sdl-build"

bash "$source_root/tools/3ds/build_old3ds_ffmpeg.sh" "$source_root" "$jobs"

python3 "$source_root/tools/3ds/prepare_release_textures.py" \
    --data "$repo/sdcard/3ds/simpsons" --out "$repo/out/native-textures" \
    --encoder "$DEVKITPRO/tools/bin/tex3ds"
cp "$repo/out/native-textures/native-ui.bundle" "$repo/sdcard/3ds/simpsons/native-ui.bundle"

build="$source_root/out/old3ds-build"
cmake -S "$source_root" -B "$build" -C "$source_root/cmake/old3ds-release.cmake" "${common[@]}" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DSRR2_OLD_3DS=ON -DSRR2_3DS_MOVIES=ON \
    "-DSRR2_OLD3DS_NATIVE_BUNDLE_INDEX:FILEPATH=$repo/out/native-textures/bundle_index.hpp" \
    "-DSDL2_DIR=$prefix/lib/cmake/SDL2" \
    "-DPNG_LIBRARY=$prefix/lib/liblibpng16_static.a" "-DPNG_PNG_INCLUDE_DIR=$prefix/include" \
    "-DZLIB_LIBRARY=$zlib" "-DZLIB_INCLUDE_DIR=$prefix/include" "$@"
cmake --build "$build" -j "$jobs"
mkdir -p "$repo/out/old3ds"
attributes=$("$DEVKITPRO/devkitARM/bin/arm-none-eabi-readelf" -A "$build/code/SRR2.elf")
printf '%s\n' "$attributes" > "$repo/out/old3ds/elf-attributes.txt"
[[ "$attributes" == *"Tag_CPU_arch: v6K"* && "$attributes" == *"Tag_FP_arch: VFPv2"* ]] || {
    echo "Unexpected executable architecture; inspect out/old3ds/elf-attributes.txt" >&2
    exit 1
}
python3 "$repo/tools/3ds/check_old3ds_memory.py" "$build/code/SRR2.elf" > "$repo/out/old3ds/memory.json"
cp "$build/SimpsonsNTSC.3dsx" "$repo/out/old3ds/SimpsonsNTSC.3dsx"
echo "Built: $repo/out/old3ds/SimpsonsNTSC.3dsx"
