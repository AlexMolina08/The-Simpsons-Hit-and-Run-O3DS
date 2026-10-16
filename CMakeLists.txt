cmake_minimum_required(VERSION 3.13...3.28)

project(SRR2)

set(SRR2_TITLE "Simpsons Hit & Run")
set(SRR2_VERSION "00.61")
set(SRR2_FILENAME "Simpsons")
set(SRR2_REGION "NTSC" CACHE STRING "Select PAL for the EFIGS multi-language version")
set_property(CACHE SRR2_REGION PROPERTY STRINGS "NTSC" "PAL")

option(SRR2_USE_DLMALLOC "Use Doug Lea's memory allocator for fixed-size heaps" OFF)
option(SRR2_REROUTE_ALL_HEAP_ALLOCATIONS "Reroute all memory allocations by new operator to internal heaps" OFF)
option(SRR2_USE_BINK "Use RAD Game Tools Bink SDK instead of FFmpeg" OFF)
option(SRR2_BUILD_TESTS "Build sample programs for internal libraries" ON)
option(SRR2_MICROPROFILE "Use microprofile instead of the internal profiler" OFF)
option(SRR2_P3D_CG "Use CG shaders rather than GLSL for OpenGL ES" OFF)
set(SRR2_P3D_PDDI "OpenGL" CACHE STRING "Pure3D PDDI implementation")
if(VITA)
	option(SRR2_USE_VITAGL "Use VitaGL as an OpenGL (ES) implementation" ON)
	set_property(CACHE SRR2_P3D_PDDI PROPERTY STRINGS "OpenGL" "GLES1" "GLES2" "GXM")
