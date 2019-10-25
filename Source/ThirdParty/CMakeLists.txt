#
# Copyright (c) 2008-2017 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Third party warnings are irrelevant to us.
if (MSVC)
	ucm_add_flags("/W0")
else ()
	ucm_add_flags("-w")
endif ()

add_subdirectory(EASTL)
add_subdirectory(entt)
add_subdirectory(LZ4)
add_subdirectory(PugiXml)
add_subdirectory(rapidjson)
add_subdirectory(STB)
add_subdirectory(IconFontCppHeaders)
add_subdirectory(CLI11)
add_subdirectory(fmt)
add_subdirectory(spdlog)
add_subdirectory(SDL)
add_subdirectory(crunch)
add_subdirectory(ETCPACK)

if (NOT MINI_URHO)
    add_subdirectory(FreeType)

    if (URHO3D_GLOW)
        add_subdirectory(thekla)
        if (NOT URHO3D_MERGE_STATIC_LIBS)
            install(TARGETS nvmesh nvcore nvmath nvimage posh EXPORT Urho3D ARCHIVE DESTINATION ${DEST_ARCHIVE_DIR})
        endif ()

        set (EMBREE_ISPC_SUPPORT OFF CACHE BOOL "")
        set (EMBREE_TUTORIALS OFF CACHE BOOL "")
        set (EMBREE_STATIC_LIB OFF CACHE BOOL "")
        set (EMBREE_TASKING_SYSTEM "INTERNAL" CACHE STRING "")
        add_definitions(-DEMBREE_STATIC_LIB)
        # TODO: Try to remove CMAKE_INCLUDE_CURRENT_DIR at all
        set (CMAKE_INCLUDE_CURRENT_DIR OFF)
        add_subdirectory(embree)
        set (CMAKE_INCLUDE_CURRENT_DIR ON)

        if (NOT URHO3D_MERGE_STATIC_LIBS)
            install(TARGETS embree tasking EXPORT Urho3D ARCHIVE DESTINATION ${DEST_ARCHIVE_DIR})
        endif ()
    endif ()

    if (URHO3D_SYSTEMUI)
        add_subdirectory(imgui)
    endif ()

    if (URHO3D_NETWORK)
        add_subdirectory (Civetweb)
        add_subdirectory (SLikeNet)
    endif ()

    if (URHO3D_IK)
        add_subdirectory (ik)
    endif ()

    if (URHO3D_NAVIGATION)
        add_subdirectory (Detour)
        add_subdirectory (DetourCrowd)
        add_subdirectory (DetourTileCache)
        add_subdirectory (Recast)
    endif ()

    if (URHO3D_URHO2D)
        add_subdirectory (Box2D)
    endif ()

    if (URHO3D_WEBP)
        add_subdirectory (WebP)
    endif ()

    if (URHO3D_PHYSICS)
        add_subdirectory (Bullet)
        add_subdirectory (StanHull)
    endif ()

    if (URHO3D_TOOLS)
        add_subdirectory(nativefiledialog)
    endif ()

    if (NOT ANDROID AND NOT ARM AND NOT WEB)
        if (URHO3D_OPENGL)
            add_subdirectory (GLEW)
        elseif (URHO3D_D3D9)
            add_subdirectory (MojoShader)
        endif ()
    endif ()

    if (URHO3D_PROFILING)
        if (NOT TARGET GLEW AND URHO3D_TOOLS)
            add_subdirectory (GLEW)
        endif ()
        add_subdirectory (tracy)
    endif ()

    if (URHO3D_TOOLS)
        add_subdirectory(Assimp)
    endif ()

    if (URHO3D_CSHARP)
        add_subdirectory(swig)
    endif ()
endif ()
if (WIN32 OR APPLE)
	add_subdirectory (LibCpuId)
endif ()

vs_group_subdirectory_targets(${CMAKE_CURRENT_SOURCE_DIR} ThirdParty)
