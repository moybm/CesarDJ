#include "BrowserComponent.h"
#include "DJLookAndFeel.h"
#include "TagReader.h"

using namespace DJColours;

namespace
{
    enum Columns { colIndex = 1, colTitle, colArtist, colBpm, colKey, colDuration, colGenre, colAlbum, colYear,
                   colRating, colPlays, colAdded };

    juce::String sortKeyFor (int column)
    {
        switch (column)
        {
            case colTitle:    return "title";
            case colArtist:   return "artist";
            case colBpm:      return "bpm";
            case colKey:      return "key";
            case colDuration: return "duration";
            case colGenre:    return "genre";
            case colAlbum:    return "album";
            case colYear:     return "year";
            case colRating:   return "rating";
            case colPlays:    return "plays";
            case colAdded:    return "added";
            default:          return {};
        }
    }

    juce::String formatDuration (double seconds)
    {
        if (seconds <= 0.0)
            return {};
        const int s = juce::roundToInt (seconds);
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    juce::String formatDate (juce::int64 unixSeconds, bool withTime)
    {
        if (unixSeconds <= 0)
            return {};
        return juce::Time (unixSeconds * 1000).formatted (withTime ? "%d/%m/%Y %H:%M" : "%d/%m/%Y");
    }

    void drawStars (juce::Graphics& g, juce::Rectangle<float> r, int rating)
    {
        const float size = juce::jmin (r.getHeight(), r.getWidth() / 5.0f);
        for (int i = 0; i < 5; ++i)
        {
            juce::Path star;
            star.addStar ({ r.getX() + size * (i + 0.5f), r.getCentreY() }, 5, size * 0.2f, size * 0.46f);
            g.setColour (i < rating ? juce::Colour (0xffe8c33a) : juce::Colours::white.withAlpha (0.12f));
            g.fillPath (star);
        }
    }
}

//==============================================================================
BrowserComponent::BrowserComponent (Library& lib, juce::PropertiesFile& props)
    : library (lib), settings (props)
{
    addAndMakeVisible (tree);
    tree.onViewSelected = [this] (const LibraryView& v, const juce::String& title) { setView (v, title); };

    addFolderButton.onClick = [this] { tree.askAddFolder(); };
    addListButton.onClick   = [this] { tree.askNewPlaylist(); };
    addFolderButton.setTooltip (juce::String::fromUTF8 ("Añade una carpeta a la biblioteca (se escanea con subcarpetas)"));
    addListButton.setTooltip ("Crea una lista nueva");
    addAndMakeVisible (addFolderButton);
    addAndMakeVisible (addListButton);

    // ---- Buscador y herramientas
    searchBox.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Buscar título, artista, álbum o género..."), textDim);
    searchBox.setFont (juce::FontOptions (14.0f));
    searchBox.onTextChange = [this] { refreshPending = true; refreshDueTime = juce::Time::getMillisecondCounter() + 150; };
    searchBox.onEscapeKey  = [this] { searchBox.clear(); refreshRows(); };
    searchBox.onReturnKey  = [this] { table.grabKeyboardFocus(); if (! rows.empty()) table.selectRow (0); };
    addAndMakeVisible (searchBox);

    bpmFilterButton.setClickingTogglesState (true);
    bpmFilterButton.setColour (juce::TextButton::buttonOnColourId, deckA.darker (0.3f));
    bpmFilterButton.setTooltip (juce::String::fromUTF8 ("Muestra solo canciones con un BPM parecido (±6%) al del deck que está sonando"));
    bpmFilterButton.onClick = [this] { refreshRows(); };
    addAndMakeVisible (bpmFilterButton);

    analyseButton.onClick = [this]
    {
        if (library.isBusy())
            library.cancelAnalysis();
        else
            library.analysePending();
    };
    analyseButton.setTooltip ("Calcula en segundo plano el BPM y el beatgrid de las canciones pendientes");
    addAndMakeVisible (analyseButton);

    // ---- Tabla
    auto& header = table.getHeader();
    const int columnFlags = juce::TableHeaderComponent::defaultFlags;
    header.addColumn ("#",          colIndex,    40,  30, 60, juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable);
    header.addColumn (juce::String::fromUTF8 ("Título"),     colTitle,    300, 80, -1, columnFlags);
    header.addColumn ("Artista",    colArtist,   200, 60, -1, columnFlags);
    header.addColumn ("BPM",        colBpm,      60,  45, 90, columnFlags);
    header.addColumn ("Tono",       colKey,      50,  40, 80, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Duración"),   colDuration, 70,  50, 90, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Género"),     colGenre,    120, 50, -1, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Álbum"),      colAlbum,    170, 50, -1, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Año"),        colYear,     50,  40, 70, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Valoración"), colRating,   90,  80, 110, columnFlags);
    header.addColumn ("Veces",      colPlays,    55,  40, 80, columnFlags);
    header.addColumn (juce::String::fromUTF8 ("Añadida"),    colAdded,    90,  60, 140, columnFlags);

