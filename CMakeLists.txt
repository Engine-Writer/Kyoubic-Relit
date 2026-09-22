cmake_minimum_required(VERSION 3.15)

# Defaults to cl on Windows, clang++ elsewhere. Override with
# -DCMAKE_CXX_COMPILER=clang-cl / clang++ / g++ (eww) / whatever's on PATH
# flag STYLE follows the compiler you pick (MSVC-style for cl/clang-cl,
# GNU-style for g++/clang++). The TARGET binary follows the host you're
# building on (CMAKE_SYSTEM_NAME, which CMake itself already gets right
# for cross-compiles via a toolchain file. We don't need to reinvent
# that detection... ).
if(NOT DEFINED CMAKE_CXX_COMPILER)
    if(WIN32)
        set(CMAKE_CXX_COMPILER cl)
    else()
        set(CMAKE_CXX_COMPILER clang++)
    endif()
endif()

# Cross-building with clang-cl off Windows (WSL + xwin): CMake's own
# project()-time compiler sanity check runs before any target-level
# target_link_options() we set later ever apply, and clang-cl's default
# linker choice off-Windows is plain `ld`, which chokes on MSVC-style
# `-nologo`. Force lld globally, early, via CMAKE_EXE_LINKER_FLAGS
# WIN32 isn't set yet this early, so check CMAKE_HOST_WIN32 (always
# available) and the compiler name directly instead. WARNING: UGLY DETECTION
if(NOT CMAKE_HOST_WIN32 AND CMAKE_CXX_COMPILER MATCHES "clang-cl")
    if(NOT DEFINED XWIN_DIR)
        if(DEFINED ENV{XWIN})
            set(XWIN_DIR $ENV{XWIN})
        else()
            set(XWIN_DIR $ENV{HOME}/.xwin)
        endif()
    endif()
    # Same -nologo-vs-plain-ld problem as above, plus: the compiler sanity
    # check links a real binary before any target_link_directories() we
    # set later ever runs, and needs libcmt.lib/oldnames.lib to do that
    # so the xwin CRT lib dirs have to be global and this early too.
    set(CMAKE_EXE_LINKER_FLAGS_INIT
        "-fuse-ld=lld -Xlinker /libpath:${XWIN_DIR}/crt/lib/x86_64 -Xlinker /libpath:${XWIN_DIR}/sdk/lib/um/x86_64 -Xlinker /libpath:${XWIN_DIR}/sdk/lib/ucrt/x86_64")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")
endif()

project(Kyoubic-Relit CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# IS_MSVC_STYLE: which FLAG SYNTAX this compiler speaks (/EHsc vs -Wall).
# CMAKE_CXX_SIMULATE_ID covers clang-cl (reports Clang as the real ID, MSVC
# as the simulated one) plain MSVC covers cl itself.
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(IS_MSVC_STYLE ON)
else()
    set(IS_MSVC_STYLE OFF)
endif()

if(NOT WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND IS_MSVC_STYLE)
    set(IS_XWIN_CROSS_BUILD ON)
else()
    set(IS_XWIN_CROSS_BUILD OFF)
endif()

if(IS_MSVC_STYLE)
    # this repo has no debug/release split (see README)
    set(BUILD_FLAGS /O2 /EHsc /nologo)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
    set(BUILD_FLAGS -O2)
endif()

if(IS_XWIN_CROSS_BUILD)
    # PLEASE README:
    # Cross-building off-Windows via xwin cuz clang-cl's own driver doesn't
    # know about the MSVC CRT/Windows SDK headers
    # point it at wherever `xwin --accept-license splat` put them
    # (default $HOME/.xwin, override with -DXWIN_DIR=... if yours lives elsewhere).
    if(NOT DEFINED XWIN_DIR)
        set(XWIN_DIR $ENV{HOME}/.xwin)
    endif()
endif()

if(NOT IS_XWIN_CROSS_BUILD)
    find_package(Threads REQUIRED)
endif()

file(GLOB MODEL_FILES "${CMAKE_SOURCE_DIR}/assets/models/*.obj")
add_custom_target(models ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/models"
)
foreach(model_file ${MODEL_FILES})
    get_filename_component(model_name ${model_file} NAME)
    add_custom_command(TARGET models POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${model_file}" "${CMAKE_BINARY_DIR}/models/${model_name}"
    )
endforeach()

add_executable(raytracer src/main.cpp src/obj.cpp)
add_dependencies(raytracer models)
target_compile_options(raytracer PRIVATE ${BUILD_FLAGS})
target_include_directories(raytracer PRIVATE src src/include)

add_executable(bake src/bake.cpp src/obj.cpp)
target_compile_options(bake PRIVATE ${BUILD_FLAGS})
target_include_directories(bake PRIVATE src src/include)

if(IS_XWIN_CROSS_BUILD)
    foreach(target raytracer bake)
        target_include_directories(${target} PRIVATE
            ${XWIN_DIR}/crt/include
            ${XWIN_DIR}/sdk/include/ucrt
            ${XWIN_DIR}/sdk/include/um
            ${XWIN_DIR}/sdk/include/shared
        )
        target_link_directories(${target} PRIVATE
            ${XWIN_DIR}/crt/lib/x86_64
            ${XWIN_DIR}/sdk/lib/um/x86_64
            ${XWIN_DIR}/sdk/lib/ucrt/x86_64
        )
        target_link_options(${target} PRIVATE -fuse-ld=lld)
    endforeach()
else()
    target_link_libraries(raytracer PRIVATE Threads::Threads)
    target_link_libraries(bake PRIVATE Threads::Threads)
endif()