elseif(NINTENDO_3DS)
	set(SRR2_P3D_PDDI "PICA" CACHE STRING "Pure3D PDDI implementation" FORCE)
	option(SRR2_OLD_3DS "Target original 3DS hardware (no New 3DS features)" OFF)
	option(SRR2_3DS_MOVIES "Play converted movies (software MPEG-1 on Old 3DS, hardware H.264 on New 3DS)" ON)
	option(SRR2_OLD3DS_GPU_SKIN "Selective 23-bone PICA skinning; CPU fallback retained" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_PLAYERS "Include Bart alongside Homer in selective skin eligibility" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_METRICS "Buffer selective skinning metrics" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_AB "Test only: alternate CPU/GPU every 120 rendered frames" OFF)
	if(SRR2_OLD3DS_GPU_SKIN_AB AND (NOT SRR2_OLD3DS_GPU_SKIN OR NOT SRR2_OLD3DS_GPU_SKIN_METRICS))
		message(FATAL_ERROR "GPU_SKIN_AB requires GPU_SKIN and GPU_SKIN_METRICS")
	endif()
	if(SRR2_OLD_3DS AND (SRR2_OLD3DS_GPU_SKIN OR SRR2_OLD3DS_GPU_SKIN_METRICS))
		add_definitions(-DSRR2_OLD3DS_GPU_SKIN_SUPPORT)
	endif()
	option(SRR2_OLD3DS_MATERIAL_QUEUE "Conservative native static submesh material queue" OFF)
	option(SRR2_OLD3DS_MATERIAL_QUEUE_AB "Test only: alternate legacy/native queue every telemetry interval" OFF)
	option(SRR2_OLD3DS_MATERIAL_QUEUE_SORT "Allow conservative screen-disjoint material reordering" OFF)
	option(SRR2_OLD3DS_QUEUE_METRICS "Buffer renderer queue experiment telemetry until shutdown" OFF)
	option(SRR2_OLD3DS_BONE_ONCE "Avoid duplicate skin bone matrix multiplication" OFF)
	option(SRR2_OLD3DS_JOINT_TRANSLATE "Translate character joints without full affine multiplication" OFF)
	option(SRR2_OLD3DS_DEFAULT_CACHE "Cache absent attributes on persistent vertex buffers" OFF)
	option(SRR2_OLD3DS_SKIN_DIRECT "Write skinned vertices directly to the PICA stream" OFF)
	option(SRR2_OLD3DS_SKIN_WEIGHTS "Skip zero-weight bone transforms and unit-weight scaling during skinning" OFF)
	option(SRR2_OLD3DS_STREAM_FAST "Fast-path immediate vertex streaming and default attribute setup" OFF)
	option(SRR2_OLD3DS_FLAT_TRI_FAST "Reset per-triangle XZ mask and stop quadratic rescan on miss in mFlatTriFast" OFF)
	option(SRR2_OLD3DS_SPATIAL_WALK_FAST "Avoid dead BoxPts copies in MarkAll and early-out physics/character distance checks" OFF)
	set(SRR2_OLD3DS_EXPERIMENT_ID "" CACHE STRING "Explicit diagnostic experiment identifier")
	option(SRR2_OLD3DS_INDEX_CACHE "Preserve index validation across vertex-only updates" OFF)
	option(SRR2_OLD3DS_SHADOW_MESH "Use indexed vehicle blob shadows" OFF)
	# Experimental: skip the repeat collision-volume updates the pair loop would
	# otherwise perform within one collision-detection pass. Off by default so
	# the shipped build keeps the original behaviour.
	option(SRR2_OLD3DS_COLLVOL_CACHE "Skip repeated collision-volume updates within a detection pass" OFF)
	# Experimental: perform the repeated updates unchanged but hash the volume's
	# geometry before and after each one, so a mismatch can falsify the cache.
	option(SRR2_OLD3DS_COLLVOL_VERIFY "Hash collision-volume geometry around repeated updates" OFF)
    if(SRR2_OLD3DS_MATERIAL_QUEUE_AB AND (NOT SRR2_OLD3DS_MATERIAL_QUEUE OR NOT SRR2_OLD3DS_QUEUE_METRICS))
        message(FATAL_ERROR "MATERIAL_QUEUE_AB requires MATERIAL_QUEUE and QUEUE_METRICS")
    endif()
	if(SRR2_OLD_3DS)
		add_definitions(-DSRR2_OLD_3DS)
        foreach(feature GPU_SKIN GPU_SKIN_PLAYERS GPU_SKIN_METRICS GPU_SKIN_AB MATERIAL_QUEUE MATERIAL_QUEUE_AB QUEUE_METRICS BONE_ONCE JOINT_TRANSLATE DEFAULT_CACHE SKIN_DIRECT SKIN_WEIGHTS STREAM_FAST FLAT_TRI_FAST SPATIAL_WALK_FAST)
            if(SRR2_OLD3DS_${feature})
                add_definitions(-DSRR2_OLD3DS_${feature})
            endif()
        endforeach()
        if(SRR2_OLD3DS_MATERIAL_QUEUE AND SRR2_OLD3DS_MATERIAL_QUEUE_SORT)
            add_definitions(-DSRR2_OLD3DS_MATERIAL_QUEUE_SORT)
        endif()
		if(SRR2_OLD3DS_COLLVOL_CACHE)
			add_definitions(-DSRR2_OLD3DS_COLLVOL_CACHE)
		endif()
		if(SRR2_OLD3DS_COLLVOL_VERIFY)
			add_definitions(-DSRR2_OLD3DS_COLLVOL_VERIFY)
		endif()
		# Stamped into the boot log so a captured log can be matched to the
		# executable that produced it. The collision-volume experiment has three
		# modes, so the id names the mode rather than being bumped by hand.
		if(SRR2_OLD3DS_INDEX_CACHE)
            add_definitions(-DSRR2_OLD3DS_INDEX_CACHE)
            set(SRR2_OLD3DS_BUILD_ID "display-index-1")
        elseif(SRR2_OLD3DS_SHADOW_MESH)
			add_definitions(-DSRR2_OLD3DS_SHADOW_MESH)
			set(SRR2_OLD3DS_BUILD_ID "render-shadow-1")
		elseif(NOT SRR2_OLD3DS_COLLVOL_VERIFY AND NOT SRR2_OLD3DS_COLLVOL_CACHE)
			set(SRR2_OLD3DS_BUILD_ID "display-diag-1")
		elseif(SRR2_OLD3DS_COLLVOL_VERIFY)
			set(SRR2_OLD3DS_BUILD_ID "coll-verify-2")
		elseif(SRR2_OLD3DS_COLLVOL_CACHE)
			set(SRR2_OLD3DS_BUILD_ID "coll-cache-1")
		else()
			set(SRR2_OLD3DS_BUILD_ID "coll-1")
		endif()
        if(SRR2_OLD3DS_EXPERIMENT_ID)
            set(SRR2_OLD3DS_BUILD_ID "${SRR2_OLD3DS_EXPERIMENT_ID}")
        endif()
		add_definitions(-DSRR2_OLD3DS_BUILD_ID="${SRR2_OLD3DS_BUILD_ID}")
		# Retain the normal 3DSX startup/linker script, using ARM11 barriers.
		string(REPLACE "-specs=3dsx.specs"
			"-specs=\"${CMAKE_CURRENT_SOURCE_DIR}/tools/3ds/old3ds.specs\""
			CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
	endif()
else()
	set_property(CACHE SRR2_P3D_PDDI PROPERTY STRINGS "OpenGL" "GLES1" "GLES2")
endif()

if(VITA)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	add_definitions(-DRAD_VITA)
	## Configuration options for this app
	# Display name (under bubble in LiveArea)
	set(VITA_APP_NAME ${SRR2_TITLE})
	# Unique ID must be exactly 9 characters. Recommended: XXXXYYYYY where X =
	# unique string of developer and Y = a unique number for this app
	set(VITA_TITLEID  "ROADRAGE2")
	# Optional version string to show in LiveArea's more info screen
	set(VITA_VERSION ${SRR2_VERSION})
endif()

if(NINTENDO_3DS)
	add_definitions(-DRAD_3DS)
	set(SRR2_BUILD_TESTS OFF CACHE BOOL "Build sample programs for internal libraries" FORCE)
	if(NOT SRR2_3DS_MOVIES)
		add_definitions(-DSRR2_NO_MOVIES)
	endif()
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()

if(NOT VITA AND NOT NINTENDO_3DS)
	find_package(SDL3)
endif()
if(SDL3_FOUND)
	get_target_property(SDL_INCLUDE_DIR SDL3::Headers INTERFACE_INCLUDE_DIRECTORIES)
	include_directories(${SDL_INCLUDE_DIR}/SDL3)
	set(SDL_LIBRARY SDL3::SDL3)
else()
	find_package(SDL2 REQUIRED)
	set(SDL_LIBRARY SDL2::SDL2 SDL2::SDL2main)
endif()
find_package(PNG REQUIRED)
if(NINTENDO_3DS)
	add_library(srr2_threads_stub INTERFACE)
	add_library(Threads::Threads ALIAS srr2_threads_stub)
else()
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	find_package(OpenAL REQUIRED)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-fvisibility=hidden)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(
			-Werror
			-ggdb
		)
	endif()
	if(VITA)
		add_compile_options(
			-mtune=cortex-a9
			-mfpu=neon
		)
	endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DRAD_DEBUG)
