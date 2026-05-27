add_library(MPlayerCore STATIC
    demuxer/Demuxer.cpp
    decoder/SoftwareDecoder.cpp
    decoder/DecoderFactory.cpp
    renderer/RendererFactory.cpp
    controller/PlayerController.cpp
)

target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/${MPLAYER_FFMPEG_PATH}/include
)

target_link_libraries(MPlayerCore PUBLIC MPlayerFFmpeg)
