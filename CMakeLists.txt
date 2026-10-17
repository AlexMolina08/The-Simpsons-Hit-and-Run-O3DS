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
	option(SRR2_OLD3DS_WHEEL_ROTATION_REUSE "Reuse exact unchanged suspension rotation matrices" OFF)
	option(SRR2_OLD3DS_IO_WAIT_YIELD "Yield pending gameplay I/O without shortening loading-screen task slices" OFF)
	option(SRR2_OLD3DS_CLIP_PLANE_PREP "Prepare sparse projection side-plane weights once per projection" OFF)
	option(SRR2_OLD3DS_SPHERE_REGION_REUSE "Reuse sphere node selection inside proven plane-decision regions" OFF)
	option(SRR2_OLD3DS_SPHERE_WALK_CACHE "Reuse exact sphere tree-node selection while rechecking live entities" OFF)
	option(SRR2_OLD3DS_AREA_MEMBERSHIP "Index collision-area members within each submission callback" OFF)
	option(SRR2_OLD3DS_JOINT_COHERENCE_CHAIN "Retain verified world coherence across unchanged pose assignment and driver writes" OFF)
	option(SRR2_OLD3DS_CHANNEL_HINT "Reuse validated nearby animation key brackets without changing interpolation" OFF)
	option(SRR2_OLD3DS_POSE_BINDINGS "Reuse animation group bindings within each pose blend" OFF)
	option(SRR2_OLD3DS_SPEED_OPTIMIZATION "Test O3 loop and interprocedural optimization without fast math" OFF)
	option(SRR2_OLD3DS_IPA_POINTER_ANALYSIS "Test interprocedural pointer analysis with full IPO at O2" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET "Use freed texture memory for a bounded 320 KiB shared skin cache" OFF)
	option(SRR2_OLD3DS_JOINT_ROTATION_REUSE "Reuse unchanged joint rotation products while updating translation" OFF)
	option(SRR2_OLD3DS_JOINT_WORLD_REUSE "Reuse coherent joint worlds while preserving dirty propagation" OFF)
	option(SRR2_OLD3DS_QUATERNION_REUSE "Reuse exact rotation conversions with quaternion provenance" OFF)
	option(SRR2_OLD3DS_JOINT_ANGLE_FAST "Avoid acos inside the existing near-alignment dead zone" OFF)
	option(SRR2_OLD3DS_POSE_REUSE "Retain proven-valid joint worlds during same-skeleton pose assignment" OFF)
	option(SRR2_OLD3DS_LTO_DEVIRTUALIZE "Retain extra class information for link-time devirtualization" OFF)
	option(SRR2_OLD3DS_IPO "Optimize across translation units without relaxed floating-point rules" OFF)
	option(SRR2_OLD3DS_CHARACTER_SHADOW "Stream indexed character blob shadows" OFF)
	option(SRR2_OLD3DS_QUERY_SCRATCH "Use caller-owned physics query scratch storage" OFF)
	option(SRR2_OLD3DS_GROUND_REUSE "Reuse identical terrain queries within ground-plane preparation" OFF)
	option(SRR2_OLD3DS_DRAW_PREP "Prepare reusable native material and geometry data" OFF)
	option(SRR2_OLD3DS_COLL_PREP "Fuse collision volume transform preparation" OFF)
	option(SRR2_OLD3DS_LOAD_UPLOAD "Flush newly loaded geometry once before publication" OFF)
	option(SRR2_OLD3DS_TRI_HIERARCHY "Reject hierarchical terrain ranges while preserving candidate order" OFF)
	option(SRR2_OLD3DS_TRI_BLOCKS "Conservative eight-triangle terrain bounds" OFF)
	option(SRR2_OLD3DS_SKIN_RING "Stream CPU skin output through frame-owned linear buffers" OFF)
	option(SRR2_OLD3DS_HOT_PATHS "Apply O3 to measured hot functions while retaining global O2" OFF)
	option(SRR2_OLD3DS_LIGHT_CAPTURE "Skip disabled and exact repeated eye-light captures" OFF)
	option(SRR2_OLD3DS_DIRECTIONAL_LIGHT "Prepare directional light vectors once per light-state change" OFF)
	option(SRR2_OLD3DS_LIGHT_LOOP "Bound PICA lighting to enabled light slots" OFF)
	option(SRR2_OLD3DS_TEXTURE_TILES "Fuse texture conversion with tile-local writes" OFF)
	option(SRR2_OLD3DS_GPU_SKIN "Selective 23-bone PICA skinning; CPU fallback retained" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_EYE_CACHE "Reuse exact eye-space bones across skin groups" OFF)
	option(SRR2_OLD3DS_SKIN_ZERO "Skip provably neutral zero-weight CPU skin influences" OFF)
	option(SRR2_OLD3DS_SLERP_CACHE "Reuse exact quaternion interpolation angle invariants" OFF)
	option(SRR2_OLD3DS_JOINT_AXIS_ONCE "Reuse parent-axis values within each joint-angle update" OFF)
	option(SRR2_OLD3DS_BONE_USAGE "Prepare only bone matrices referenced by exact immutable skin palettes" OFF)
	option(SRR2_OLD3DS_QUAD_STREAM "Submit sprite and billboard quads with four vertices and exact triangle indices" OFF)
	option(SRR2_OLD3DS_JOINT_VECTOR_REUSE "Reuse exact joint-vector rotations while retaining transform and constraint updates" OFF)
	option(SRR2_OLD3DS_SLERP_ASSOCIATIVE "Use two ways within the existing Slerp entry budget" OFF)
	option(SRR2_OLD3DS_VRAM_TEXTURES "Move first-upload static textures to bounded spare VRAM" OFF)
	option(SRR2_OLD3DS_VEHICLE_PROFILE "Sample vehicle update phases without enlarging the telemetry ring" OFF)
	option(SRR2_OLD3DS_SKIN_CLIP "Conservatively cull skin rendering before palette or vertex writes" OFF)
	option(SRR2_OLD3DS_CLIP_BOUNDS "Conservatively reject static native geometry outside side clip planes" OFF)
	option(SRR2_OLD3DS_UNLIT_PROJECTION "Use sparse mono projection for unlit geometry" OFF)
	option(SRR2_OLD3DS_NO_NORMAL_FETCH "Omit constant normals from static native vertex fetches" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_BORROW "Borrow idle skin pool capacity within the original total cap" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_PACKED "Pack GPU skin attributes with bounded native-resolution error" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_COSTUMES "GPU skin audited costumes and remaining eligible cast" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_CAST "GPU skin the audited five-player cast and named NPCs" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE "Compose eye-space bones for a 25-bone GPU palette" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_RIGID_PACK "Store rigid GPU groups without unused float weights" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_BATCH "Retain GPU skin state across adjacent groups within one skin" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS "Use a separate bounded GPU skin pool for selected pedestrian meshes" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_COMPACT "Use a 24-bone mono GPU palette with packed lighting constants" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_PLAYERS "Include Bart alongside Homer in selective skin eligibility" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_METRICS "Buffer selective skinning metrics" OFF)
	option(SRR2_OLD3DS_GPU_SKIN_AB "Test only: alternate CPU/GPU every 120 rendered frames" OFF)
    if(SRR2_OLD3DS_SKIN_CLIP AND (NOT SRR2_OLD3DS_CLIP_BOUNDS OR NOT SRR2_OLD3DS_GPU_SKIN))
        message(FATAL_ERROR "SKIN_CLIP requires CLIP_BOUNDS and GPU_SKIN source metadata")
    endif()
    if(SRR2_OLD3DS_CLIP_BOUNDS AND NOT SRR2_OLD3DS_NO_NORMAL_FETCH)
        message(FATAL_ERROR "CLIP_BOUNDS requires static native load metadata from NO_NORMAL_FETCH")
    endif()
    if(SRR2_OLD3DS_UNLIT_PROJECTION AND NOT SRR2_OLD3DS_LIGHT_LOOP)
        message(FATAL_ERROR "UNLIT_PROJECTION requires LIGHT_LOOP")
    endif()
    if(SRR2_OLD3DS_NO_NORMAL_FETCH AND (NOT SRR2_OLD3DS_LOAD_UPLOAD OR SRR2_OLD3DS_MATERIAL_QUEUE))
        message(FATAL_ERROR "NO_NORMAL_FETCH requires LOAD_UPLOAD and direct submission")
    endif()
    if(SRR2_OLD3DS_GPU_SKIN_COSTUMES AND NOT SRR2_OLD3DS_GPU_SKIN_CAST)
        message(FATAL_ERROR "GPU_SKIN_COSTUMES requires GPU_SKIN_CAST")
    endif()
    if(SRR2_OLD3DS_TRI_HIERARCHY AND NOT SRR2_OLD3DS_TRI_BLOCKS)
        message(FATAL_ERROR "TRI_HIERARCHY requires TRI_BLOCKS")
    endif()
    if(SRR2_OLD3DS_GPU_SKIN_PACKED AND (NOT SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE OR NOT SRR2_OLD3DS_GPU_SKIN_RIGID_PACK))
        message(FATAL_ERROR "GPU_SKIN_PACKED requires GPU_SKIN_EYE_PALETTE and GPU_SKIN_RIGID_PACK")
    endif()
    if(SRR2_OLD3DS_GPU_SKIN_BORROW AND (NOT SRR2_OLD3DS_GPU_SKIN_PACKED OR NOT SRR2_OLD3DS_GPU_SKIN_CAST OR NOT SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS))
        message(FATAL_ERROR "GPU_SKIN_BORROW requires GPU_SKIN_PACKED, GPU_SKIN_CAST and GPU_SKIN_PEDESTRIANS")
    endif()
	if(SRR2_OLD3DS_GPU_SKIN_EYE_CACHE AND NOT SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE)
		message(FATAL_ERROR "GPU_SKIN_EYE_CACHE requires GPU_SKIN_EYE_PALETTE")
	endif()
	if(SRR2_OLD3DS_JOINT_COHERENCE_CHAIN AND (NOT SRR2_OLD3DS_JOINT_WORLD_REUSE OR NOT SRR2_OLD3DS_POSE_REUSE))
		message(FATAL_ERROR "JOINT_COHERENCE_CHAIN requires JOINT_WORLD_REUSE and POSE_REUSE")
	endif()
	if(SRR2_OLD3DS_BONE_USAGE AND (NOT SRR2_OLD3DS_GPU_SKIN OR NOT SRR2_OLD3DS_BONE_ONCE))
		message(FATAL_ERROR "BONE_USAGE requires GPU_SKIN support and BONE_ONCE")
	endif()
	if(SRR2_OLD3DS_QUAD_STREAM AND NOT SRR2_OLD3DS_SHADOW_MESH)
		message(FATAL_ERROR "QUAD_STREAM requires transient indexed-draw support from SHADOW_MESH")
	endif()
	if(SRR2_OLD3DS_SLERP_ASSOCIATIVE AND NOT SRR2_OLD3DS_SLERP_CACHE)
		message(FATAL_ERROR "SLERP_ASSOCIATIVE requires SLERP_CACHE")
	endif()
	if(SRR2_OLD3DS_JOINT_ROTATION_REUSE AND (NOT SRR2_OLD3DS_JOINT_WORLD_REUSE OR NOT SRR2_OLD3DS_POSE_REUSE))
		message(FATAL_ERROR "JOINT_ROTATION_REUSE requires JOINT_WORLD_REUSE and POSE_REUSE")
	endif()
	if(SRR2_OLD3DS_CLIP_PLANE_PREP AND NOT SRR2_OLD3DS_CLIP_BOUNDS)
		message(FATAL_ERROR "CLIP_PLANE_PREP requires CLIP_BOUNDS")
	endif()
	if(SRR2_OLD3DS_SPHERE_REGION_REUSE AND NOT SRR2_OLD3DS_SPHERE_WALK_CACHE)
		message(FATAL_ERROR "SPHERE_REGION_REUSE requires SPHERE_WALK_CACHE")
	endif()
	if(SRR2_OLD3DS_SPHERE_WALK_CACHE AND NOT SRR2_OLD3DS_SPATIAL_WALK_FAST)
		message(FATAL_ERROR "SPHERE_WALK_CACHE requires SPATIAL_WALK_FAST")
	endif()
	if(SRR2_OLD3DS_VRAM_TEXTURES AND NOT SRR2_OLD3DS_DRAW_PREP)
		message(FATAL_ERROR "VRAM_TEXTURES requires DRAW_PREP for fenced retirement")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET AND (NOT SRR2_OLD3DS_GPU_SKIN_BORROW OR NOT SRR2_OLD3DS_VRAM_TEXTURES))
		message(FATAL_ERROR "GPU_SKIN_EXTRA_BUDGET requires GPU_SKIN_BORROW and VRAM_TEXTURES")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_CAST AND (NOT SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE OR NOT SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS))
		message(FATAL_ERROR "GPU_SKIN_CAST requires GPU_SKIN_EYE_PALETTE and GPU_SKIN_PEDESTRIANS")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE AND NOT SRR2_OLD3DS_GPU_SKIN_COMPACT)
		message(FATAL_ERROR "GPU_SKIN_EYE_PALETTE requires GPU_SKIN_COMPACT")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_RIGID_PACK AND NOT SRR2_OLD3DS_GPU_SKIN)
		message(FATAL_ERROR "GPU_SKIN_RIGID_PACK requires GPU_SKIN")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_BATCH AND NOT SRR2_OLD3DS_GPU_SKIN)
		message(FATAL_ERROR "GPU_SKIN_BATCH requires GPU_SKIN")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS AND NOT SRR2_OLD3DS_GPU_SKIN_COMPACT)
		message(FATAL_ERROR "GPU_SKIN_PEDESTRIANS requires GPU_SKIN_COMPACT")
	endif()
	if(SRR2_OLD3DS_GPU_SKIN_COMPACT AND (NOT SRR2_OLD_3DS OR NOT SRR2_OLD3DS_GPU_SKIN OR NOT SRR2_OLD3DS_LIGHT_LOOP))
		message(FATAL_ERROR "GPU_SKIN_COMPACT requires OLD_3DS, GPU_SKIN and LIGHT_LOOP")
	endif()
	if(SRR2_OLD3DS_SKIN_RING)
		set(SRR2_OLD3DS_SKIN_DIRECT ON CACHE BOOL "Direct CPU skin output" FORCE)
	endif()
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
	option(SRR2_OLD3DS_SKIN_WEIGHTS "Avoid unit-weight scaling for rigid CPU-skinned vertices" OFF)
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
    if(SRR2_OLD3DS_DIRECTIONAL_LIGHT AND (NOT SRR2_OLD3DS_LIGHT_LOOP OR NOT SRR2_OLD3DS_UNLIT_PROJECTION OR NOT SRR2_OLD3DS_GPU_SKIN_PACKED))
        message(FATAL_ERROR "DIRECTIONAL_LIGHT requires LIGHT_LOOP, UNLIT_PROJECTION and GPU_SKIN_PACKED")
    endif()
    if(SRR2_OLD3DS_MATERIAL_QUEUE_AB AND (NOT SRR2_OLD3DS_MATERIAL_QUEUE OR NOT SRR2_OLD3DS_QUEUE_METRICS))
        message(FATAL_ERROR "MATERIAL_QUEUE_AB requires MATERIAL_QUEUE and QUEUE_METRICS")
    endif()
	if(SRR2_OLD_3DS)
		add_definitions(-DSRR2_OLD_3DS)
        if(SRR2_OLD3DS_IPO)
            include(CheckIPOSupported)
            check_ipo_supported(RESULT old3ds_ipo_supported OUTPUT old3ds_ipo_error LANGUAGES C CXX)
            if(NOT old3ds_ipo_supported)
                message(FATAL_ERROR "Old 3DS IPO unavailable: ${old3ds_ipo_error}")
            endif()
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
        foreach(feature JOINT_AXIS_ONCE BONE_USAGE QUAD_STREAM JOINT_VECTOR_REUSE SLERP_ASSOCIATIVE HOT_PATHS LIGHT_CAPTURE DIRECTIONAL_LIGHT IO_WAIT_YIELD CLIP_PLANE_PREP CHANNEL_HINT SPHERE_REGION_REUSE JOINT_COHERENCE_CHAIN SPHERE_WALK_CACHE AREA_MEMBERSHIP GPU_SKIN_EXTRA_BUDGET POSE_BINDINGS JOINT_ROTATION_REUSE VRAM_TEXTURES VEHICLE_PROFILE SKIN_CLIP CLIP_BOUNDS UNLIT_PROJECTION NO_NORMAL_FETCH GPU_SKIN_BORROW GPU_SKIN_PACKED TRI_HIERARCHY GPU_SKIN_COSTUMES SKIN_ZERO GPU_SKIN_EYE_CACHE SLERP_CACHE GPU_SKIN_CAST GPU_SKIN_EYE_PALETTE WHEEL_ROTATION_REUSE JOINT_WORLD_REUSE GPU_SKIN_RIGID_PACK QUATERNION_REUSE GPU_SKIN_BATCH GPU_SKIN_PEDESTRIANS JOINT_ANGLE_FAST GPU_SKIN_COMPACT POSE_REUSE CHARACTER_SHADOW QUERY_SCRATCH GROUND_REUSE DRAW_PREP COLL_PREP INDEX_CACHE SHADOW_MESH LOAD_UPLOAD TRI_BLOCKS SKIN_RING LIGHT_LOOP TEXTURE_TILES GPU_SKIN GPU_SKIN_PLAYERS GPU_SKIN_METRICS GPU_SKIN_AB MATERIAL_QUEUE MATERIAL_QUEUE_AB QUEUE_METRICS BONE_ONCE JOINT_TRANSLATE DEFAULT_CACHE SKIN_DIRECT SKIN_WEIGHTS STREAM_FAST FLAT_TRI_FAST SPATIAL_WALK_FAST)
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
            set(SRR2_OLD3DS_BUILD_ID "display-index-1")
        elseif(SRR2_OLD3DS_SHADOW_MESH)
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

