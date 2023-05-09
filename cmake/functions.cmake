function ( suppress_warnings TARGET_NAME )
    get_target_property( target_options ${TARGET_NAME} COMPILE_OPTIONS )

    if ( MSVC )
        list( REMOVE_ITEM target_options /W4 )
    else ()
        list( APPEND target_options -w )
    endif ()

    set_property( TARGET ${TARGET_NAME} PROPERTY COMPILE_OPTIONS ${target_options} )
endfunction ()

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
