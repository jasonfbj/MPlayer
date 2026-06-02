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

add_library(MPlayerFFmpeg INTERFACE)
target_include_directories(MPlayerFFmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})

set(FFMPEG_LIBS avcodec avformat avutil swscale swresample avfilter)

if(WIN32)
    set(FFMPEG_LINK_LIBS "")
    foreach(lib ${FFMPEG_LIBS})
        list(APPEND FFMPEG_LINK_LIBS ${FFMPEG_LIB_DIR}/${lib}.lib)
    endforeach()
    target_link_libraries(MPlayerFFmpeg INTERFACE ${FFMPEG_LINK_LIBS})
elseif(ANDROID)
    # Android 使用共享库 (.so)，避免静态库 -fPIC / LLVM 版本兼容性问题
    target_link_directories(MPlayerFFmpeg INTERFACE ${FFMPEG_LIB_DIR})
    set(FFMPEG_LINK_LIBS "")
    foreach(lib ${FFMPEG_LIBS})
        list(APPEND FFMPEG_LINK_LIBS ${lib})
    endforeach()
    target_link_libraries(MPlayerFFmpeg INTERFACE ${FFMPEG_LINK_LIBS})
else()
    set(FFMPEG_LINK_LIBS "")
    foreach(lib ${FFMPEG_LIBS})
        list(APPEND FFMPEG_LINK_LIBS ${FFMPEG_LIB_DIR}/lib${lib}.a)
    endforeach()
    target_link_libraries(MPlayerFFmpeg INTERFACE ${FFMPEG_LINK_LIBS})
endif()

set(MPLAYER_FFMPEG_PATH "${FFMPEG_PLATFORM_DIR}" PARENT_SCOPE)