else()
	add_definitions(-DRAD_RELEASE)
endif()

add_definitions(
	-DRAD_CONSOLE
	-DRAD_WIN32
	-DRAD_SDL
)

if(SRR2_REGION STREQUAL "PAL")
	add_definitions(-DPAL)
endif()
if(SRR2_USE_DLMALLOC)
	add_definitions(-DUSE_DOUG_LEA_HEAP)
endif()
if(SRR2_REROUTE_ALL_HEAP_ALLOCATIONS)
	add_definitions(-DREROUTE_ALL_HEAP_ALLOCATIONS)
endif()
if(SRR2_USE_BINK)
	add_definitions(-DRAD_MOVIEPLAYER_USE_BINK)
elseif(NINTENDO_3DS)
	if(SRR2_3DS_MOVIES)
		add_definitions(-DRAD_MOVIEPLAYER_CTR)
		if(SRR2_OLD_3DS)
			add_definitions(-DRAD_MOVIEPLAYER_CTR_SOFTWARE)
		endif()
	else()
		add_definitions(-DRAD_MOVIEPLAYER_NULL)
	endif()
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(FFmpeg REQUIRED IMPORTED_TARGET
			libavformat
			libavcodec
			libavutil
			libswresample
			libswscale
	)
endif()
if(SRR2_MICROPROFILE)
	add_definitions(-DMICROPROFILE_ENABLED)
endif()
if(SRR2_P3D_CG)
	add_definitions(-DRAD_CG)
endif()
if(SRR2_USE_VITAGL)
    add_definitions(-DRAD_VITAGL)
