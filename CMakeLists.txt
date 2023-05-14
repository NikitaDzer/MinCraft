cmake_minimum_required(VERSION 3.13)
project(MinCraft)

option(INSOURCEBUILD OFF) # Prevent in-source style builds
if((${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR}) AND NOT
                                                              ${INSOURCEBUILD})
    message(
        FATAL_ERROR
            "In-source building disabled. Provide -DINSOURCEBUILD option if you are sure about it."
    )
endif()

option(SANITIZE OFF)
if(${SANITIZE})
    add_compile_options(-fsanitize=undefined,address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined,address -fno-omit-frame-pointer)
endif()

option(THREAD_SANITIZE OFF)
if(${THREAD_SANITIZE})
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread -fno-omit-frame-pointer)
endif()

if(THREAD_SANITIZE AND SANITIZE)
    message(FATAL_ERROR "Thread and address sanitizer can't be used together")
endif()

include(cmake/functions.cmake)
include(cmake/dependencies.cmake)
include(cmake/imgui.cmake)

set(VKWRAP_SOURCES src/vk/debug.cc src/vk/loader.cc src/vk/surface.cc
                   src/glfw/window.cc)

add_library(vkwrap ${VKWRAP_SOURCES})
target_include_directories(vkwrap PUBLIC include/vkwrap include/common
                                         include/glfw)
target_link_libraries(vkwrap PUBLIC range-v3 Boost::boost spdlog glfw
                                    VulkanMemoryAllocator Vulkan::Vulkan)
target_enable_linter(vkwrap)
target_compile_features(vkwrap PUBLIC cxx_std_20)

set(CHUNK_SOURCES src/chunk/chunk_man.cc src/chunk/chunk_gen.cc
                  src/chunk/chunk_mesher.cc)

add_library(chunk ${CHUNK_SOURCES})
target_include_directories(chunk PUBLIC include/chunk include/common)
target_enable_linter(chunk)
enable_warnings(chunk)
target_compile_features(chunk PUBLIC cxx_std_20)
target_link_libraries(chunk PRIVATE perlin range-v3 Boost::boost)

function(add_example_executable TARGET_NAME SOURCE)
    add_executable(${TARGET_NAME} ${SOURCE})
    target_link_libraries(${TARGET_NAME} PRIVATE vkwrap)
    target_enable_linter(${TARGET_NAME})
    enable_warnings(${TARGET_NAME})
    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)
endfunction()

add_spirv_shader(vertex_shader shaders/vertex_shader.vert)
add_spirv_shader(fragment_shader shaders/fragment_shader.frag)

option(VK_INFO OFF)
if(${VK_INFO})
    add_example_executable(vk_info examples/vk_info.cc)
    add_dependencies(vk_info vertex_shader fragment_shader)
endif()

option(GLFW_INFO OFF)
if(${GLFW_INFO})
    add_example_executable(glfw_info examples/glfw_info.cc)
endif()

option(CHUNKS_TEST OFF)
if(${CHUNKS_TEST})
    add_example_executable(chunks_test examples/chunks_test.cc)
    target_link_libraries(chunks_test PRIVATE chunk)
endif()

# Main app target
set(MINCRAFT_SOURCES src/mincraft/mincraft.cc src/mincraft/info_gui.cc)

add_executable(mincraft ${MINCRAFT_SOURCES})
target_link_libraries(
    mincraft
    PRIVATE vkwrap
            gui_impl
            KTX::ktx
            chunk
            glm
            Boost::boost
            Boost::program_options)

enable_warnings(mincraft)
target_enable_linter(mincraft)
target_compile_features(mincraft PUBLIC cxx_std_20)
target_include_directories(mincraft PRIVATE include/imgui)
add_dependencies(mincraft vertex_shader fragment_shader)

# Copy texture directory
add_custom_command(
    TARGET mincraft
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/textures
            $<TARGET_FILE_DIR:mincraft>)
