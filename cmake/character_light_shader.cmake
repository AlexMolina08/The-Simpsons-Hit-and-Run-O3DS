# Give the existing unlit skin path a single GPU key/fill calculation. The three
# existing unlit constant uploads carry its parameters; bone/stereo layout stays.
file(READ "${INPUT}" shader)
if(NOT shader MATCHES "dp3 r5.x, u_bones" OR NOT shader MATCHES "u_eye_sceneAmbient")
    message(FATAL_ERROR "Prepared character lighting requires the eye-space skin palette")
endif()
set(lighting [=[
    mul r0, fixclr, in_colour
    ; PreparedCharacterLightBegin
    ; Skinning already produced r5. Do not run CPU scene-light selection.
    dp3 r2.x, r5.xyz, r5.xyz
    max r2.x, fixclr, r2.x
    rsq r2.x, r2.x
    mul r9.xyz, r5.xyz, r2.xxx
    dp3 r6.x, u_eye_matAmbient.xyz, r9.xyz
    max r6.x, zeros, r6.x
    mov r7, u_eye_matEmissive
    mul r6.rgb, r7.rgb, r6.xxx
    mov r7, u_eye_sceneAmbient
    add r6.rgb, r7.rgb, r6.rgb
    mul r0.rgb, r0.rgb, r6.rgb
    ; PreparedCharacterLightEnd
]=])
string(REGEX REPLACE "mul[ \t]+r0,[ \t]+fixclr,[ \t]+in_colour" "${lighting}" shader "${shader}")
file(WRITE "${OUTPUT}" "${shader}")
