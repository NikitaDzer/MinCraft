include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(RANGES_CXX_STD 20)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24") # Faster fetching with
                                                  # delegating to find_package
                                                  # if version allows
    FetchContent_Declare(
        ranges_lib
        GIT_REPOSITORY https://github.com/ericniebler/range-v3
        GIT_TAG 0.12.0
        FIND_PACKAGE_ARGS
        NAMES
        range-v3)

    FetchContent_Declare(
        glm_lib
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
        FIND_PACKAGE_ARGS
        NAMES
        glm)
else() # Fallback for lower versions
    FetchContent_Declare(
        ranges_lib
        GIT_REPOSITORY https://github.com/ericniebler/range-v3
        GIT_TAG 0.12.0)

    FetchContent_Declare(
        glm_lib
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8)
endif()

FetchContent_Declare(
    vma_lib
    GIT_REPOSITORY
        https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.0.1)

FetchContent_Declare(
    spdlog_lib
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0)

FetchContent_Declare(
    perlin_lib
    GIT_REPOSITORY https://github.com/Reputeless/PerlinNoise.git
    GIT_TAG v3.0.0)

FetchContent_MakeAvailable(ranges_lib spdlog_lib glm_lib perlin_lib vma_lib)

add_library(perlin INTERFACE)
target_include_directories(perlin INTERFACE ${perlin_lib_SOURCE_DIR})

# -v- VulkanMemoryAllocator configuration -v-
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)

# Force CMake to override lib's options with ours.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

target_compile_features(VulkanMemoryAllocator PRIVATE cxx_std_20)
suppress_warnings(VulkanMemoryAllocator)
# -^- VulkanMemoryAllocator configuration -^-

# Hacky workaround for using ktxlib without compiling from source

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW
    )# Fix warning https://cmake.org/cmake/help/latest/policy/CMP0135.html
endif()

FetchContent_Declare(
    ktxlib
    URL https://github.com/KhronosGroup/KTX-Software/releases/download/v4.1.0/KTX-Software-4.1.0-Linux-x86_64.tar.bz2
    URL_HASH SHA1=20494aa8e62749a6daabab583b03c4d8e8aed535)

FetchContent_MakeAvailable(ktxlib)
find_package(Ktx 4.1.0 EXACT REQUIRED PATHS ${ktxlib_SOURCE_DIR}/lib/cmake/ktx/)

# External dependecies
find_package(glfw3 3.3.0 REQUIRED)
find_package(Vulkan 1.3.0 REQUIRED)
find_package(Boost 1.61.0 REQUIRED COMPONENTS program_options)
find_package(Threads REQUIRED)
find_program(
    glslc
    NAMES glslc
    HINTS Vulkan::glslc REQUIRED)