endif()
if(SRR2_P3D_PDDI STREQUAL "GXM")
    add_definitions(-DRAD_GXM)
endif()
if(SRR2_P3D_PDDI STREQUAL "PICA")
	add_definitions(-DRAD_PICA)
endif()
if(SRR2_P3D_PDDI STREQUAL "GLES1")
	add_definitions(-DRAD_GLES)
	add_definitions(-DRAD_GLES_VERSION=1)
endif()
if(SRR2_P3D_PDDI STREQUAL "GLES2")
	add_definitions(-DRAD_GLES)
	add_definitions(-DRAD_GLES_VERSION=2)
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(libs)
add_subdirectory(code)

if(VITA)
	## Create Vita files
	if(SRR2_P3D_PDDI STREQUAL "GLES2" AND NOT SRR2_USE_VITAGL)
		vita_create_self(${PROJECT_NAME}.self code/${PROJECT_NAME} UNSAFE)
		# The FILE directive lets you add additional files to the VPK, the syntax is
		# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
		vita_create_vpk(${SRR2_FILENAME}${SRR2_REGION}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
			VERSION ${VITA_VERSION}
			NAME ${VITA_APP_NAME}
			FILE sce_sys/icon0.png sce_sys/icon0.png
			sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
			sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
			sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
			art art

			#suprx
			module/libGLESv2.suprx module/libGLESv2.suprx
			module/libgpu_es4_ext.suprx module/libgpu_es4_ext.suprx
			module/libIMGEGL.suprx module/libIMGEGL.suprx
			module/libpvrPSP2_WSEGL.suprx module/libpvrPSP2_WSEGL.suprx
		)
	else()
		vita_create_self(${PROJECT_NAME}.self code/${PROJECT_NAME})
		# The FILE directive lets you add additional files to the VPK, the syntax is
		# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
		vita_create_vpk(${SRR2_FILENAME}${SRR2_REGION}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
			VERSION ${VITA_VERSION}
			NAME ${VITA_APP_NAME}
			FILE sce_sys/icon0.png sce_sys/icon0.png
			sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
			sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
			sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
			art art

			#gxp
			libs/pure3d/pddi/gxm/shaders/vertex_cg.gxp shaders/vertex_cg.gxp
			libs/pure3d/pddi/gxm/shaders/color_cg.gxp shaders/color_cg.gxp
			libs/pure3d/pddi/gxm/shaders/texture_cg.gxp shaders/texture_cg.gxp
			libs/pure3d/pddi/gxm/shaders/alpha_cg.gxp shaders/alpha_cg.gxp
		)
	endif()
endif()

if(NINTENDO_3DS)
	target_link_options(${PROJECT_NAME} PRIVATE
		-Wl,--wrap=linearAlloc -Wl,--wrap=linearMemAlign -Wl,--wrap=linearRealloc
		-Wl,--wrap=linearGetSize -Wl,--wrap=linearFree -Wl,--wrap=linearSpaceFree)

	ctr_generate_smdh(${PROJECT_NAME}.smdh
		NAME        ${SRR2_TITLE}
		DESCRIPTION "Radical Entertainment - v${SRR2_VERSION}"
		AUTHOR      "Radical Entertainment"
	)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/romfs")
		ctr_create_3dsx(${PROJECT_NAME}
			OUTPUT ${SRR2_FILENAME}${SRR2_REGION}.3dsx
			SMDH   ${PROJECT_NAME}.smdh
			ROMFS  "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
		)
	else()
		ctr_create_3dsx(${PROJECT_NAME}
			OUTPUT ${SRR2_FILENAME}${SRR2_REGION}.3dsx
			SMDH   ${PROJECT_NAME}.smdh
		)
	endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
	nx_generate_nacp(OUTPUT ${PROJECT_NAME}.nacp
		NAME ${SRR2_TITLE}
		AUTHOR "Radical Entertainment"
		VERSION ${SRR2_VERSION}
	)
	nx_create_nro(code/${PROJECT_NAME}
		OUTPUT ${SRR2_FILENAME}${SRR2_REGION}.nro
		ICON ${PROJECT_NAME}.jpg
		NACP ${PROJECT_NAME}.nacp
		ROMFS romfs
	)
endif()
