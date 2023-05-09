include ( FetchContent )
set ( FETCHCONTENT_QUIET OFF )

include ( cmake/functions.cmake )

FetchContent_Declare (
    imgui_lib       
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.5
)

FetchContent_MakeAvailable ( imgui_lib )

set ( GUI_SOURCES 
    ${imgui_lib_SOURCE_DIR}/imgui.cpp
    ${imgui_lib_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_lib_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_lib_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_lib_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_lib_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_lib_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library ( gui_impl ${GUI_SOURCES} )
target_include_directories ( gui_impl PUBLIC 
    ${imgui_lib_SOURCE_DIR} 
    ${imgui_lib_SOURCE_DIR}/backends
)
suppress_warnings ( gui_impl )