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
