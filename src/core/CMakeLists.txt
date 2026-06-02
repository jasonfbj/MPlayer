add_library(MPlayerCore STATIC
    demuxer/Demuxer.cpp
    decoder/SoftwareDecoder.cpp
    decoder/DecoderFactory.cpp
    renderer/RendererFactory.cpp
    controller/PlayerController.cpp
)

target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(MPlayerCore PUBLIC MPlayerFFmpeg)