# Select profile export linkage before subdirectories inherit linker flags.
set(SRR2_OLD3DS_PGO "OFF" CACHE STRING "Old 3DS GCC profile mode: OFF, GENERATE or USE")
set_property(CACHE SRR2_OLD3DS_PGO PROPERTY STRINGS OFF GENERATE USE)
if(SRR2_OLD_3DS AND SRR2_OLD3DS_IPA_POINTER_ANALYSIS)
    if(NOT SRR2_OLD3DS_IPO OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT SRR2_OLD3DS_PGO STREQUAL "OFF")
        message(FATAL_ERROR "IPA_POINTER_ANALYSIS requires GNU full IPO with PGO OFF")
    endif()
    # Improve alias information across entity, animation and physics boundaries.
    # Keep O2 and its existing floating-point rules and inlining limits.
    add_compile_options(-fipa-pta)
    add_link_options(-fipa-pta)
endif()
if(SRR2_OLD_3DS AND SRR2_OLD3DS_HOT_PATHS)
    if(NOT SRR2_OLD3DS_IPO OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT SRR2_OLD3DS_PGO STREQUAL "OFF" OR SRR2_OLD3DS_SPEED_OPTIMIZATION)
        message(FATAL_ERROR "HOT_PATHS requires GNU full IPO, global O2 and PGO OFF")
    endif()
