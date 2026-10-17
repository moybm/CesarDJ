#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "DJLookAndFeel.h"
#include "Branding.h"

class CesarDJApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        // Pantalla de bienvenida con la Vespa mientras se prepara todo (se borra sola)
        auto* splash = new juce::SplashScreen (Branding::appName, Branding::createSplashImage(), true);

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        splash->toFront (false);
        splash->deleteAfterDelay (juce::RelativeTime::seconds (2.2), true);
        mainWindow->loadFiles (commandLine);
    }

    // Abrir una canción con MiDJ ya abierto la carga en la misma ventana
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (mainWindow != nullptr)
            mainWindow->loadFiles (commandLine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override  { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, DJColours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1280, 760, 8000, 5000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            // Para que los atajos de teclado funcionen desde el principio
            getContentComponent()->grabKeyboardFocus();
        }

        // Al activar la ventana (arranque, volver desde otra app) el teclado vuelve a MiDJ,
        // salvo que ya lo tenga un control (p. ej. el buscador)
        void activeWindowStatusChanged() override
        {
            DocumentWindow::activeWindowStatusChanged();

            if (isActiveWindow())
                if (auto* content = getContentComponent())
                    if (! content->hasKeyboardFocus (true))
                        content->grabKeyboardFocus();
        }

        void loadFiles (const juce::String& commandLine)
        {
            if (auto* main = dynamic_cast<MainComponent*> (getContentComponent()))
                main->loadFiles (commandLine);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    DJLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (CesarDJApplication)
