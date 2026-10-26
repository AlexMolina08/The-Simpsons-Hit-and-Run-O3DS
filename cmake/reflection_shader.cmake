# Extend the existing world shader while retaining its complete uniform layout.
# Two entrypoints share the original projection and lighting code. Ordinary
# geometry retains the baseline instruction stream without reflection branches.
file(READ "${INPUT}" shader)
set(original "${shader}")
if(NOT shader MATCHES "mov[ \t]+o_tex0,[ \t]+in_tex0" OR
   NOT shader MATCHES "\\.fvec u_mv\\[4\\]")
    message(FATAL_ERROR "Unsupported reflection shader source")
endif()
string(REGEX REPLACE "\\.out[ \t]+o_tex0[ \t]+texcoord0" ".out o_tex0 texcoord0 o2.xy\n.out - texcoord1 o2.zw" shader "${shader}")

set(reflection [=[
    ; PackedCoordinatesBegin
    ; UV0.xy and reflection UV1.xy share one output vector. Ordinary geometry
    ; retains the original three-register output footprint, including in stereo.
    ifu u_reflective
    ; Reuse the eye-space position and transform the existing rigid-mesh normal.
    dp3 r2.x, u_mv[0].xyz, in_normal.xyz
    dp3 r2.y, u_mv[1].xyz, in_normal.xyz
    dp3 r2.z, u_mv[2].xyz, in_normal.xyz
    dp3 r3.x, r2.xyz, r2.xyz
    max r3.x, fixclr, r3.x
    rsq r3.x, r3.x
    mul r2.xyz, r2.xyz, r3.xxx
    dp3 r3.y, r8.xyz, r8.xyz
    max r3.y, fixclr, r3.y
    rsq r3.y, r3.y
    mul r4.xyz, r8.xyz, r3.yyy
    dp3 r3.z, r2.xyz, r4.xyz
    add r3.z, r3.z, r3.z
    mad r4.xyz, -r2.xyz, r3.zzz, r4.xyz
    ; The retail spheremap shader maps reflection-vector XY into its 2D map.
    ; u_stereoOn.y carries one half without consuming a stereo/palette register.
    mov r5, u_stereoOn
    mul r4.xy, r4.xy, r5.yyyy
    add r4.x, r4.x, r5.y
    add r4.y, -r4.y, r5.y
    mov r15, in_tex0
    mov r15.zw, r4.xyxy
    ; Write every configured component of this shared register exactly once.
    mov o2, r15
    .else
    ; Ordinary geometry retains the original single UV move.
    mov o2, in_tex0
    .end
    ; PackedCoordinatesEnd
]=])
set(body_light [=[
    ; PreparedVehicleLightBegin
    ifc !cmp.x
    ; The reflection calculation already normalized the eye-space normal r2.
    mov r7, u_stereoOn
    add r6.x, r2.y, -r2.x
    mul r6.x, r6.x, r7.y
    add r6.x, r6.x, r7.y
    max r6.x, zeros, r6.x
    min r6.x, ones, r6.x
    mad r6.x, r6.x, r7.w, r7.z
    mul r0.rgb, r0.rgb, r6.xxx
    .end
    ; PreparedVehicleLightEnd
]=])
# Lighting leaves cmp.x set only for the lit path. DoLighting must preserve it,
# as the existing jmpc after that call already requires. Reflection math moves
# after color evaluation; r8 remains the original eye-space position.
string(FIND "${original}" ".proc DoLighting" lighting_start)
if(lighting_start LESS 0)
    message(FATAL_ERROR "Reflection tail requires the original lighting procedure")
endif()
string(SUBSTRING "${original}" ${lighting_start} -1 lighting_procedure)
if(lighting_procedure MATCHES "[\n\r][ \t]*cmp[ \t]")
    message(FATAL_ERROR "Reflection tail requires DoLighting to preserve comparison flags")
endif()

# An entry at vmain falls through into OrdinaryTail. A reflective entry CALLs
# only vmain's projection block, then evaluates the original color expression.
# Splitting at the position write adds no instructions or operand descriptors
# to the ordinary program. The SDK loads the shared DVLP, including both tails.
string(REGEX REPLACE "mov[ \t]+o_pos,[ \t]+r1" "mov o_pos, r1\n.end\n.proc OrdinaryTail" ordinary "${original}")
string(REGEX MATCH "mov[ \t]+o_tex0,[ \t]+in_tex0" uv_line "${original}")
string(FIND "${original}" "${uv_line}" uv_offset)
string(LENGTH "${uv_line}" uv_length)
math(EXPR colour_start "${uv_offset}+${uv_length}")
string(REGEX MATCH "mov[ \t]+o_colour,[ \t]+r0" colour_line "${original}")
string(FIND "${original}" "${colour_line}" colour_end)
math(EXPR colour_length "${colour_end}-${colour_start}")
string(SUBSTRING "${original}" ${colour_start} ${colour_length} colour)
string(REPLACE "Unlit" "ReflectionUnlit" colour "${colour}")
string(REPLACE "Done" "ReflectionDone" colour "${colour}")
string(FIND "${shader}" ".entry vmain" entry_offset)
string(SUBSTRING "${shader}" 0 ${entry_offset} header)
string(REPLACE "    .else\n" "${body_light}    .else\n" reflection "${reflection}")
string(REPLACE "    ifu u_reflective\n" "" reflection "${reflection}")
string(FIND "${reflection}" "    .else\n" ordinary_branch)
string(SUBSTRING "${reflection}" 0 ${ordinary_branch} reflection)
string(APPEND header ".alias worldSun b4\n")
set(world_uv [=[
    ; PackedCoordinatesBegin
    ifu worldSun
    ; World-space XZ to the PC-baked ground shadow texture. Static entities
    ; already store world positions; no CPU vertex work or additional draw.
    mov r7, u_lightPos[0]
    mul r6.xy, in_pos.xz, r7.xy
    add r6.xy, r6.xy, r7.zw
    mov r15, in_tex0
    mov r15.zw, r6.xyxy
    mov o2, r15
    .else
]=])
string(REPLACE "    ; PackedCoordinatesBegin\n" "${world_uv}" reflection "${reflection}")
string(APPEND reflection "    .end\n")
set(shader "${header}.entry ReflectEntry\n.proc ReflectEntry\n    call vmain\n${colour}\n${reflection}    ; PackedCoordinatesEnd\n    mov o_colour, r0\n    end\n.end\n")
if(NOT DEFINED ORDINARY_OUTPUT)
    set(ORDINARY_OUTPUT "${OUTPUT}.ordinary")
endif()
file(WRITE "${ORDINARY_OUTPUT}" "${ordinary}")
file(WRITE "${OUTPUT}" "${shader}")
