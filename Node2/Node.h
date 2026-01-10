#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h"
#include "../include/writer.h"
#include "../include/socket.h"
#include <JuceHeader.h>
#include <queue>

#pragma once

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Node 2: Gateway", juce::NotificationType::dontSendNotification);
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        settingsButton.setButtonText("Audio Settings");
        settingsButton.setSize(120, 40);
        settingsButton.setCentrePosition(300, 140);
        settingsButton.onClick = [this]() {
            juce::DialogWindow::LaunchOptions options;
            auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager, 1, 2, 1, 2, false, false, true, false);
            selector->setSize(500, 450);
            options.content.setOwned(selector);
            options.dialogTitle = "Audio Settings";
            options.componentToCentreAround = this;
            options.launchAsync();
            };
        addAndMakeVisible(settingsButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        auto processFunc = [this](FrameType& frame) {
            socket_t tool;
            auto conf1 = GlobalConfig().get(Config::NODE1); // 目标是发回给 Node 1

            // --- 逻辑 1: 处理 DNS 请求 ---
            if (frame.type == Config::DNS_REQ) {
                fprintf(stderr, "[Gateway] DNS Query Received: %s\n", frame.body.c_str());
                char ip[100] = { 0 };
#if defined (_MSC_VER)
                WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
                if (tool.hostname_to_ip(frame.body.c_str(), ip) == 0) {
                    fprintf(stderr, "[Gateway] Resolved: %s -> %s\n", frame.body.c_str(), ip);
                    FrameType resp{ Config::DNS_RSP, Str2IPType("1234"), 53, std::string(ip) };
                    writer->send(resp);
                }
            }
            // --- 逻辑 2: 处理 TCP SYN (握手第一步) ---
            else if (frame.type == Config::TCP_SYN) {
                fprintf(stderr, "[Gateway] TCP SYN Received (Handshake Part 1): %s\n", frame.body.c_str());
                // 回复一个 ACK，告诉 Node 1 可以发 HTTP 请求了
                FrameType ack{ Config::TCP_ACK, Str2IPType("1234"), 80, "ACK:OK" };
                writer->send(ack);
                fprintf(stderr, "[Gateway] TCP ACK Sent to Node 1.\n");
            }
            // --- 逻辑 3: 处理 HTTP 请求 (抓取网页) ---
            else if (frame.type == Config::HTTP_REQ) {
                fprintf(stderr, "[Gateway] HTTP Request Received for: %s\n", frame.body.c_str());

                // 初始化网络环境
#if defined (_MSC_VER)
                WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                char target_ip[100] = { 0 };
                socket_t tool;

                // 使用我们修正后的函数解析域名
                if (tool.hostname_to_ip(frame.body.c_str(), target_ip) == 0) {
                    fprintf(stderr, "[Gateway] DNS Pre-resolve: %s -> %s\n", frame.body.c_str(), target_ip);

                    tcp_client_t client;
                    // 注意：www.example.com 有时拦截爬虫，我们也可以试试 http://101.pku.edu.cn
                    if (client.connect(target_ip, 80) == 0) {
                        fprintf(stderr, "[Gateway] Connected to remote host at %s\n", target_ip);

                        // 构造请求头（必须包含 User-Agent，防止被服务器拒绝）
                        std::string httpRequest =
                            "GET / HTTP/1.1\r\n"
                            "Host: " + frame.body + "\r\n"
                            "User-Agent: AetherNet/1.0\r\n"
                            "Connection: close\r\n\r\n";

                        client.write_all(httpRequest.c_str(), (int)httpRequest.size());

                        // 接收响应
                        char buf[1024] = { 0 };
                        int bytesRead = client.read_all(buf, 1023);

                        if (bytesRead > 0) {
                            fprintf(stderr, "[Gateway] Data fetched (%d bytes). Sending to Node 1...\n", bytesRead);
                            FrameType httpResp{ Config::HTTP_RSP, Str2IPType("1234"), 80, std::string(buf, bytesRead) };
                            writer->send(httpResp);
                        }
                        else {
                            fprintf(stderr, "[Gateway] No data received from server.\n");
                        }
                    }
                    else {
                        fprintf(stderr, "[Gateway] Connect to %s failed.\n", target_ip);
                    }
                }
                else {
                    fprintf(stderr, "[Gateway] Cannot resolve: %s\n", frame.body.c_str());
                }
            }
            };
        reader = new Reader(&directInput, &directInputLock, processFunc);
        reader->startThread();
        writer = new Writer(&directOutput, &directOutputLock);
    }

    void prepareToPlay(int, double) override { initThreads(); }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto buffer = bufferToFill.buffer;
        auto bufferSize = buffer->getNumSamples();
        const float* data = buffer->getReadPointer(0);
        directInputLock.enter();
        for (int i = 0; i < bufferSize; ++i) { directInput.push(data[i]); }
        directInputLock.exit();
        buffer->clear();
        float* writePos = buffer->getWritePointer(0);
        directOutputLock.enter();
        for (int i = 0; i < bufferSize; ++i) {
            writePos[i] = directOutput.empty() ? 0.0f : directOutput.front();
            if (!directOutput.empty()) directOutput.pop();
        }
        directOutputLock.exit();
    }

    void releaseResources() override { delete reader; delete writer; }

    Reader* reader{ nullptr }; Writer* writer{ nullptr };
    std::queue<float> directInput; CriticalSection directInputLock;
    std::queue<float> directOutput; CriticalSection directOutputLock;
    juce::Label titleLabel; juce::TextButton settingsButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};