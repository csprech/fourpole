#include "engine/Engine.h"
#include "ui/MainComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace fourpole
{
class FourpoleApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        engine = std::make_unique<Engine>();
        window = std::make_unique<MainWindow> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        if (window != nullptr)
            if (auto* mc = dynamic_cast<MainComponent*> (window->getContentComponent()))
                mc->prepareToQuit();
        window.reset();
        engine.reset();
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, Engine& eng)
            : DocumentWindow (name, juce::Colour (0xff0b0b0b), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (eng), true);
            setResizable (true, true);
            setResizeLimits (1240, 720, 2560, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            if (auto* c = getContentComponent())
                c->grabKeyboardFocus();
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
        /** Coming back to the window (Dock, Cmd-Tab, or launched behind another app): the computer
            keyboard plays the synth straight away, without a click first. */
        void activeWindowStatusChanged() override
        {
            DocumentWindow::activeWindowStatusChanged();
            if (isActiveWindow())
                if (auto* c = getContentComponent(); c != nullptr && c->isShowing() && ! c->hasKeyboardFocus (true))
                    c->grabKeyboardFocus();
        }
    };

private:
    std::unique_ptr<Engine> engine;
    std::unique_ptr<MainWindow> window;
};
} // namespace fourpole

START_JUCE_APPLICATION (fourpole::FourpoleApplication)