endif()
if(SRR2_OLD_3DS AND SRR2_OLD3DS_SPEED_OPTIMIZATION)
    if(NOT SRR2_OLD3DS_IPO OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT SRR2_OLD3DS_PGO STREQUAL "OFF")
        message(FATAL_ERROR "SPEED_OPTIMIZATION requires GNU full IPO with PGO OFF")
    endif()
    # Apply at both compilation and LTO so hot loops can be specialized across
    # libraries. Do not enable fast math, reassociation or altered rounding rules.
    # Preserve I-cache and application-region headroom rather than growing every
    # constant-trip loop. Other O3 loop and IPA transformations remain enabled.
    # O3's larger inlining thresholds grew the program by 565 KiB. Keep O2's
    # auto/single-call limits while testing the stronger loop transformations.
    add_compile_options(-O3 -fno-unroll-completely-grow-size
        --param=max-inline-insns-auto=15 --param=max-inline-insns-single=70)
    add_link_options(-O3 -fno-unroll-completely-grow-size
        --param=max-inline-insns-auto=15 --param=max-inline-insns-single=70)
endif()
if(SRR2_OLD_3DS AND SRR2_OLD3DS_LTO_DEVIRTUALIZE)
    if(NOT SRR2_OLD3DS_IPO OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT SRR2_OLD3DS_PGO STREQUAL "OFF")
        message(FATAL_ERROR "LTO_DEVIRTUALIZE requires GNU full IPO with PGO OFF")
    endif()
    # GCC needs this type information in the LTO objects as well as at link.
    # Keep original optimization level and floating-point semantics.
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fdevirtualize-at-ltrans>")
    add_link_options(-fdevirtualize-at-ltrans)