    const auto savedColumns = settings.getValue ("browserColumns");
    if (savedColumns.isNotEmpty())
        header.restoreFromString (savedColumns);

    table.setRowHeight (25);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::outlineColourId, border);
    table.setOutlineThickness (1);
    table.getViewport()->setScrollBarThickness (10);
    addAndMakeVisible (table);

    emptyLabel.setText (juce::String::fromUTF8 ("Tu biblioteca está vacía.\nAnade la carpeta donde tienes tu música: se escanea en segundo plano ")
                        + juce::String::fromUTF8 ("y después puedes analizar el BPM de todas las canciones."), juce::dontSendNotification);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, textDim);
    emptyLabel.setFont (juce::FontOptions (14.0f));
    emptyLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (emptyLabel);

    emptyAddButton.setColour (juce::TextButton::buttonColourId, deckA.darker (0.3f));
    emptyAddButton.onClick = [this] { tree.askAddFolder(); };
    addChildComponent (emptyAddButton);

    // ---- Panel de información
    loadA.setColour (juce::TextButton::buttonColourId, deckA.darker (0.4f));
    loadB.setColour (juce::TextButton::buttonColourId, deckB.darker (0.4f));
    loadA.onClick = [this] { if (auto* t = selectedTrack()) if (onLoadRequest) onLoadRequest (t->file, 0); };
    loadB.onClick = [this] { if (auto* t = selectedTrack()) if (onLoadRequest) onLoadRequest (t->file, 1); };
    addAndMakeVisible (loadA);
    addAndMakeVisible (loadB);

    for (auto* c : { (juce::Component*) &addFolderButton, (juce::Component*) &addListButton, (juce::Component*) &bpmFilterButton,
                     (juce::Component*) &analyseButton, (juce::Component*) &loadA, (juce::Component*) &loadB,
                     (juce::Component*) &emptyAddButton })
        c->setWantsKeyboardFocus (false);

    library.addChangeListener (this);
    refreshRows();
    startTimerHz (5);
}

BrowserComponent::~BrowserComponent()
{
    settings.setValue ("browserColumns", table.getHeader().toString());
    library.removeChangeListener (this);
}

//==============================================================================
void BrowserComponent::setView (const LibraryView& newView, const juce::String& title)
{
    view = newView;
    viewTitle = title;
    table.deselectAllRows();
    refreshRows();
    table.scrollToEnsureRowIsOnscreen (0);
}

void BrowserComponent::refreshRows()
{
    refreshPending = false;

    // Recordar la selección para restaurarla
    std::set<juce::int64> selectedKeys;
    const bool byEntry = view.type == LibraryView::Type::playlist || view.type == LibraryView::Type::history;
    const auto sel = table.getSelectedRows();
    for (int i = 0; i < sel.size(); ++i)
        if (sel[i] < (int) rows.size())
            selectedKeys.insert (byEntry ? rows[(size_t) sel[i]].entryId : rows[(size_t) sel[i]].id);

    double bpmMin = 0.0, bpmMax = 0.0;
    if (bpmFilterButton.getToggleState() && referenceBpm != nullptr)
    {
        const double ref = referenceBpm();
        if (ref > 0.0)
        {
            bpmMin = ref * 0.94;
            bpmMax = ref * 1.06;
        }
    }

    rows = library.query (view, searchBox.getText(), sortColumn, sortAscending, bpmMin, bpmMax);
    table.updateContent();

    juce::SparseSet<int> newSelection;
    for (int i = 0; i < (int) rows.size(); ++i)
        if (selectedKeys.count (byEntry ? rows[(size_t) i].entryId : rows[(size_t) i].id) > 0)
            newSelection.addRange ({ i, i + 1 });
    table.setSelectedRows (newSelection, juce::dontSendNotification);

    const bool libraryEmpty = rows.empty() && view.type == LibraryView::Type::all
                              && searchBox.isEmpty() && library.countTracks() == 0;
    emptyLabel.setVisible (libraryEmpty);
    emptyAddButton.setVisible (libraryEmpty);

    table.repaint();
    updateInfo();
    repaint (titleArea);
}

void BrowserComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Durante un escaneo llegan muchos avisos: agruparlos
    tree.refresh();

    // Si la lista abierta se ha renombrado, actualizar el título
    if (view.type == LibraryView::Type::playlist)
        for (auto& p : library.getPlaylists())
            if (p.id == view.playlistId && p.name != viewTitle)
            {
                viewTitle = p.name;
                repaint (titleArea);
            }

    refreshPending = true;
    refreshDueTime = juce::Time::getMillisecondCounter() + 300;
}

void BrowserComponent::timerCallback()
{
    if (refreshPending && juce::Time::getMillisecondCounter() >= refreshDueTime)
        refreshRows();

    const auto status = library.getStatusText();
    if (status != statusText)
    {
        statusText = status;
        repaint (statusArea);
    }

    const int pending = library.countPending();
    analyseButton.setButtonText (library.isBusy() ? juce::String ("Detener")
                                 : pending > 0 ? "Analizar BPM (" + juce::String (pending) + ")"
                                               : juce::String (juce::String::fromUTF8 ("BPM al día")));
    analyseButton.setEnabled (library.isBusy() || pending > 0);

    // Las marcas de "cargada en A/B" cambian sin avisar: repintar la tabla de vez en cuando
    table.repaint();
}

void BrowserComponent::markPlayedThisSession (const juce::File& file)
{
    playedThisSession.insert (file.getFullPathName().toLowerCase());
    table.repaint();
}

//==============================================================================
void BrowserComponent::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (selected)
        g.fillAll (deckA.withAlpha (0.32f));
    else if (row % 2 == 1)
        g.fillAll (juce::Colours::white.withAlpha (0.025f));

    if (row < (int) rows.size() && loadedFile != nullptr)
        for (int d = 0; d < 2; ++d)
            if (loadedFile (d) == rows[(size_t) row].file)
            {
                g.setColour (forDeck (d));
                g.fillRect (d * 4, 0, 3, h);
            }

    juce::ignoreUnused (w);
}

void BrowserComponent::paintCell (juce::Graphics& g, int row, int column, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    const auto& t = rows[(size_t) row];
    const bool played = playedThisSession.count (t.file.getFullPathName().toLowerCase()) > 0;
    const auto mainColour = selected ? juce::Colours::white : (played ? textDim : text);
    juce::String s;
    auto just = juce::Justification::centredLeft;

    switch (column)
    {
        case colIndex:
            s = juce::String (row + 1);
            just = juce::Justification::centredRight;
            if (played)
            {
                g.setColour (play);
                g.fillEllipse (10.0f, h * 0.5f - 3.0f, 6.0f, 6.0f);
            }
            break;
        case colTitle:    s = t.title; break;
        case colArtist:   s = t.artist; break;
        case colBpm:
            if (t.bpm > 0.0)
                s = t.analysed ? juce::String (t.bpm, 1) : "~" + juce::String (juce::roundToInt (t.bpm));
            just = juce::Justification::centredRight;
            break;
        case colKey:      s = t.key; break;
        case colDuration: s = formatDuration (t.duration); just = juce::Justification::centredRight; break;
        case colGenre:    s = t.genre; break;
        case colAlbum:    s = t.album; break;
        case colYear:     s = t.year; break;
        case colRating:
            drawStars (g, juce::Rectangle<float> (4.0f, 0.0f, (float) w - 8.0f, (float) h).reduced (0.0f, 5.0f), t.rating);
            return;
        case colPlays:    s = t.playCount > 0 ? juce::String (t.playCount) : juce::String(); just = juce::Justification::centredRight; break;
        case colAdded:
            s = view.type == LibraryView::Type::history ? formatDate (t.playedAt, true) : formatDate (t.dateAdded, false);
            break;
        default: break;
    }

    const bool dim = column != colTitle && column != colArtist && column != colBpm;
    g.setColour (column == colBpm && ! t.analysed ? textDim : (dim && ! selected ? textDim : mainColour));
    g.setFont (juce::FontOptions (column == colTitle ? 14.0f : 13.0f, column == colBpm ? juce::Font::bold : juce::Font::plain));
    g.drawText (s, 6, 0, w - 10, h, just, true);
}

