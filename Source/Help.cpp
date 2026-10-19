#include "Help.h"
#include "DJLookAndFeel.h"
#include "Branding.h"

using namespace DJColours;

namespace
{
    juce::PropertiesFile* props = nullptr;

    juce::String utf8 (const char* s)  { return juce::String::fromUTF8 (s); }

    int findSection (const juce::String& id)
    {
        const auto& list = Help::sections();
        for (int i = 0; i < (int) list.size(); ++i)
            if (id == list[(size_t) i].id)
                return i;
        return -1;
    }

    //==========================================================================
    class HelpComponent : public juce::Component,
                          private juce::ListBoxModel
    {
    public:
        HelpComponent()
        {
            search.setTextToShowWhenEmpty (utf8 ("Buscar en el manual..."), textDim);
            search.setFont (juce::FontOptions (14.0f));
            search.onTextChange = [this] { filter(); };
            search.onEscapeKey = [this] { search.clear(); filter(); };
            addAndMakeVisible (search);

            index.setModel (this);
            index.setRowHeight (30);
            index.setColour (juce::ListBox::backgroundColourId, inset);
            addAndMakeVisible (index);

            content.setMultiLine (true, true);
            content.setReadOnly (true);
            content.setCaretVisible (false);
            content.setScrollbarsShown (true);
            content.setColour (juce::TextEditor::backgroundColourId, panel);
            content.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            content.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            content.setBorder (juce::BorderSize<int> (14, 22, 14, 22));
            addAndMakeVisible (content);

            resetButton.setButtonText ("Volver a mostrar los avisos");
            resetButton.setTooltip (utf8 ("Vuelven a aparecer los avisos en los que marcaste \"No volver a mostrar\""));
            resetButton.onClick = []
            {
                Help::resetNotices();
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Avisos",
                                                        utf8 ("Los avisos volverán a aparecer."));
            };
            addAndMakeVisible (resetButton);

            footer.setText (Branding::appName + " " + JUCE_APPLICATION_VERSION_STRING
                            + utf8 ("   ·   F1 abre este manual desde cualquier sitio"), juce::dontSendNotification);
            footer.setColour (juce::Label::textColourId, textDim);
            footer.setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (footer);

            filter();
        }

        void showSection (const juce::String& id)
        {
            if (search.getText().isNotEmpty())
            {
                search.clear();
                filter();
            }
            const int s = findSection (id);
            for (int row = 0; row < (int) visible.size(); ++row)
                if (visible[(size_t) row] == s)
                {
                    index.selectRow (row);
                    return;
                }
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto bottom = r.removeFromBottom (40).reduced (12, 6);
            resetButton.setBounds (bottom.removeFromRight (230));
            footer.setBounds (bottom);

            auto left = r.removeFromLeft (250);
            search.setBounds (left.removeFromTop (40).reduced (8, 6));
            index.setBounds (left);
            content.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (inset);
            g.setColour (border);
            g.drawVerticalLine (250, 0.0f, (float) getHeight() - 40.0f);
            g.drawHorizontalLine (getHeight() - 40, 0.0f, (float) getWidth());
        }

