add_executable(MPlayerWin WIN32
    main.cpp
    WinMainWindow.cpp
    D3D11Renderer.cpp
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
