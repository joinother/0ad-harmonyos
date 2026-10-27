cmake_minimum_required(VERSION 3.10)
project(zeroad CXX C)

# ROOT points to a directory containing the 0 A.D. source tree and
# cross-compiled dependencies (lycium). Set ZEROAD_ROOT env var to override.
# Otherwise walk up from this file until we find a directory that contains both
# marker paths (robust against exact nesting depth / hvigor loader copies).
set(ROOT "")
if(DEFINED ENV{ZEROAD_ROOT})
    set(ROOT "$ENV{ZEROAD_ROOT}")
else()
    set(_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 1 10)
        get_filename_component(_dir "${_dir}" DIRECTORY)
        if(EXISTS "${_dir}/lycium/lycium/usr" AND EXISTS "${_dir}/0ad-0.28.0/source")
            set(ROOT "${_dir}")
            break()
        endif()
    endforeach()
endif()
if(NOT ROOT)
    message(FATAL_ERROR "Could not locate the 0 A.D. ROOT (expected sibling directories '0ad-0.28.0' and 'lycium'). Set the ZEROAD_ROOT environment variable.")
endif()
message(STATUS "0 A.D. ROOT = ${ROOT}")
set(AD "${ROOT}/0ad-0.28.0")
set(SRC "${AD}/source")
set(USR "${ROOT}/lycium/lycium/usr")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------- SDL3 (shared, HarmonyOS backend) ----------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory("SDL" EXCLUDE_FROM_ALL)

# ---------------- collect engine sources ----------------
file(GLOB_RECURSE ENGINE_SRCS CONFIGURE_DEPENDS "${SRC}/*.cpp" "${SRC}/*.c")
# tests
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/tests/|/test/|test_setup\\.cpp|CxxTestRunner")
# other platforms
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/os/win/|/os/osx/|/os/bsd/|/os/android/")
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/arch/(x86_x64|ia32|amd64|arm|ppc64|e2k)/")
# Atlas editor: keep GameInterface (compiled into the engine, provides
# BeginAtlas); exclude the wxWidgets UI/object/frontends
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/tools/atlas/AtlasUI/")
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/tools/atlas/AtlasObject/")
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/tools/atlas/AtlasFrontends/")
# RL interface is always built; only exclude the debug-adapter interface
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/dapinterface/")
# mongoose embedded HTTP server: compile the C++ wrapper (used by the RL
# interface and Profiler2), exclude the unmodified C original (duplicate code)
list(FILTER ENGINE_SRCS EXCLUDE REGEX "mongoose_orig\\.c")
# collada converter built as a separate shared library (libCollada.so)
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/collada/")
# Windows MSVC runtime (we use the gcc/posix runtime path)
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/rtl/msc/")
# Online lobby (XMPP) + Jingle ICE-UDP NAT traversal. gloox and miniupnpc
# are cross-compiled in manual-deps and linked below, so build the full
# lobby client and the UPnP port-mapping code.
# Documentation example component (not a real component, IDs are not generated)
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/simulation2/docs/")
# Platform-specific GLAD loaders we don't use on HarmonyOS
list(FILTER ENGINE_SRCS EXCLUDE REGEX "glad/src/glx\\.cpp")
list(FILTER ENGINE_SRCS EXCLUDE REGEX "glad/src/wgl\\.c")
# GL and GLES2 loaders emit identical glad_gl* symbols; we use the GL loader
list(FILTER ENGINE_SRCS EXCLUDE REGEX "glad/src/gles2\\.cpp")
# Windows-only tinygettext dirent shim
list(FILTER ENGINE_SRCS EXCLUDE REGEX "tinygettext/src/win32/")
# mocks test (keep mocks_real)
list(FILTER ENGINE_SRCS EXCLUDE REGEX "mocks_test\\.cpp")

add_library(pyrogenesis STATIC ${ENGINE_SRCS})
# RTTI shim that emits the missing typeinfo for js::BaseProxyHandler
target_sources(pyrogenesis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ohos_rtti_shim.cpp)

