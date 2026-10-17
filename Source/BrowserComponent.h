#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "Library.h"
#include "LibraryTree.h"

// Navegador de la biblioteca:
//  [árbol: Todas / Recientes / Historial / Listas / Carpetas] [buscador + tabla de canciones] [info + carátula]
//  - Arrastrar canciones a un deck para cargarlas o a una lista para añadirlas
//  - Doble clic / Enter: cargar en el primer deck libre. Clic derecho: menú
//  - Soltar carpetas o archivos desde el Explorador de Windows: se añaden a la biblioteca
class BrowserComponent : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::TableListBoxModel,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    BrowserComponent (Library& library, juce::PropertiesFile& settings);
    ~BrowserComponent() override;

    std::function<void (const juce::File&, int deckIndex)> onLoadRequest;  // deckIndex -1 = primer deck libre
    std::function<juce::File (int deckIndex)> loadedFile;                  // para marcar lo cargado en A/B
    std::function<double()> referenceBpm;                                  // BPM del deck que suena (filtro)
    std::function<int()> referenceKey;                                     // tonalidad del deck que suena

    void markPlayedThisSession (const juce::File& file);
    void focusSearch()                          { searchBox.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override  { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    // TableListBoxModel
    int getNumRows() override                   { return (int) rows.size(); }
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int w, int h, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;
    void deleteKeyPressed (int lastRowSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void sortOrderChanged (int columnId, bool forwards) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& selectedRows) override;
    juce::String getCellTooltip (int row, int column) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setView (const LibraryView& view, const juce::String& title);
    void refreshRows();
    void showTrackMenu (int row);
    juce::Array<juce::int64> selectedTrackIds() const;
    juce::Array<juce::int64> selectedEntryIds() const;
    const LibraryTrack* selectedTrack() const;
    void updateInfo();

    Library& library;
    juce::PropertiesFile& settings;

    LibraryTree tree { library };
    juce::TextButton addFolderButton { "+ Carpeta" }, addListButton { "+ Lista" };

    juce::TextEditor searchBox;
    juce::TextButton bpmFilterButton { juce::String::fromUTF8 ("BPM ±6%") }, keyFilterButton { juce::String::fromUTF8 ("ARMÓNICO") }, analyseButton;
    juce::TableListBox table { "canciones", this };
    juce::TextButton emptyAddButton { juce::String::fromUTF8 ("Añadir carpeta de música...") };
    juce::Label emptyLabel;

    juce::TextButton loadA { "Cargar en A" }, loadB { "Cargar en B" };

    LibraryView view;
    juce::String viewTitle = "Todas las canciones";
    std::vector<LibraryTrack> rows;
    juce::String sortColumn;
    bool sortAscending = true;
    std::set<juce::String> playedThisSession;

    juce::Image cover;
    juce::int64 coverTrackId = -1;
    juce::Rectangle<int> titleArea, statusArea, infoArea, ratingArea;
    juce::String statusText;
    bool refreshPending = false;
    juce::uint32 refreshDueTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserComponent)
};
