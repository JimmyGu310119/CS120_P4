#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h"
#include "../include/writer.h"
#include <JuceHeader.h>
#include <queue>
#include <string>
#include <vector>

#pragma once

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Aethernet Terminal (Node 1)", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(400, 30);
        titleLabel.setCentrePosition(300, 20);
        addAndMakeVisible(titleLabel);

        // 模拟控制台日志显示区
        logDisplay.setMultiLine(true);
        logDisplay.setReadOnly(true);
        logDisplay.setScrollbarsShown(true);
        logDisplay.setCaretVisible(false);
        logDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
        logDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
        logDisplay.setFont(juce::Font("Consolas", 14.0f, juce::Font::plain));
        logDisplay.setBounds(20, 50, 560, 180);
        addAndMakeVisible(logDisplay);

        // 模拟终端输入框
        terminalInput.setReturnKeyStartsNewLine(false);
        terminalInput.onReturnKey = [this]() { processCommand(); };
        terminalInput.setBounds(20, 240, 560, 30);
        terminalInput.setTextToShowWhenEmpty("Enter command (e.g., ping www.baidu.com -n 10)", juce::Colours::grey);
        addAndMakeVisible(terminalInput);

        addLogLine("Aethernet v1.0 initialized. Ready for commands.");

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void addLogLine(const juce::String& line) {
        juce::MessageManager::callAsync([this, line]() {
            logDisplay.moveCaretToEnd();
            logDisplay.insertTextAtCaret(line + juce::newLine);
            });
    }

    void processCommand() {
        juce::String rawCmd = terminalInput.getText().trim();
        terminalInput.clear();
        if (rawCmd.isEmpty()) return;

        addLogLine("> " + rawCmd);
        std::string cmd = rawCmd.toStdString();

        // --- 解析 PING 指令 ---
        if (cmd.find("ping") == 0) {
            std::string domain = rawCmd.fromFirstOccurrenceOf("ping ", false, false).upToFirstOccurrenceOf(" ", false, false).toStdString();
            int count = 1;
            if (rawCmd.contains("-n")) {
                count = rawCmd.fromFirstOccurrenceOf("-n ", false, false).getIntValue();
            }
            if (count <= 0) count = 1;

            std::thread([this, domain, count]() {
                auto conf2 = GlobalConfig().get(Config::NODE2);
                for (int i = 0; i < count; ++i) {
                    addLogLine(juce::String::formatted("Ping Request %d/%d to %s...", i + 1, count, domain.c_str()));
                    FrameType frame{ Config::DNS_REQ, Str2IPType(conf2.ip), 53, domain };
                    writer->send(frame);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 等待回包
                }
                }).detach();
        }
        // --- 解析 CURL 指令 ---
        else if (cmd.find("curl") == 0) {
            std::string url = rawCmd.fromFirstOccurrenceOf("http://", false, false).toStdString();
            if (url.empty()) url = rawCmd.fromFirstOccurrenceOf("curl ", false, false).toStdString();

            std::thread([this, url]() {
                currentUrl = url;
                auto conf2 = GlobalConfig().get(Config::NODE2);
                addLogLine("Initiating TCP Handshake with " + juce::String(url) + "...");
                // 使用要求的 Initial Sequence Number 0x12345678
                FrameType synFrame{ Config::TCP_SYN, Str2IPType(conf2.ip), 80, "SEQ:0x12345678" };
                writer->send(synFrame);
                }).detach();
        }
        else {
            addLogLine("Unknown command. Use 'ping' or 'curl'.");
        }
    }

    void initThreads() {
        auto processFunc = [this](FrameType& frame) {
            if (frame.type == Config::DNS_RSP) {
                addLogLine("[DNS] Resolved: " + juce::String(frame.body));
            }
            else if (frame.type == Config::TCP_ACK) {
                addLogLine("[TCP] Handshake ACK received. Fetching HTTP...");
                auto conf2 = GlobalConfig().get(Config::NODE2);
                FrameType reqFrame{ Config::HTTP_REQ, Str2IPType(conf2.ip), 80, currentUrl };
                writer->send(reqFrame);
            }
            else if (frame.type == Config::HTTP_RSP) {
                addLogLine("[HTTP Data Received]: " + juce::String(frame.body.substr(0, 40)) + "...");
                // 收到网页源码弹窗
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "HTTP Content", frame.body);
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

    void releaseResources() override { if (reader) reader->stopThread(1000); delete reader; delete writer; }

    Reader* reader; Writer* writer;
    std::queue<float> directInput; CriticalSection directInputLock;
    std::queue<float> directOutput; CriticalSection directOutputLock;
    juce::Label titleLabel; juce::TextEditor logDisplay, terminalInput;
    std::string currentUrl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};