juce::String BrowserComponent::getCellTooltip (int row, int column)
{
    if (row >= 0 && row < (int) rows.size() && (column == colTitle || column == colArtist))
        return rows[(size_t) row].file.getFullPathName();
    return {};
}

void BrowserComponent::cellClicked (int row, int column, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    if (e.mods.isPopupMenu())
    {
        showTrackMenu (row);
        return;
    }

    if (column == colRating)
    {
        const int w = table.getHeader().getColumnWidth (colRating);
        const int clicked = juce::jlimit (0, 5, (int) ((e.x - 4) / ((w - 8) / 5.0f)) + 1);
        const auto& t = rows[(size_t) row];
        library.setRating (t.id, clicked == t.rating ? 0 : clicked);
    }
}

void BrowserComponent::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size() && onLoadRequest != nullptr)
        onLoadRequest (rows[(size_t) row].file, -1);
}

void BrowserComponent::returnKeyPressed (int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < (int) rows.size() && onLoadRequest != nullptr)
        onLoadRequest (rows[(size_t) lastRowSelected].file, -1);
}

void BrowserComponent::deleteKeyPressed (int)
{
    if (view.type == LibraryView::Type::playlist)
    {
        library.removeFromPlaylist (selectedEntryIds());
        return;
    }

    if (view.type == LibraryView::Type::history)
        return;

    const auto ids = selectedTrackIds();
    if (ids.isEmpty())
        return;

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Quitar de la biblioteca",
        "Quitar " + juce::String (ids.size()) + juce::String::fromUTF8 (" canción(es) de la biblioteca?\nLos archivos NO se borran del disco."),
        "Quitar", "Cancelar", nullptr,
        juce::ModalCallbackFunction::create ([this, ids] (int r) { if (r == 1) library.removeTracks (ids); }));
}

void BrowserComponent::selectedRowsChanged (int)
{
    updateInfo();
}

void BrowserComponent::sortOrderChanged (int columnId, bool forwards)
{
    sortColumn = sortKeyFor (columnId);
    sortAscending = forwards;
    refreshRows();
}

juce::var BrowserComponent::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.isEmpty() || selectedRows[0] >= (int) rows.size())
        return {};

    juce::StringArray ids;
    for (int i = 0; i < selectedRows.size(); ++i)
        if (selectedRows[i] < (int) rows.size())
            ids.add (juce::String (rows[(size_t) selectedRows[i]].id));

    // El deck usa el primer archivo; las listas usan todos los ids
    return "file:" + rows[(size_t) selectedRows[0]].file.getFullPathName() + "|ids:" + ids.joinIntoString (",");
}

juce::Array<juce::int64> BrowserComponent::selectedTrackIds() const
{
    juce::Array<juce::int64> ids;
    const auto sel = table.getSelectedRows();
    for (int i = 0; i < sel.size(); ++i)
        if (sel[i] < (int) rows.size())
            ids.add (rows[(size_t) sel[i]].id);
    return ids;
}

juce::Array<juce::int64> BrowserComponent::selectedEntryIds() const
{
    juce::Array<juce::int64> ids;
    const auto sel = table.getSelectedRows();
    for (int i = 0; i < sel.size(); ++i)
        if (sel[i] < (int) rows.size())
            ids.add (rows[(size_t) sel[i]].entryId);
    return ids;
}

const LibraryTrack* BrowserComponent::selectedTrack() const
{
    const int row = table.getLastRowSelected();
    return row >= 0 && row < (int) rows.size() ? &rows[(size_t) row] : nullptr;
}

