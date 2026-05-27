# third_party/ffmpeg/CMakeLists.txt
# 根据平台自动选择预编译库路径

if(WIN32)
    set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows/x64")
elseif(ANDROID)
    if(${ANDROID_ABI} STREQUAL "arm64-v8a")
        set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/arm64-v8a")
    elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
        set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/armeabi-v7a")
    endif()
elseif(APPLE)
    set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/macOS/arm64")
endif()

if(NOT FFMPEG_PLATFORM_DIR)
    message(WARNING "FFmpeg platform dir not set, using fallback")
    set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/macOS/arm64")
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_PLATFORM_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_PLATFORM_DIR}/lib")

add_library(MPlayerFFmpeg INTERFACE IMPORTED)
target_include_directories(MPlayerFFmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})

set(FFMPEG_LIBS avcodec avformat avutil swscale swresample)

if(WIN32)
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(MPlayerFFmpeg INTERFACE
            ${FFMPEG_LIB_DIR}/${lib}.lib
        )
    endforeach()
else()
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(MPlayerFFmpeg INTERFACE
            ${FFMPEG_LIB_DIR}/lib${lib}.a
        )
    endforeach()
endif()

set(MPLAYER_FFMPEG_PATH "${FFMPEG_PLATFORM_DIR}" PARENT_SCOPE)
