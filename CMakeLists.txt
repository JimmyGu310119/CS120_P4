cmake_minimum_required(VERSION 3.15)

# 显式声明支持 C 和 CXX 两种语言
project(AetherNet_Project4 VERSION 1.0.0 LANGUAGES C CXX)

# 强力开启语言支持（防止生成阶段变量丢失）
enable_language(C)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 JUCE 文件夹
add_subdirectory(JUCE)

# 包含头文件目录
include_directories(include)

# Windows 联网必须链接的库
set(NET_LIBS ws2_32)

# --- 构建 Node1 (客户端) ---
juce_add_gui_app(Node1
    PRODUCT_NAME "Node1"
)

target_sources(Node1 PRIVATE
    include/main.cpp
    Node1/Node.h
    include/utils.cpp
    include/config.cpp
    include/socket.cpp
    include/reader.h
    include/writer.h
)

target_link_libraries(Node1 PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    ${NET_LIBS}
)

# --- 构建 Node2 (网关) ---
juce_add_gui_app(Node2
    PRODUCT_NAME "Node2"
)

target_sources(Node2 PRIVATE
    include/main.cpp
    Node2/Node.h
    include/utils.cpp
    include/config.cpp
    include/socket.cpp
    include/reader.h
    include/writer.h
)

target_link_libraries(Node2 PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    ${NET_LIBS}
)