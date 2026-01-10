cmake_minimum_required(VERSION 3.15)
project(AetherNet_Project4 VERSION 1.0.0 LANGUAGES C CXX)

enable_language(C)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Boost 路径
include_directories("D:/boost") 

# 引入 JUCE
add_subdirectory(JUCE)

# 包含公共头文件目录
include_directories(include)

set(NET_LIBS ws2_32)

# --- 构建 Node1 (客户端) ---
juce_add_gui_app(Node1 PRODUCT_NAME "Node1")
juce_generate_juce_header(Node1)

# 【关键修改：告诉 Node1 去自己的文件夹找 Node.h】
target_include_directories(Node1 PRIVATE Node1)

target_sources(Node1 PRIVATE
    include/main.cpp
    Node1/Node.h
    include/utils.cpp
    include/config.cpp
    include/socket.cpp
    include/reader.h
    include/writer.h
)
target_link_libraries(Node1 PRIVATE juce::juce_audio_utils juce::juce_gui_extra ${NET_LIBS})

# --- 构建 Node2 (网关) ---
juce_add_gui_app(Node2 PRODUCT_NAME "Node2")
juce_generate_juce_header(Node2)

# 【关键修改：告诉 Node2 去自己的文件夹找 Node.h】
target_include_directories(Node2 PRIVATE Node2)

target_sources(Node2 PRIVATE
    include/main.cpp
    Node2/Node.h
    include/utils.cpp
    include/config.cpp
    include/socket.cpp
    include/reader.h
    include/writer.h
)
target_link_libraries(Node2 PRIVATE juce::juce_audio_utils juce::juce_gui_extra ${NET_LIBS})