void BrowserComponent::showTrackMenu (int row)
{
    if (! table.isRowSelected (row))
        table.selectRow (row);

    const auto ids = selectedTrackIds();
    const auto entries = selectedEntryIds();
    const auto file = rows[(size_t) row].file;

    juce::PopupMenu m;
    m.addItem ("Cargar en deck A", [this, file] { if (onLoadRequest) onLoadRequest (file, 0); });
    m.addItem ("Cargar en deck B", [this, file] { if (onLoadRequest) onLoadRequest (file, 1); });
    m.addSeparator();

    juce::PopupMenu lists;
    for (auto& p : library.getPlaylists())
    {
        if (view.type == LibraryView::Type::playlist && p.id == view.playlistId)
            continue;
        const auto id = p.id;
        lists.addItem (p.name, [this, id, ids] { library.addToPlaylist (id, ids); });
    }
    if (lists.getNumItems() > 0)
        lists.addSeparator();
    lists.addItem (juce::String::fromUTF8 ("Nueva lista con la selección..."), [this, ids] { tree.askNewPlaylist (ids); });
    m.addSubMenu (juce::String::fromUTF8 ("Añadir a lista"), lists);

    if (view.type == LibraryView::Type::playlist)
        m.addItem ("Quitar de esta lista", [this, entries] { library.removeFromPlaylist (entries); });

    m.addItem ("Volver a analizar BPM", [this, ids] { library.requeueAnalysis (ids); library.analysePending(); });
    m.addItem ("Mostrar en el Explorador", [file] { file.revealToUser(); });

    if (view.type != LibraryView::Type::history)
    {
        m.addSeparator();
        m.addItem ("Quitar de la biblioteca...", [this] { deleteKeyPressed (0); });
    }

    m.showMenuAsync (juce::PopupMenu::Options());
}

//==============================================================================
void BrowserComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        const juce::File f (path);
        if (f.isDirectory())
            library.addFolder (f);
        else
            library.addFileAsync (f);
    }
}

void BrowserComponent::mouseDown (const juce::MouseEvent& e)
{
    if (ratingArea.contains (e.getPosition()))
        if (auto* t = selectedTrack())
        {
            const int clicked = juce::jlimit (0, 5, (e.x - ratingArea.getX()) * 5 / juce::jmax (1, ratingArea.getWidth()) + 1);
            library.setRating (t->id, clicked == t->rating ? 0 : clicked);
        }
}

void BrowserComponent::updateInfo()
{
    const auto* t = selectedTrack();
    const juce::int64 id = t != nullptr ? t->id : -1;

    if (id != coverTrackId)
    {
        coverTrackId = id;
        cover = t != nullptr ? TagReader::readCoverArt (t->file) : juce::Image();
    }

    loadA.setEnabled (t != nullptr);
    loadB.setEnabled (t != nullptr);
    repaint (infoArea);
}

