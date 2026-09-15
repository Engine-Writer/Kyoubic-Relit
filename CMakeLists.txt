cmake_minimum_required(VERSION 3.15)

if(WIN32 AND NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER cl)
endif()

project(Kyoubic-Relit CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(BUILD_FLAGS /O2 /EHsc /nologo)
    # this repo has no debug/release split (see README)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
    set(BUILD_FLAGS -O2)
endif()

find_package(Threads REQUIRED)

add_executable(raytracer src/main.cpp src/obj.cpp)
target_compile_options(raytracer PRIVATE ${BUILD_FLAGS})
target_include_directories(raytracer PRIVATE src src/include)
target_link_libraries(raytracer PRIVATE Threads::Threads)

add_executable(bake src/bake.cpp src/obj.cpp)
target_compile_options(bake PRIVATE ${BUILD_FLAGS})
target_include_directories(bake PRIVATE src src/include)
target_link_libraries(bake PRIVATE Threads::Threads)