endif()

set(SRR2_OLD3DS_PGO_EXCLUDE_SOURCES "" CACHE STRING "Changed source files compiled without stale profiles")
if(SRR2_OLD_3DS AND SRR2_OLD3DS_PGO STREQUAL "GENERATE")
        get_filename_component(old3ds_gcc_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(READ "${old3ds_gcc_bin}/../arm-none-eabi/lib/3dsx.ld" old3ds_pgo_linker)
        string(REPLACE "*(.rodata)"
            "__gcov_info_start = .; KEEP(*(.gcov_info)) __gcov_info_end = .; *(.rodata)"
            old3ds_pgo_linker "${old3ds_pgo_linker}")
        file(WRITE "${CMAKE_BINARY_DIR}/old3ds-pgo.ld" "${old3ds_pgo_linker}")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tools/3ds/old3ds.specs" old3ds_pgo_specs)
        string(REPLACE "3dsx.ld%s" "${CMAKE_BINARY_DIR}/old3ds-pgo.ld"
            old3ds_pgo_specs "${old3ds_pgo_specs}")
        file(WRITE "${CMAKE_BINARY_DIR}/old3ds-pgo.specs" "${old3ds_pgo_specs}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/tools/3ds/old3ds.specs"
            "${CMAKE_BINARY_DIR}/old3ds-pgo.specs"
            CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()

add_subdirectory(libs)
add_subdirectory(code)

# Profile training is a separate emulator-only build. Normal builds keep no
# counters, dumping code or profiling runtime. Use the same build directory for
# GENERATE and USE so GCC's object/profile paths remain identical.
if(SRR2_OLD_3DS AND NOT SRR2_OLD3DS_PGO STREQUAL "OFF")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Old 3DS PGO requires the devkitARM GCC toolchain")
    endif()
    set(old3ds_pgo_targets SRR2 sim poser radmath p3d pddi choreo)
    if(SRR2_OLD3DS_PGO STREQUAL "GENERATE")
        foreach(profile_target IN LISTS old3ds_pgo_targets)
            target_compile_options(${profile_target} PRIVATE -fprofile-generate -fprofile-info-section)
        endforeach()
        target_link_options(SRR2 PRIVATE -fprofile-generate)
        target_compile_definitions(SRR2 PRIVATE SRR2_OLD3DS_PGO_GENERATE)
        target_sources(SRR2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/code/main/old3dspgo.cpp")
        # Keep the shutdown/export functions outside the recorded workload.
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/code/main/win32main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/code/main/old3dspgo.cpp"
            TARGET_DIRECTORY SRR2 PROPERTIES COMPILE_OPTIONS -fno-profile-generate)
    elseif(SRR2_OLD3DS_PGO STREQUAL "USE")
        foreach(profile_target IN LISTS old3ds_pgo_targets)
            # Profile-guided native objects avoid GCC 16.1's crashing WPA pass.
            set_property(TARGET ${profile_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
            target_compile_options(${profile_target} PRIVATE -fprofile-use -fprofile-partial-training -Werror=coverage-mismatch)
        endforeach()
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/code/main/win32main.cpp"
            TARGET_DIRECTORY SRR2 PROPERTIES COMPILE_OPTIONS -fno-profile-use)
        # GCC 16.1 crashes in whole-program profile processing for this graph.
        # Consume profiles in native objects; unprofiled libraries keep normal IPO.
        target_link_options(SRR2 PRIVATE -fno-profile-use)
        foreach(profile_source IN LISTS SRR2_OLD3DS_PGO_EXCLUDE_SOURCES)
            set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${profile_source}"
                TARGET_DIRECTORY ${old3ds_pgo_targets} PROPERTIES COMPILE_OPTIONS -fno-profile-use)
        endforeach()
    else()
        message(FATAL_ERROR "Unknown Old 3DS PGO mode: ${SRR2_OLD3DS_PGO}")
    endif()
endif()


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
