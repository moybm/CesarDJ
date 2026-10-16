#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Library.h"

// Árbol de la biblioteca: Todas · Recientes · Historial · Listas · Carpetas
// Clic derecho para crear / renombrar / borrar; se pueden soltar canciones sobre una lista.
class LibraryTree : public juce::TreeView
{
public:
    explicit LibraryTree (Library& library);
    ~LibraryTree() override;

    void refresh();                                         // vuelve a leer listas y carpetas
    void selectView (const LibraryView& view);

    std::function<void (const LibraryView&, const juce::String& title)> onViewSelected;

    void askNewPlaylist (const juce::Array<juce::int64>& tracksToAdd = {});
    void askAddFolder();

    Library& library;

private:
    friend class LibraryTreeItem;

    void itemChosen (const LibraryView& view, const juce::String& title);
    void askText (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onOk);

    std::unique_ptr<juce::TreeViewItem> root;
    std::unique_ptr<juce::AlertWindow> prompt;
    std::unique_ptr<juce::FileChooser> chooser;
    bool refreshing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryTree)
};