file(GLOB PCH_DIRS LIST_DIRECTORIES true "${SRC}/pch/*")
list(FILTER PCH_DIRS EXCLUDE REGEX "/(atlas|test)$")

set(ENGINE_INCLUDES
	${SRC}
	${PCH_DIRS}
	${AD}/libraries/source/cxxtest-4.4
	${SRC}/third_party/glad/include
	${SRC}/third_party/tinygettext/include
	${CMAKE_CURRENT_SOURCE_DIR}/SDL/include
	${CMAKE_CURRENT_SOURCE_DIR}/SDL/include/SDL3
	${USR}/mozjs/arm64-v8a/include
	${USR}/boost/arm64-v8a/include
	${USR}/fmt/arm64-v8a/include
	${USR}/icu/arm64-v8a/include
	${USR}/libxml2/arm64-v8a/include/libxml2
	${USR}/freetype2/arm64-v8a/include/freetype2
	${USR}/libpng/arm64-v8a/include
	${USR}/zlib/arm64-v8a/include
	${USR}/libiconv/arm64-v8a/include
	${USR}/libsodium/arm64-v8a/include
	${USR}/libcurl/arm64-v8a/include
	${USR}/openal/arm64-v8a/include
	${USR}/enet/arm64-v8a/include
	${USR}/fcollada/arm64-v8a/include
	${USR}/nvtt/arm64-v8a/include
	${USR}/libvorbis/arm64-v8a/include
	${USR}/libogg/arm64-v8a/include
	${USR}/gloox/arm64-v8a/include
	${USR}/miniupnpc/arm64-v8a/include
)
target_include_directories(pyrogenesis PRIVATE ${ENGINE_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(pyrogenesis PRIVATE
	NDEBUG
	LINUX
	CONFIG_FINAL=1
	CONFIG_ENABLE_PCH=0
	CONFIG2_LOBBY=1
	CONFIG2_MINIUPNPC=1
	CONFIG2_DAP_INTERFACE=0
	_GNU_SOURCE
)
target_compile_options(pyrogenesis PRIVATE
	-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
	-Wno-reorder -Wno-undef -Wno-redundant-decls -Wno-non-virtual-dtor
	-fvisibility=hidden
)

# ---------------- libCollada.so (DAE -> PMD/PSA converter) ----------------
file(GLOB COLLADA_SRCS "${SRC}/collada/*.cpp")
if(COLLADA_SRCS)
	add_library(Collada SHARED ${COLLADA_SRCS})
	target_include_directories(Collada PRIVATE ${SRC}/collada ${ENGINE_INCLUDES})
	target_compile_definitions(Collada PRIVATE
		NDEBUG LINUX CONFIG_FINAL=1 CONFIG_ENABLE_PCH=0 _GNU_SOURCE
	)
	target_compile_options(Collada PRIVATE
		-fvisibility=hidden -fno-strict-aliasing
		-Wno-all -Wno-extra -Wno-deprecated-copy-with-user-provided-copy
	)
	target_link_libraries(Collada PRIVATE
		${USR}/fcollada/arm64-v8a/lib/libFColladaSR.a
		${USR}/libxml2/arm64-v8a/lib/libxml2.so
		${USR}/libiconv/arm64-v8a/lib/libiconv.so
		${USR}/zlib/arm64-v8a/lib/libz.a
		dl m
	)
	set_target_properties(Collada PROPERTIES OUTPUT_NAME Collada PREFIX lib)
else()
	message(STATUS "Collada: no sources matched; skipping libCollada.so (not needed at runtime; art ships pre-converted and Atlas/DAE authoring is excluded).")
endif()

# ---------------- libmain.so ----------------
add_library(main SHARED ohos_wrapper.c ohos_bootstrap.cpp ohos_cli.cpp cacert_data.c ohos_input_monitor.cpp)
target_include_directories(main PRIVATE ${ENGINE_INCLUDES})

file(GLOB BOOST_LIBS "${USR}/boost/arm64-v8a/lib/*.a")
set(STATIC_LIBS
	${USR}/fcollada/arm64-v8a/lib/libFColladaSR.a
	${USR}/mozjs/arm64-v8a/lib/libjs_static.a
	${USR}/mozjs/arm64-v8a/lib/libjsrust.a
	${USR}/nvtt/arm64-v8a/lib/libnvtt.a
	${USR}/nvtt/arm64-v8a/lib/libnvimage.a
	${USR}/nvtt/arm64-v8a/lib/libnvcore.a
	${USR}/nvtt/arm64-v8a/lib/libnvmath.a
	${USR}/nvtt/arm64-v8a/lib/libnvthread.a
	${USR}/nvtt/arm64-v8a/lib/libsquish.a
	${USR}/nvtt/arm64-v8a/lib/libbc6h.a
	${USR}/nvtt/arm64-v8a/lib/libbc7.a
	${USR}/nvtt/arm64-v8a/lib/libposh.a
	${USR}/freetype2/arm64-v8a/lib/libfreetype.a
	${USR}/libpng/arm64-v8a/lib/libpng.a
	${USR}/zlib/arm64-v8a/lib/libz.a
	${USR}/libsodium/arm64-v8a/lib/libsodium.a
	${USR}/libcurl/arm64-v8a/lib/libcurl.a
	${USR}/openssl/arm64-v8a/lib/libssl.a
	${USR}/openssl/arm64-v8a/lib/libcrypto.a
	${USR}/openal/arm64-v8a/lib/libopenal.a
	${USR}/enet/arm64-v8a/lib/static/libenet.a
	${USR}/gloox/arm64-v8a/lib/libgloox.a
	${USR}/miniupnpc/arm64-v8a/lib/libminiupnpc.a
	${USR}/fmt/arm64-v8a/lib/libfmt.a
	${USR}/libvorbis/arm64-v8a/lib/libvorbis.a
	${USR}/libvorbis/arm64-v8a/lib/libvorbisfile.a
	${USR}/libvorbis/arm64-v8a/lib/libvorbisenc.a
	${USR}/libogg/arm64-v8a/lib/libogg.a
	${BOOST_LIBS}
)
set(DYNAMIC_LIBS
	${USR}/icu/arm64-v8a/lib/libicui18n.so
	${USR}/icu/arm64-v8a/lib/libicuuc.so
	${USR}/icu/arm64-v8a/lib/libicudata.so
	${USR}/libxml2/arm64-v8a/lib/libxml2.so
	${USR}/libiconv/arm64-v8a/lib/libiconv.so
)
target_link_libraries(main PRIVATE
	-Wl,--whole-archive pyrogenesis -Wl,--no-whole-archive
	-Wl,--start-group ${STATIC_LIBS} -Wl,--end-group
	${DYNAMIC_LIBS}
	SDL3::SDL3
	hilog_ndk.z ohinput dl m pthread
)
set_target_properties(main PROPERTIES OUTPUT_NAME main)

# libxml2 loads liblzma.so.5 at runtime (XZ-compressed docs). Copy it next to
# the other built .so so the packager includes it.
add_custom_command(TARGET main POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${USR}/xz/arm64-v8a/lib/liblzma.so.5
		$<TARGET_FILE_DIR:main>/liblzma.so.5)

# ---------------- libclibridge.so (NAPI command bridge) ----------------
# ArkTS imports { command } from 'libclibridge.so'; it forwards into libmain.
add_library(clibridge SHARED ohos_napi.cpp)
target_include_directories(clibridge PRIVATE
	/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot/usr/include)
target_link_libraries(clibridge PRIVATE ace_napi.z dl)
set_target_properties(clibridge PROPERTIES OUTPUT_NAME clibridge PREFIX lib)
# hvigor only explicitly builds the engine targets; make clibridge a build
# dependency of main so it is produced and lands in the packaged obj dir.
add_dependencies(main clibridge)
