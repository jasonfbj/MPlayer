add_executable(MPlayerWin WIN32
    main.cpp
    MPlayerApp.cpp
    MPlayerFrame.cpp
    VideoPanel.cpp
    ControlPanel.cpp
    D3D11Renderer.cpp
    D3D11VAHardwareDecoder.cpp
    WinAudioOutput.cpp
)

# wxWidgets paths
set(WXWIDGETS_ROOT "${CMAKE_SOURCE_DIR}/third_party/wxWidgets/windows/x64")
set(WXWIDGETS_INCLUDE_DIR "${WXWIDGETS_ROOT}/include")
set(WXWIDGETS_INCLUDE_DIR_MSVC "${WXWIDGETS_ROOT}/include/msvc")
set(WXWIDGETS_LIB_DIR "${WXWIDGETS_ROOT}/lib/vc_x64_lib")

target_include_directories(MPlayerWin PRIVATE
    ${WXWIDGETS_INCLUDE_DIR}
    ${WXWIDGETS_INCLUDE_DIR_MSVC}
)

target_link_libraries(MPlayerWin PRIVATE
    MPlayerCore
    # wxWidgets static libraries
    ${WXWIDGETS_LIB_DIR}/wxmsw32u_core.lib
    ${WXWIDGETS_LIB_DIR}/wxbase32u.lib
    ${WXWIDGETS_LIB_DIR}/wxregexu.lib
    ${WXWIDGETS_LIB_DIR}/wxexpat.lib
    ${WXWIDGETS_LIB_DIR}/wxpng.lib
    ${WXWIDGETS_LIB_DIR}/wxzlib.lib
    ${WXWIDGETS_LIB_DIR}/wxjpeg.lib
    ${WXWIDGETS_LIB_DIR}/wxtiff.lib
    # D3D11
    d3d11
    dxgi
    d3dcompiler
    dxguid
    # Windows system libs
    imm32
    ole32
    oleaut32
    comctl32
    uxtheme
    version
    ws2_32
    winmm
    rpcrt4
    kernel32
    user32
    gdi32
    winspool
    comdlg32
    advapi32
    shell32
    uuid
)

if(MSVC)
    target_compile_options(MPlayerWin PRIVATE /utf-8)
    target_compile_definitions(MPlayerWin PRIVATE
        UNICODE
        _UNICODE
        wxNO_NET_LIB
        wxNO_XML_LIB
        wxNO_HTML_LIB
        wxNO_QA_LIB
        wxNO_XRC_LIB
        wxNO_AUI_LIB
        wxNO_PROPGRID_LIB
        wxNO_RIBBON_LIB
        wxNO_RICHTEXT_LIB
        wxNO_MEDIA_LIB
        wxNO_STC_LIB
        wxNO_WEBVIEW_LIB
        wxNO_GL_LIB
    )
endif()

# Post-build: copy FFmpeg DLLs to output directory
set(FFMPEG_DLL_DIR "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/windows/x64/bin")
if(EXISTS "${FFMPEG_DLL_DIR}")
    add_custom_command(TARGET MPlayerWin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_DLL_DIR}"
            "$<TARGET_FILE_DIR:MPlayerWin>"
        COMMENT "Copying FFmpeg DLLs to output directory"
    )
endif()