//==============================================================================
void BrowserComponent::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    // Título de la vista + número de canciones
    g.setColour (text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (viewTitle, titleArea, juce::Justification::centredLeft, true);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String ((int) rows.size()) + (rows.size() == 1 ? juce::String::fromUTF8 (" canción") : " canciones"),
                titleArea, juce::Justification::centredRight);

    // Estado del trabajo en segundo plano
    if (statusText.isNotEmpty())
    {
        g.setColour (cue);
        g.drawText (statusText, statusArea, juce::Justification::centredRight, true);
    }

    // ---- Panel de información
    auto info = infoArea;
    g.setColour (inset);
    g.fillRect (info);
    info = info.reduced (12, 10);
    info.removeFromBottom (40);   // botones

    const auto* t = selectedTrack();

    const int coverSize = juce::jmin (info.getWidth(), juce::jmax (60, info.getHeight() - 210), 190);
    auto coverArea = info.removeFromTop (coverSize).withSizeKeepingCentre (coverSize, coverSize);
    if (cover.isValid())
    {
        g.drawImage (cover, coverArea.toFloat(), juce::RectanglePlacement::centred);
    }
    else
    {
        g.setColour (panelLight);
        g.fillRoundedRectangle (coverArea.toFloat(), 4.0f);
        g.setColour (textDim.withAlpha (0.5f));
        const auto c = coverArea.getCentre().toFloat();
        const float s = coverSize / 60.0f;
        g.fillEllipse (c.x - 9 * s, c.y + 4 * s, 10 * s, 8 * s);
        g.fillRect (c.x - 0.5f * s, c.y - 14 * s, 2.0f * s, 22 * s);
        g.fillRect (c.x - 0.5f * s, c.y - 14 * s, 10.0f * s, 3.0f * s);
    }
    info.removeFromTop (8);

    if (t == nullptr)
    {
        ratingArea = {};
        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (juce::String::fromUTF8 ("Selecciona una canción.\nDoble clic o arrástrala a un deck para cargarla; ")
                          + juce::String::fromUTF8 ("arrástrala a una lista para añadirla."), info.removeFromTop (70), juce::Justification::centredTop, 5);
        return;
    }

    g.setColour (text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (t->title, info.removeFromTop (20), juce::Justification::centredLeft, true);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (t->artist.isEmpty() ? juce::String ("Artista desconocido") : t->artist, info.removeFromTop (18),
                juce::Justification::centredLeft, true);

    ratingArea = info.removeFromTop (22).removeFromLeft (100);
    drawStars (g, ratingArea.toFloat().reduced (0.0f, 2.0f), t->rating);
    info.removeFromTop (4);

    auto line = [&] (const juce::String& label, const juce::String& value)
    {
        if (value.isEmpty() || info.getHeight() < 16)
            return;
        auto r = info.removeFromTop (17);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (label, r.removeFromLeft (82), juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (value, r, juce::Justification::centredLeft, true);
    };

    line ("BPM:", t->bpm > 0.0 ? juce::String (t->bpm, 1) + (t->analysed ? "" : " (etiqueta)") : juce::String ("sin analizar"));
    line ("Tono:", t->key);
    line (juce::String::fromUTF8 ("Duración:"), formatDuration (t->duration));
    line (juce::String::fromUTF8 ("Álbum:"), t->album);
    line (juce::String::fromUTF8 ("Género:"), t->genre);
    line (juce::String::fromUTF8 ("Año:"), t->year);
    line ("Reproducida:", t->playCount > 0 ? juce::String (t->playCount) + " veces" : juce::String ("nunca"));
    line (juce::String::fromUTF8 ("Última vez:"), formatDate (t->lastPlayed, true));
    line (juce::String::fromUTF8 ("Añadida:"), formatDate (t->dateAdded, false));
    line ("Comentario:", t->comment);
}

void BrowserComponent::resized()
{
    auto area = getLocalBounds().reduced (6);

    // Izquierda: árbol + botones
    auto left = area.removeFromLeft (juce::jmin (250, area.getWidth() / 6 + 40));
    auto leftButtons = left.removeFromBottom (30);
    addFolderButton.setBounds (leftButtons.removeFromLeft (leftButtons.getWidth() / 2).reduced (0, 0).withTrimmedRight (2));
    addListButton.setBounds (leftButtons.withTrimmedLeft (2));
    left.removeFromBottom (6);
    tree.setBounds (left);
    area.removeFromLeft (6);

    // Derecha: info
    infoArea = area.removeFromRight (juce::jmin (290, area.getWidth() / 4));
    {
        auto buttons = infoArea.withTrimmedTop (infoArea.getHeight() - 40).reduced (10, 6);
        loadA.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).withTrimmedRight (3));
        loadB.setBounds (buttons.withTrimmedLeft (3));
    }
    area.removeFromRight (6);

    // Centro: barra superior + tabla
    auto bar = area.removeFromTop (30);
    analyseButton.setBounds (bar.removeFromRight (150));
    bar.removeFromRight (6);
    bpmFilterButton.setBounds (bar.removeFromRight (86));
    bar.removeFromRight (10);
    statusArea = bar.removeFromRight (juce::jmin (230, bar.getWidth() / 4));
    bar.removeFromRight (10);
    titleArea = bar.removeFromLeft (juce::jmin (260, bar.getWidth() / 3));
    bar.removeFromLeft (10);
    searchBox.setBounds (bar);

    area.removeFromTop (6);
    table.setBounds (area);

    auto hint = area.withSizeKeepingCentre (juce::jmin (520, area.getWidth() - 20), 120);
    emptyLabel.setBounds (hint.removeFromTop (70));
    emptyAddButton.setBounds (hint.withSizeKeepingCentre (240, 34));
}
