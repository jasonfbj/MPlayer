add_library(MPlayerAndroid SHARED
    jni/MPlayerJNI.cpp
    GLESRenderer.cpp
    AndroidAudioOutput.cpp
)

target_include_directories(MPlayerAndroid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MPlayerAndroid PRIVATE
    MPlayerCore
    EGL
    GLESv3
    OpenSLES
    android
    log
)
