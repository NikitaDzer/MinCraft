cmake_minimum_required ( VERSION 3.13 )
project ( MinCraft )

option ( INSOURCEBUILD OFF ) # Prevent in-source style builds
if ( (${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR}) AND NOT ${INSOURCEBUILD} )
    message ( FATAL_ERROR "In-source building disabled. Provide -DINSOURCEBUILD option if you are sure about it." )
endif ()

if ( MSVC )
    add_compile_options( /W4 )
elseif ( "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" )
    add_compile_options (
        -Wall
        -Wextra
        -pedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wmisleading-indentation
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
elseif ( "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU" )
    add_compile_options (
        -Wall
        -Wextra
        -pedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
else ()
    message(FATAL_ERROR "Unsupported compiler: [${CMAKE_CXX_COMPILER_ID}]")
endif ()

option ( LINT OFF )

if ( ${LINT} )
    find_program( CLANG_TIDY_EXECUTABLE "clang-tidy" )
    
    set(
        CLANG_TIDY_COMMAND "${CLANG_TIDY_EXECUTABLE}"
        "-checks=-*,modernize-*,-modernize-use-trailing-return-type*,readability-*,-readability-implicit-bool-conversion*,\
        -readability-identifier-length*,portability-*,bugprone-*" 
    )

    function ( target_enable_linter TARGET_NAME )
        set_target_properties ( ${TARGET_NAME} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}" )
    endfunction ()
else ()
    function ( target_enable_linter TARGET_NAME )
        # Do nothing
    endfunction ()
endif ()

option ( SANITIZE OFF )
if ( ${SANITIZE} )
    add_compile_options( -fsanitize=undefined,address -fno-omit-frame-pointer )
    add_link_options( -fsanitize=undefined,address -fno-omit-frame-pointer )
endif ()

option ( THREAD_SANITIZE OFF )
if ( ${THREAD_SANITIZE} )
    add_compile_options( -fsanitize=thread -fno-omit-frame-pointer )
    add_link_options( -fsanitize=thread -fno-omit-frame-pointer )
endif ()

if ( THREAD_SANITIZE AND SANITIZE )
    message(FATAL_ERROR "Thread and address sanitizer can't be used together")
endif ()

# Quality of life external libs
set(RANGES_CXX_STD 20)
add_subdirectory ( 3rd-party/range-v3 EXCLUDE_FROM_ALL )
add_subdirectory ( 3rd-party/spdlog EXCLUDE_FROM_ALL )

if ( THREAD_SANITIZE AND SANITIZE )
    message(FATAL_ERROR "Thread and address sanitizer can't be used together")
endif ()

function ( add_spirv_shader TARGET_NAME INPUT_FILE )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv
        COMMAND ${glslc} -c ${INPUT_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv
        MAIN_DEPENDENCY ${INPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target ( ${TARGET_NAME} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv )
    set ( ${TARGET_NAME}_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv PARENT_SCOPE )
endfunction ()

function ( suppress_warnings TARGET_NAME )
    get_target_property( target_options ${TARGET_NAME} COMPILE_OPTIONS )

    if ( MSVC )
        list( REMOVE_ITEM target_options /W4 )
    else ()
        list( APPEND target_options -w )
    endif ()

    set_property( TARGET ${TARGET_NAME} PROPERTY COMPILE_OPTIONS ${target_options} )
endfunction ()

# Quality of life external libs
set( RANGES_CXX_STD 20 )
add_subdirectory ( 3rd-party/range-v3 EXCLUDE_FROM_ALL )
add_subdirectory ( 3rd-party/popl EXCLUDE_FROM_ALL )
add_subdirectory ( 3rd-party/spdlog EXCLUDE_FROM_ALL )

# External dependecies
find_package ( glfw3 REQUIRED )
find_package ( Vulkan 1.3 REQUIRED )
find_package ( Hana REQUIRED )
find_program ( glslc NAMES glslc HINTS Vulkan::glslc REQUIRED )

# -v- VulkanMemoryAllocator configuration -v-
set( VMA_STATIC_VULKAN_FUNCTIONS OFF )
set( VMA_DYNAMIC_VULKAN_FUNCTIONS ON )

# Force CMake to override lib's options with ours. 
set( CMAKE_POLICY_DEFAULT_CMP0077 NEW )

add_subdirectory ( 3rd-party/VulkanMemoryAllocator EXCLUDE_FROM_ALL )

target_compile_features( VulkanMemoryAllocator PRIVATE cxx_std_20 )
suppress_warnings( VulkanMemoryAllocator )
# -^- VulkanMemoryAllocator configuration -^-

function ( add_spirv_shader TARGET_NAME INPUT_FILE )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv
        COMMAND ${glslc} -c ${INPUT_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv
        MAIN_DEPENDENCY ${INPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target ( ${TARGET_NAME} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv )
    set ( ${TARGET_NAME}_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.spv PARENT_SCOPE )
endfunction ()

set ( VKWRAP_SOURCES
    src/vk/debug.cc
    src/vk/loader.cc
    src/vk/surface.cc
    src/glfw/window.cc 
)

add_library ( vkwrap ${VKWRAP_SOURCES} )
target_include_directories ( vkwrap PUBLIC include/vkwrap include/common include/glfw )
target_link_libraries ( vkwrap PUBLIC range-v3 hana spdlog glfw VulkanMemoryAllocator Vulkan::Vulkan )
target_enable_linter ( vkwrap )
target_compile_features ( vkwrap PUBLIC cxx_std_20 )

set ( CHUNK_SOURCES
    src/chunk/region.cc
    src/chunk/chunk_gen.cc
)

add_library( chunk ${CHUNK_SOURCES} )
target_include_directories( chunk PUBLIC include/chunk include/common )
target_enable_linter( chunk )
target_compile_features( chunk PUBLIC cxx_std_20 )

function( add_example_executable TARGET_NAME SOURCE )
    add_executable ( ${TARGET_NAME} ${SOURCE} )
    target_link_libraries ( ${TARGET_NAME} PRIVATE vkwrap )
    target_enable_linter ( ${TARGET_NAME} )
    target_compile_features ( ${TARGET_NAME} PUBLIC cxx_std_20 )
endfunction()

option ( VK_INFO OFF )
if ( ${VK_INFO} )
    add_example_executable( vk_info examples/vk_info.cc )
    add_spirv_shader( vertex_shader shaders/vertex_shader.vert )
    add_spirv_shader( fragment_shader shaders/fragment_shader.frag )
endif ()

option ( GLFW_INFO OFF )
if ( ${GLFW_INFO} )
    add_example_executable( glfw_info examples/glfw_info.cc )
endif()

option ( CHUNKS_TEST OFF )
if ( ${CHUNKS_TEST} )
    add_example_executable( chunks_test examples/chunks_test.cc )
    target_link_libraries( chunks_test PRIVATE chunk )
endif ()

# Imgui backend
set ( GUI_SOURCES 
    3rd-party/imgui/imgui.cpp
    3rd-party/imgui/imgui_demo.cpp
    3rd-party/imgui/imgui_draw.cpp
    3rd-party/imgui/imgui_tables.cpp
    3rd-party/imgui/imgui_widgets.cpp
    3rd-party/imgui/backends/imgui_impl_glfw.cpp
    3rd-party/imgui/backends/imgui_impl_vulkan.cpp
)

add_library ( gui_impl ${GUI_SOURCES} )
target_include_directories ( gui_impl PUBLIC 
    3rd-party/imgui 
    3rd-party/imgui/backends
)
suppress_warnings ( gui_impl )
 
# Main app target
set ( MINCRAFT_SOURCES
    src/mincraft.cc
)

add_executable ( mincraft ${MINCRAFT_SOURCES} )
target_link_libraries ( mincraft PRIVATE vkwrap popl gui_impl )
target_enable_linter ( mincraft )
target_compile_features ( mincraft PUBLIC cxx_std_20 )
target_include_directories( mincraft PRIVATE include/imgui )