    private:
        int getNumRows() override  { return (int) visible.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, (int) visible.size()))
                return;
            if (selected)
            {
                g.fillAll (deckA.withAlpha (0.3f));
                g.setColour (deckA);
                g.fillRect (0, 0, 3, h);
            }
            g.setColour (selected ? juce::Colours::white : text);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (utf8 (Help::sections()[(size_t) visible[(size_t) row]].title), 14, 0, w - 20, h,
                        juce::Justification::centredLeft, true);
        }

        void selectedRowsChanged (int row) override
        {
            if (juce::isPositiveAndBelow (row, (int) visible.size()))
                render (visible[(size_t) row]);
        }

        void filter()
        {
            const auto words = juce::StringArray::fromTokens (search.getText().trim(), " ", "");
            visible.clear();
            const auto& list = Help::sections();
            for (int i = 0; i < (int) list.size(); ++i)
            {
                const auto all = utf8 (list[(size_t) i].title) + " " + utf8 (list[(size_t) i].text);
                bool ok = true;
                for (auto& w : words)
                    ok = ok && all.containsIgnoreCase (w);
                if (ok)
                    visible.push_back (i);
            }
            index.updateContent();
            index.repaint();

            if (! visible.empty())
                index.selectRow (0);
            else
            {
                content.clear();
                content.setFont (juce::FontOptions (14.0f));
                content.setColour (juce::TextEditor::textColourId, textDim);
                content.insertTextAtCaret (utf8 ("No hay nada en el manual con esas palabras."));
            }
        }

        void render (int sectionIndex)
        {
            const auto& s = Help::sections()[(size_t) sectionIndex];
            content.clear();

            auto put = [this] (const juce::String& t, float size, bool bold, juce::Colour c)
            {
                content.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
                content.setColour (juce::TextEditor::textColourId, c);
                content.insertTextAtCaret (t);
            };

            put ("\n", 10.0f, false, text);       // margen: al volver arriba el título no queda cortado
            put (utf8 (s.title) + "\n\n", 22.0f, true, juce::Colours::white);

            for (auto line : juce::StringArray::fromLines (utf8 (s.text)))
            {
                if (line.startsWith ("## "))
                    put ("\n" + line.substring (3) + "\n", 16.0f, true, deckA.brighter (0.3f));
                else if (line.startsWith ("- "))
                    put (juce::String::fromUTF8 ("    \xe2\x80\xa2  ") + line.substring (2) + "\n", 14.5f, false, text);
                else if (line.startsWith ("! "))
                    put ("\n" + juce::String::fromUTF8 ("\xe2\x9a\xa0  ") + line.substring (2) + "\n", 14.5f, true, cue);
                else if (line.isEmpty())
                    put ("\n", 8.0f, false, text);
                else
                    put (line + "\n", 14.5f, false, text);
            }
            content.setCaretPosition (0);
        }

        juce::TextEditor search, content;
        juce::ListBox index;
        juce::TextButton resetButton;
        juce::Label footer;
        std::vector<int> visible;
    };

    class HelpWindow : public juce::DocumentWindow
    {
    public:
        HelpWindow()
            : juce::DocumentWindow (utf8 ("Ayuda de CesarDJ"), inset, juce::DocumentWindow::allButtons)
        {
            comp = new HelpComponent();
            setContentOwned (comp, false);
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            setResizeLimits (720, 480, 2400, 1600);
            centreWithSize (1000, 700);
            setVisible (true);
        }

        void closeButtonPressed() override   { setVisible (false); }

        HelpComponent* comp = nullptr;
    };

    std::unique_ptr<HelpWindow> window;

    //==========================================================================
    class NoticeComponent : public juce::Component
    {
    public:
        NoticeComponent (const juce::String& noticeId, const juce::String& message, const juce::String& sectionId,
                         std::function<void()> afterwards)
            : id (noticeId), section (sectionId), then (std::move (afterwards))
        {
            body.setText (message, juce::dontSendNotification);
            body.setFont (juce::FontOptions (14.5f));
            body.setColour (juce::Label::textColourId, text);
            body.setJustificationType (juce::Justification::topLeft);
            body.setMinimumHorizontalScale (1.0f);
            addAndMakeVisible (body);

            dontShow.setButtonText ("No volver a mostrar este aviso");
            dontShow.setColour (juce::ToggleButton::textColourId, textDim);
            dontShow.setColour (juce::ToggleButton::tickColourId, play);
            addAndMakeVisible (dontShow);

            manualButton.setButtonText ("Ver en el manual");
            manualButton.onClick = [this] { finish (true); };
            okButton.setButtonText ("Entendido");
            okButton.setColour (juce::TextButton::buttonColourId, deckA.darker (0.3f));
            okButton.onClick = [this] { finish (false); };
            manualButton.setVisible (section.isNotEmpty());
            addAndMakeVisible (manualButton);
            addAndMakeVisible (okButton);

            // Alto según el texto
            juce::AttributedString as;
            as.append (message, juce::FontOptions (14.5f), text);
            as.setWordWrap (juce::AttributedString::byWord);
            juce::TextLayout layout;
            layout.createLayout (as, (float) (width - 48));
            textHeight = juce::jlimit (40, 520, (int) std::ceil (layout.getHeight()) + 10);
            setSize (width, textHeight + 24 + 30 + 16 + 34 + 20);
        }

        ~NoticeComponent() override
        {
            // También si se cierra con la X o Esc: se respeta la casilla y sigue lo que venía después
            if (dontShow.getToggleState() && props != nullptr)
            {
                props->setValue ("noticeHidden_" + id, true);
                props->saveIfNeeded();
            }
            if (then != nullptr)
                juce::MessageManager::callAsync (then);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (24, 20);
            body.setBounds (r.removeFromTop (textHeight));
            r.removeFromTop (12);
            dontShow.setBounds (r.removeFromTop (30));
            r.removeFromTop (12);
            auto buttons = r.removeFromTop (34);
            okButton.setBounds (buttons.removeFromRight (130));
            buttons.removeFromRight (10);
            if (manualButton.isVisible())
                manualButton.setBounds (buttons.removeFromRight (160));
        }

        void paint (juce::Graphics& g) override     { g.fillAll (panel); }

    private:
        void finish (bool openManual)
        {
            if (openManual)
            {
                const auto s = section;
                juce::MessageManager::callAsync ([s] { Help::open (s); });
            }
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (openManual ? 2 : 1);
        }

        static constexpr int width = 560;
        juce::String id, section;
        std::function<void()> then;
        juce::Label body;
        juce::ToggleButton dontShow;
        juce::TextButton manualButton, okButton;
        int textHeight = 60;
    };
}

//==============================================================================
void Help::install (juce::PropertiesFile& settings)   { props = &settings; }

void Help::shutdown()
{
    window = nullptr;
    props = nullptr;
}

void Help::open (const juce::String& sectionId)
{
    if (window == nullptr)
        window = std::make_unique<HelpWindow>();

    window->setVisible (true);
    if (window->isMinimised())
        window->setMinimised (false);
    window->toFront (true);
    if (sectionId.isNotEmpty())
        window->comp->showSection (sectionId);
}

bool Help::isNoticeHidden (const juce::String& noticeId)
{
    return props != nullptr && props->getBoolValue ("noticeHidden_" + noticeId, false);
}

void Help::resetNotices()
{
    if (props == nullptr)
        return;
    for (auto& key : props->getAllProperties().getAllKeys())
        if (key.startsWith ("noticeHidden_"))
            props->removeValue (key);
    props->saveIfNeeded();
}

void Help::notice (const juce::String& noticeId, const juce::String& title, const juce::String& message,
                   const juce::String& sectionId, std::function<void()> then)
{
    if (isNoticeHidden (noticeId))
    {
        if (then != nullptr)
            then();
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new NoticeComponent (noticeId, message, sectionId, std::move (then)));
    options.dialogTitle = title;
    options.dialogBackgroundColour = panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
