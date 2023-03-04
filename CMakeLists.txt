cmake_minimum_required ( VERSION 3.11 )
project ( MinCraft )

option ( INSOURCEBUILD OFF ) # Prevent in-source style builds
if ( (${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR}) AND NOT ${INSOURCEBUILD} )
    message ( FATAL_ERROR "In-source building disabled. Provide -DINSOURCEBUILD option if you are sure about it." )
endif ()

option ( LINT OFF )

if ( ${LINT} )
    find_program( CLANG_TIDY_EXECUTABLE "clang-tidy" )
    
    set(
        CLANG_TIDY_COMMAND "${CLANG_TIDY_EXECUTABLE}"
        "-checks=-*,modernize-*,-modernize-use-trailing-return-type*,readability-*,portability-*,bugprone-*" 
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
if ( SANITIZE )
    add_compile_options( -fsanitize=undefined,address -fno-omit-frame-pointer )
    add_link_options( -fsanitize=undefined,address -fno-omit-frame-pointer )
endif ()


# External dependecies
find_package ( glfw3 REQUIRED )
find_package ( Vulkan 1.3 REQUIRED )
find_program ( glslc NAMES glslc HINTS Vulkan::glslc REQUIRED )

# Quality of life external libs
add_subdirectory ( 3rd-party/range-v3 EXCLUDE_FROM_ALL )
add_subdirectory ( 3rd-party/spdlog EXCLUDE_FROM_ALL )

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
)

add_library( vkwrap ${VKWRAP_SOURCES} )
target_include_directories( vkwrap PUBLIC include/vk include/common )
target_link_libraries( vkwrap range-v3 spdlog )
target_enable_linter ( vkwrap )
target_compile_features ( vkwrap PUBLIC cxx_std_20 )

add_executable ( vkinfo examples/vkinfo.cc )
target_include_directories ( vkinfo PRIVATE include/common )
target_link_libraries( vkinfo PRIVATE glfw Vulkan::Vulkan vkwrap )
target_enable_linter ( vkinfo )
target_compile_features ( vkinfo PUBLIC cxx_std_20 )