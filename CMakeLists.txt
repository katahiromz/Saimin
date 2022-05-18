# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(Saimin C CXX RC)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

##############################################################################

add_subdirectory(freeglut-3.2.2)

# Saimin.exe
add_executable(Saimin WIN32 Saimin.cpp tess.cpp Saimin_res.rc)
target_compile_definitions(Saimin PRIVATE -DINITGUID -DUNICODE -D_UNICODE -DFREEGLUT_LIB_PRAGMAS=0)
if(MSVC)
    target_include_directories(Saimin PRIVATE "../freeglut-MSVC-3.0.0-2.mp/include")
else()
    target_include_directories(Saimin PRIVATE "../freeglut-MinGW-3.0.0-1.mp/include")
endif()
target_link_libraries(Saimin PRIVATE comctl32 opengl32 glu32 shlwapi winmm freeglut)

##############################################################################
