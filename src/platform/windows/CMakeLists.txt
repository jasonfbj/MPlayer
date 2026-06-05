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
# Release: bundled in third_party; Debug: from Conan cache
set(WXWIDGETS_RELEASE_ROOT "${CMAKE_SOURCE_DIR}/third_party/wxWidgets/windows/x64")
set(WXWIDGETS_DEBUG_ROOT "H:/conan_data/data/wxWidgets/3.2.8/1602/dev/package/8cf01e2f50fcd6b63525e70584df0326550364e1")

# Use generator expressions to pick the right paths per config
set(WXWIDGETS_INCLUDE_DIR "$<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/include")
set(WXWIDGETS_INCLUDE_DIR_MSVC "$<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/include/msvc")
set(WXWIDGETS_LIB_DIR "$<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib")

target_include_directories(MPlayerWin PRIVATE
    ${WXWIDGETS_INCLUDE_DIR}
    ${WXWIDGETS_INCLUDE_DIR_MSVC}
)

target_link_libraries(MPlayerWin PRIVATE
    MPlayerCore
    # wxWidgets static libraries (Release: wx*32u, Debug: wx*32ud)
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxmsw32ud_core,wxmsw32u_core>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxbase32ud,wxbase32u>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxregexud,wxregexu>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxexpatd,wxexpat>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxpngd,wxpng>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxzlibd,wxzlib>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxjpegd,wxjpeg>.lib
    $<IF:$<CONFIG:Debug>,${WXWIDGETS_DEBUG_ROOT},${WXWIDGETS_RELEASE_ROOT}>/lib/vc_x64_lib/$<IF:$<CONFIG:Debug>,wxtiffd,wxtiff>.lib
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
