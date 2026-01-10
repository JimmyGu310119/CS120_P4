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
        titleLabel.setSize(300, 40);
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);
        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        auto processFunc = [this](FrameType &frame) {
            socket_t tool;
            auto conf1 = GlobalConfig().get(Config::NODE1);

            if (frame.type == Config::DNS_REQ) {
                fprintf(stderr, "[Gateway] DNS Query Received: %s\n", frame.body.c_str());
                char ip[100] = { 0 };

                // 补丁：手动初始化一下 Windows Socket 环境，防止 tool 没初始化
#if defined (_MSC_VER)
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                int ret = tool.hostname_to_ip(frame.body.c_str(), ip);

                if (ret == 0) {
                    fprintf(stderr, "[Gateway] Resolved: %s -> %s\n", frame.body.c_str(), ip);
                    FrameType resp{ Config::DNS_RSP, Str2IPType(conf1.ip), 53, std::string(ip) };
                    writer->send(resp);
                    fprintf(stderr, "[DNS Response] Sent to Node 1.\n");
                }
                else {
                    // 如果失败了，打印错误码
                    fprintf(stderr, "[Gateway] DNS Error! Return code: %d\n", ret);
                    // 即使解析失败，我们也回传一个错误信息给 Node 1，防止 Node 1 死等
                    FrameType resp{ Config::DNS_RSP, Str2IPType(conf1.ip), 53, "Error:0.0.0.0" };
                    writer->send(resp);
                }
            }
        };
        reader = new Reader(&directInput, &directInputLock, processFunc);
        reader->startThread();
        writer = new Writer(&directOutput, &directOutputLock);
    }

    void prepareToPlay(int, double) override { initThreads(); }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
        auto buffer = bufferToFill.buffer;
        auto bufferSize = buffer->getNumSamples();
        const float *data = buffer->getReadPointer(0);
        directInputLock.enter();
        for (int i = 0; i < bufferSize; ++i) { directInput.push(data[i]); }
        directInputLock.exit();
        buffer->clear();
        float *writePos = buffer->getWritePointer(0);
        directOutputLock.enter();
        for (int i = 0; i < bufferSize; ++i) {
            writePos[i] = directOutput.empty() ? 0.0f : directOutput.front();
            if (!directOutput.empty()) directOutput.pop();
        }
        directOutputLock.exit();
    }

    void releaseResources() override { delete reader; delete writer; }

    Reader *reader; Writer *writer;
    std::queue<float> directInput; CriticalSection directInputLock;
    std::queue<float> directOutput; CriticalSection directOutputLock;
    juce::Label titleLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};