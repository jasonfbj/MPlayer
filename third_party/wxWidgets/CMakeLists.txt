# third_party/wxWidgets/CMakeLists.txt
# 与 third_party/ffmpeg 同模式：导入预编译 wxWidgets 库

if(NOT WIN32)
    return()  # wxWidgets 仅用于 Windows
endif()

set(WXWIDGETS_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows/x64")

set(WXWIDGETS_INCLUDE_DIR "${WXWIDGETS_PLATFORM_DIR}/include")
set(WXWIDGETS_INCLUDE_DIR_MSVC "${WXWIDGETS_PLATFORM_DIR}/include/msvc")
set(WXWIDGETS_LIB_DIR "${WXWIDGETS_PLATFORM_DIR}/lib/vc_x64_lib")

# 导入 wxWidgets 作为 interface library
add_library(MPlayerWxWidgets INTERFACE IMPORTED)
target_include_directories(MPlayerWxWidgets INTERFACE
    ${WXWIDGETS_INCLUDE_DIR}
    ${WXWIDGETS_INCLUDE_DIR_MSVC}
)

# wxWidgets 3.2.8 静态链接需要的库（Conan x86_64 静态编译）
set(WXWIDGETS_LIBS
    wxmsw32u_core
    wxbase32u
    wxregexu
    wxexpat
    wxpng
    wxzlib
    wxjpeg
    wxtiff
)

foreach(lib ${WXWIDGETS_LIBS})
    if(EXISTS "${WXWIDGETS_LIB_DIR}/${lib}.lib")
        target_link_libraries(MPlayerWxWidgets INTERFACE
            ${WXWIDGETS_LIB_DIR}/${lib}.lib
        )
    else()
        message(WARNING "wxWidgets library not found: ${WXWIDGETS_LIB_DIR}/${lib}.lib")
    endif()
endforeach()

# wxWidgets 在 Windows 上的系统依赖
target_link_libraries(MPlayerWxWidgets INTERFACE
    comctl32
    uxtheme
    kernel32
    user32
    gdi32
    winspool
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    rpcrt4
    imm32
    version
    ws2_32
    winmm
)
