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
            // 使用 JUCE 弹窗输入，解决终端不显示字符的问题
            auto* aw = new juce::AlertWindow("DNS Lookup", "Enter domain to resolve:", juce::MessageBoxIconType::QuestionIcon);
            aw->addTextEditor("domain", "www.baidu.com", "Domain:");
            aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            aw->addButton("Cancel", 0);

            aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result) {
                if (result == 1) { // 用户点了 OK
                    std::string domain = aw->getTextEditorContents("domain").toStdString();
                    auto conf2 = GlobalConfig().get(Config::NODE2);
                    FrameType frame{ Config::DNS_REQ, Str2IPType(conf2.ip), 53, domain };
                    writer->send(frame);
                    std::cout << "[Sent] DNS Request for " << domain << " sent." << std::endl;
                }
                delete aw;
                }));
            };
        addAndMakeVisible(dnsButton);

        // HTTP/TCP 按钮
        httpButton.setButtonText("HTTP Curl");
        httpButton.setSize(120, 40);
        httpButton.setCentrePosition(450, 140);
        httpButton.onClick = [this]() {
            auto* aw = new juce::AlertWindow("HTTP Curl", "Enter URL to fetch:", juce::MessageBoxIconType::QuestionIcon);
            aw->addTextEditor("url", "www.example.com", "URL:");
            aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            aw->addButton("Cancel", 0);

            aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result) {
                if (result == 1) {
                    currentUrl = aw->getTextEditorContents("url").toStdString();
                    auto conf2 = GlobalConfig().get(Config::NODE2);
                    FrameType synFrame{ Config::TCP_SYN, Str2IPType(conf2.ip), 80, "SEQ:0x12345678" };
                    writer->send(synFrame);
                    std::cout << "[TCP] SYN Sent for " << currentUrl << std::endl;
                }
                delete aw;
                }));
            };
        addAndMakeVisible(httpButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        auto processFunc = [this](FrameType& frame) {
            if (frame.type == Config::DNS_RSP) {
                // 收到解析结果，直接弹窗显示给 TA 看，更显眼！
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "DNS Result", "Resolved IP: " + frame.body);
                std::cout << "\n[DNS Result] Resolved IP: " << frame.body << std::endl;
            }
            else if (frame.type == Config::TCP_ACK) {
                auto conf2 = GlobalConfig().get(Config::NODE2);
                FrameType reqFrame{ Config::HTTP_REQ, Str2IPType(conf2.ip), 80, currentUrl };
                writer->send(reqFrame);
            }
            else if (frame.type == Config::HTTP_RSP) {
                // HTTP 结果太长，打印到控制台，同时弹个小提示
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "HTTP Success", "Page content received! See terminal.");
                std::cout << "\n[HTTP Content]:\n" << frame.body << std::endl;
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

    void releaseResources() override {
        if (reader) { reader->stopThread(1000); delete reader; reader = nullptr; }
        delete writer; writer = nullptr;
    }

    Reader* reader{ nullptr }; Writer* writer{ nullptr };
    std::queue<float> directInput; CriticalSection directInputLock;
    std::queue<float> directOutput; CriticalSection directOutputLock;
    juce::Label titleLabel; juce::TextButton dnsButton, httpButton;
    std::string currentUrl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};