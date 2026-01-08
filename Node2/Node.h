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
                fprintf(stderr, "[Gateway] DNS Query: %s\n", frame.body.c_str());
                char ip[100] = {0};
                if (tool.hostname_to_ip(frame.body.c_str(), ip) == 0) {
                    FrameType resp{Config::DNS_RSP, Str2IPType(conf1.ip), 53, std::string(ip)};
                    writer->send(resp);
                }
            } 
            else if (frame.type == Config::TCP_SYN) {
                // 收到 SYN，返回 ACK (完成模拟握手)
                fprintf(stderr, "[Gateway] TCP SYN Received. Sending ACK...\n");
                FrameType ack{Config::TCP_ACK, Str2IPType(conf1.ip), 80, "ACK:OK"};
                writer->send(ack);
            }
            else if (frame.type == Config::HTTP_REQ) {
                // 收到 GET 请求，台式机去真的联网抓取
                fprintf(stderr, "[Gateway] HTTP Fetching: %s\n", frame.body.c_str());
                tcp_client_t client;
                if (client.connect(frame.body.c_str(), 80) == 0) {
                    std::string getReq = "GET / HTTP/1.1\r\nHost: " + frame.body + "\r\nConnection: close\r\n\r\n";
                    client.write_all(getReq.c_str(), getReq.size());
                    char buf[1024] = {0};
                    int len = client.read_all(buf, 1023); // 抓取网页前 1KB
                    FrameType resp{Config::HTTP_RSP, Str2IPType(conf1.ip), 80, std::string(buf, len)};
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