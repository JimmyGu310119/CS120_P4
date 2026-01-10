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
                fprintf(stderr, "\n[Gateway] Incoming DNS Query from Node 1: %s\n", frame.body.c_str());
                fprintf(stderr, "[Gateway] Step 1: Checking local cache... (Miss)\n");
                fprintf(stderr, "[Gateway] Step 2: Performing recursive resolution to upstream DNS...\n");

                char ip[100] = { 0 };
#if defined (_MSC_VER)
                WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                int ret = tool.hostname_to_ip(frame.body.c_str(), ip);

                if (ret == 0) {
                    fprintf(stderr, "[Gateway] Step 3: Upstream DNS returned %s\n", ip);
                    fprintf(stderr, "[Gateway] Step 4: Encapsulating DNS Response and sending via Audio...\n");
                    FrameType resp{ Config::DNS_RSP, Str2IPType(conf1.ip), 53, std::string(ip) };
                    writer->send(resp);
                    fprintf(stderr, "[Gateway] DNS Done.\n");
                }
                else {
                    fprintf(stderr, "[Gateway] DNS Resolution failed.\n");
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
            // Node2/Node.h 里的 HTTP 处理部分
            else if (frame.type == Config::HTTP_REQ) {
                fprintf(stderr, "[Gateway] HTTP Request Received for: %s\n", frame.body.c_str());

                char target_ip[100] = { 0 };
                socket_t tool;
#if defined (_MSC_VER)
                WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                if (tool.hostname_to_ip(frame.body.c_str(), target_ip) == 0) {
                    tcp_client_t client;
                    if (client.connect(target_ip, 80) == 0) {
                        std::string httpRequest = "GET / HTTP/1.1\r\nHost: " + frame.body + "\r\nConnection: close\r\n\r\n";
                        client.write_all(httpRequest.c_str(), (int)httpRequest.size());

                        char buf[2048] = { 0 }; // 缓冲区开大一点
                        int bytesRead = client.read_all(buf, 2047);

                        if (bytesRead > 0) {
                            fprintf(stderr, "[Gateway] Total: %d bytes. Starting segmentation...\n", bytesRead);

                            // --- 关键修改：每 100 字节切一刀 ---
                            int chunkSize = 100;
                            for (int i = 0; i < bytesRead; i += chunkSize) {
                                int currentSize = (std::min)(chunkSize, bytesRead - i);
                                std::string chunkBody(buf + i, currentSize);

                                // 构造小包发送 (IP强制写死为笔记本的 10.0.0.1，确保它能认出来)
                                FrameType chunkFrame{ Config::HTTP_RSP, Str2IPType("10.0.0.1"), 80, chunkBody };
                                writer->send(chunkFrame);

                                fprintf(stderr, "[Gateway] Sent Chunk: %d/%d bytes\n", i + currentSize, bytesRead);

                                // 物理层间隔：发完一段停一下，让笔记本喘口气
                                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                            }
                            fprintf(stderr, "[Gateway] All segments sent successfully.\n");
                        }
                    }
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