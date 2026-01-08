#include <JuceHeader.h>
#include "Node.h" // 注意：CMake 会根据编译目标自动寻找 Node1/Node.h 或 Node2/Node.h

class Application : public juce::JUCEApplication {
public:
    Application() = default;

    const juce::String getApplicationName() override    { return "AetherNet Project 4"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise (const juce::String&) override {
        // 创建主窗口并将 Node.h 中定义的 MainContentComponent 加载进去
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), new MainContentComponent(), *this);
    }

    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow (const juce::String& name, juce::Component* c, JUCEApplication& a)
            : DocumentWindow (name, juce::Colours::darkgrey, juce::DocumentWindow::allButtons), app (a) {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (Application)