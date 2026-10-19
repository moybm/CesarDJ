#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

// Manual de ayuda integrado (F1 / botón AYUDA) y avisos importantes con "No volver a mostrar".
namespace Help
{
    struct Section
    {
        const char* id;
        const char* title;
        const char* text;           // UTF-8; "## " subtítulo, "- " viñeta, "! " nota importante
    };
    const std::vector<Section>& sections();

    // Lo llama la ventana principal al arrancar y al cerrar
    void install (juce::PropertiesFile& settings);
    void shutdown();

    // Abre el manual (en un apartado concreto si se indica su id)
    void open (const juce::String& sectionId = {});

    // Aviso importante. Si el usuario marcó "No volver a mostrar", no aparece y se llama directamente a "then".
    // El botón "Ver en el manual" abre el apartado sectionId.
    void notice (const juce::String& noticeId, const juce::String& title, const juce::String& message,
                 const juce::String& sectionId, std::function<void()> then = {});

    bool isNoticeHidden (const juce::String& noticeId);
    void resetNotices();
}
