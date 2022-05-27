# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.13)

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
# microphone

add_subdirectory(microphone)

##############################################################################
# FreeGLUT

set(FREEGLUT_BUILD_DEMOS OFF)

add_subdirectory(freeglut)

##############################################################################
# Saimin

# Saimin.exe
add_executable(Saimin WIN32 Saimin.cpp tess.cpp InputBox.cpp Saimin_res.rc)
target_compile_definitions(Saimin PRIVATE -DINITGUID -DUNICODE -D_UNICODE -DFREEGLUT_LIB_PRAGMAS=0)
target_include_directories(Saimin PRIVATE "freeglut/include")
target_link_libraries(Saimin PRIVATE comctl32 opengl32 glu32 shell32 shlwapi winmm freeglut_static microphone_static)
if(MSVC)
    target_link_options(Saimin PRIVATE "/entry:wWinMainCRTStartup")
else()
    target_link_options(Saimin PRIVATE "-municode")
endif()

##############################################################################
