# Generate stereo companions without changing the accepted mono shaders.
file(READ "${INPUT}" shader)
if(MODE STREQUAL "main")
    string(REGEX MATCHALL "\\.fvec[ \t]+[A-Za-z0-9_]+(\\[[0-9]+\\])?" vectors "${shader}")
    set(used 0)
    foreach(vector IN LISTS vectors)
        if(vector MATCHES "\\[([0-9]+)\\]")
            math(EXPR used "${used}+${CMAKE_MATCH_1}")
        else()
            math(EXPR used "${used}+1")
        endif()
    endforeach()
    math(EXPR padding "95-${used}")
    if(padding LESS 1 OR NOT shader MATCHES "mov[ \t]+r11, u_eye")
        message(FATAL_ERROR "Unsupported main shader stereo layout")
    endif()
    string(REPLACE ".entry vmain" ".fvec u_stereoPadding[${padding}]\n.fvec u_stereoParams\n.entry vmain" shader "${shader}")
    # Skin palettes overlap u_eye. Keep both stereo parameters in the shared
    # final register so world and character programs use the same screen plane.
    # Keep this affine in homogeneous clip space, including behind the camera.
    # Per-vertex disparity clamps bend coarse roads away from touching meshes.
    # The shared camera policy limits the whole frustum before vertex shading.
    set(offset "mov r11, u_stereoParams\n\tmul r12, r1.wwww, r11.wwww\n\tadd r12, r12, -r11.zzzz")
    string(REGEX REPLACE "mov[ \t]+r11, u_eye[\r\n]+[ \t]*add[ \t]+r12, r1.wwww, -r11.yyyy[\r\n]+[ \t]*mul[ \t]+r12, r12, r11.xxxx" "${offset}" shader "${shader}")
elseif(MODE STREQUAL "skin")
    if(NOT shader MATCHES "\\.fvec u_bones\\[75\\]" OR NOT shader MATCHES "\\.fvec u_eye_quant")
        message(FATAL_ERROR "Stereo skin requires the packed 25-bone eye-space palette")
    endif()
    string(REPLACE ".entry vmain" ".bool u_stereoEnabled\n.entry vmain" shader "${shader}")
    # Quantized position, normal, and UV scales are exact powers of two.
    # Decode all three from x, leaving yzw for the same bounded stereo parameters
    # used by the main program without taking a register from the 25-bone palette.
    string(REPLACE "mul r0.xyz, u_eye_quant.xxxx, in_pos" "mul r0.xyz, u_eye_quant.xxxx, in_pos\n\tadd r0.xyz, r0, r0\n\tadd r0.xyz, r0, r0" shader "${shader}")
    string(REPLACE "mul r6.xyz, u_eye_quant.yyyy, in_normal" "mul r6.xyz, u_eye_quant.xxxx, in_normal" shader "${shader}")
    string(REPLACE "mul r10.xy, u_eye_quant.zzzz, in_tex0" "mul r10.xy, u_eye_quant.xxxx, in_tex0\n\tadd r10.xy, r10, r10\n\tadd r10.xy, r10, r10\n\tadd r10.xy, r10, r10" shader "${shader}")
    set(offset "\tifu u_stereoEnabled\n\tmov r11, u_eye_quant\n\tmul r12, r1.wwww, r11.wwww\n\tadd r12, r12, -r11.zzzz\n\tadd r1.y, r1.y, -r12.x\n\t.end\n")
    string(REGEX REPLACE "mov[ \t]+o_pos, r1" "${offset}\tmov o_pos, r1" shader "${shader}")
else()
    message(FATAL_ERROR "Unknown stereo shader kind")
endif()
file(WRITE "${OUTPUT}" "${shader}")
