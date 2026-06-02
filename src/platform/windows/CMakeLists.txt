add_executable(MPlayerWin WIN32
    main.cpp
    WinMainWindow.cpp
    D3D11Renderer.cpp
    D3D11VAHardwareDecoder.cpp
    WinAudioOutput.cpp
)

target_link_libraries(MPlayerWin PRIVATE
    MPlayerCore
    d3d11
    dxgi
    d3dcompiler
    dxguid
    imm32
    ole32
)

if(MSVC)
    target_compile_options(MPlayerWin PRIVATE /utf-8)
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
