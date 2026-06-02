add_library(MPlayerCore STATIC
    demuxer/Demuxer.cpp
    decoder/SoftwareDecoder.cpp
    decoder/DecoderFactory.cpp
    renderer/RendererFactory.cpp
    controller/PlayerController.cpp
    audio/AudioResampler.cpp
)

if(MSVC)
    target_compile_options(MPlayerCore PUBLIC /utf-8)
endif()

target_compile_definitions(MPlayerCore PUBLIC
    __STDC_CONSTANT_MACROS
    __STDC_LIMIT_MACROS
)

target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(MPlayerCore PUBLIC MPlayerFFmpeg)
