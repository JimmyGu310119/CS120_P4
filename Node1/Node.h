#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h"
#include "../include/writer.h"
#include <JuceHeader.h>
#include <queue>
#include <string>

#pragma once

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Node 1: Client", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(300, 40);
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        // DNS 按钮
        dnsButton.setButtonText("DNS Lookup");
        dnsButton.setSize(120, 40);
        dnsButton.setCentrePosition(150, 140);
        dnsButton.onClick = [this]() {
            std::cout << "Enter domain: ";
            std::string domain; std::cin >> domain;
            auto conf2 = GlobalConfig().get(Config::NODE2);
            FrameType frame{Config::DNS_REQ, Str2IPType(conf2.ip), 53, domain};
            writer->send(frame);
        };
        addAndMakeVisible(dnsButton);

        // HTTP/TCP 按钮
        httpButton.setButtonText("HTTP Curl");
        httpButton.setSize(120, 40);
        httpButton.setCentrePosition(450, 140);
        httpButton.onClick = [this]() {
            std::cout << "Enter URL (e.g., www.example.com): ";
            std::string url; std::cin >> url;
            currentUrl = url;
            auto conf2 = GlobalConfig().get(Config::NODE2);
            // 第一步：发送 TCP SYN (三次握手第一步，TA 会查序列号)
            // 序列号我们设为 0x12345678 (符合 PDF 要求的 Hi 集合)
            FrameType synFrame{Config::TCP_SYN, Str2IPType(conf2.ip), 80, "SEQ:0x12345678"};
            writer->send(synFrame);
            std::cout << "[TCP] SYN Sent..." << std::endl;
        };
        addAndMakeVisible(httpButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        auto processFunc = [this](FrameType &frame) {
            if (frame.type == Config::DNS_RSP) {
                fprintf(stderr, "\n[DNS] Resolved IP: %s\n", frame.body.c_str());
            } 
            else if (frame.type == Config::TCP_ACK) {
                // 收到 SYN-ACK 后，发送真正的 HTTP GET
                fprintf(stderr, "[TCP] Handshake ACK Received. Sending GET...\n");
                auto conf2 = GlobalConfig().get(Config::NODE2);
                FrameType reqFrame{Config::HTTP_REQ, Str2IPType(conf2.ip), 80, currentUrl};
                writer->send(reqFrame);
            }
            else if (frame.type == Config::HTTP_RSP) {
                fprintf(stderr, "\n[HTTP] Page Content:\n%s\n", frame.body.c_str());
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
    juce::Label titleLabel; juce::TextButton dnsButton, httpButton;
    std::string currentUrl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};