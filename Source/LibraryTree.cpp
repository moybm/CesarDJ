#include "LibraryTree.h"
#include "DJLookAndFeel.h"

using namespace DJColours;

namespace
{
    enum class Kind { root, all, recent, history, automix, wanted, playlistsHeader, playlist, foldersHeader, folder,
                      spotifyHeader, spotifyLogin, spotifyUser, spotifyList };

    // Iconos dibujados (no dependen de las fuentes del sistema)
    void drawIcon (juce::Graphics& g, Kind kind, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        const auto cx = r.getCentreX(), cy = r.getCentreY();

        switch (kind)
        {
            case Kind::all:
            {
                g.fillEllipse (cx - 6.0f, cy + 1.0f, 6.0f, 5.0f);
                g.fillEllipse (cx + 1.0f, cy - 1.0f, 6.0f, 5.0f);
                g.fillRect (cx - 1.5f, cy - 7.0f, 1.5f, 10.0f);
                g.fillRect (cx + 5.5f, cy - 9.0f, 1.5f, 10.0f);
                g.fillRect (cx - 1.5f, cy - 9.0f, 8.5f, 2.0f);
                break;
            }
            case Kind::recent:
            case Kind::history:
            {
                g.drawEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.5f);
                g.fillRect (cx - 0.75f, cy - 5.0f, 1.5f, 5.5f);
                g.fillRect (cx - 0.75f, cy - 0.75f, kind == Kind::recent ? 4.5f : -4.5f, 1.5f);
                break;
            }
            case Kind::playlist:
            case Kind::playlistsHeader:
                for (int i = 0; i < 3; ++i)
                {
                    g.fillRect (cx - 7.0f, cy - 5.0f + i * 4.5f, 2.0f, 2.0f);
                    g.fillRect (cx - 3.5f, cy - 5.0f + i * 4.5f, 10.5f, 2.0f);
                }
                break;
            case Kind::folder:
            case Kind::foldersHeader:
            {
                g.fillRoundedRectangle (cx - 8.0f, cy - 6.0f, 7.0f, 4.0f, 1.0f);
                g.fillRoundedRectangle (cx - 8.0f, cy - 4.0f, 16.0f, 11.0f, 1.5f);
                break;
            }
            case Kind::automix:
            {
                // Dos flechas que se cruzan (mezcla automática)
                juce::Path p;
                p.startNewSubPath (cx - 8.0f, cy + 5.0f);
                p.cubicTo (cx - 2.0f, cy + 5.0f, cx + 2.0f, cy - 5.0f, cx + 8.0f, cy - 5.0f);
                p.startNewSubPath (cx - 8.0f, cy - 5.0f);
                p.cubicTo (cx - 2.0f, cy - 5.0f, cx + 2.0f, cy + 5.0f, cx + 8.0f, cy + 5.0f);
                g.strokePath (p, juce::PathStrokeType (1.8f));
                g.fillRect (cx + 6.0f, cy - 7.0f, 2.5f, 4.5f);
                g.fillRect (cx + 6.0f, cy + 2.5f, 2.5f, 4.5f);
                break;
            }
            case Kind::wanted:
            {
                // Lupa
                g.drawEllipse (cx - 7.0f, cy - 7.0f, 10.0f, 10.0f, 1.8f);
                juce::Path handle;
                handle.startNewSubPath (cx + 1.5f, cy + 1.5f);
                handle.lineTo (cx + 6.5f, cy + 6.5f);
                g.strokePath (handle, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
            case Kind::spotifyHeader:
            case Kind::spotifyLogin:
            case Kind::spotifyUser:
            {
                // Círculo con tres ondas (sin logotipos de marca)
                g.drawEllipse (cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 1.5f);
                for (int i = 0; i < 3; ++i)
                {
                    juce::Path a;
                    const float w = 9.0f - (float) i * 2.0f, y = cy - 3.0f + (float) i * 3.0f;
                    a.startNewSubPath (cx - w * 0.5f, y);
                    a.quadraticTo (cx, y - 1.8f, cx + w * 0.5f, y + 0.6f);
                    g.strokePath (a, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                break;
            }
            case Kind::spotifyList:
                for (int i = 0; i < 3; ++i)
                {
                    g.fillRect (cx - 7.0f, cy - 5.0f + i * 4.5f, 2.0f, 2.0f);
                    g.fillRect (cx - 3.5f, cy - 5.0f + i * 4.5f, 10.5f, 2.0f);
                }
                break;
            case Kind::root:
                break;
        }
    }

    const juce::Colour spotifyGreen (0xff1ed760);
}

//==============================================================================
class LibraryTreeItem : public juce::TreeViewItem
{
public:
    LibraryTreeItem (LibraryTree& t, Kind k, const juce::String& l,
                     juce::int64 playlist = 0, const juce::File& f = {}, int c = -1)
        : tree (t), kind (k), label (l), playlistId (playlist), folder (f), count (c)
    {
        setDrawsInLeftMargin (false);
    }

    bool mightContainSubItems() override
    {
        return kind == Kind::root || isHeader();
    }

    bool isHeader() const
    {
        return kind == Kind::playlistsHeader || kind == Kind::foldersHeader || kind == Kind::spotifyHeader;
    }

    bool canBeSelected() const override
    {
        return kind != Kind::root && ! isHeader() && kind != Kind::spotifyLogin && kind != Kind::spotifyUser;
    }

    juce::String getUniqueName() const override
    {
        return juce::String ((int) kind) + ":" + juce::String (playlistId) + ":" + folder.getFullPathName() + ":" + externalId;
    }

    int getItemHeight() const override      { return isHeader() ? 30 : 25; }

    juce::String externalId;            // lista de Spotify

    LibraryView view() const
    {
        LibraryView v;
        v.type = kind == Kind::recent   ? LibraryView::Type::recentlyAdded
               : kind == Kind::history  ? LibraryView::Type::history
               : kind == Kind::playlist ? LibraryView::Type::playlist
               : kind == Kind::folder   ? LibraryView::Type::folder
               : kind == Kind::automix  ? LibraryView::Type::automix
               : kind == Kind::wanted   ? LibraryView::Type::wanted
               : kind == Kind::spotifyList ? LibraryView::Type::spotify
                                        : LibraryView::Type::all;
        v.playlistId = playlistId;
        v.folder = folder;
        v.externalId = externalId;
        return v;
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        const bool header = isHeader();

        if (isSelected())
        {
            g.setColour (deckA.withAlpha (0.3f));
            g.fillRect (0, 0, width, height);
            g.setColour (deckA);
            g.fillRect (0, 0, 3, height);
        }

        auto area = juce::Rectangle<int> (0, 0, width, height);
        const auto iconColour = kind == Kind::playlist ? juce::Colour (0xffb05cf0)
                              : kind == Kind::folder   ? juce::Colour (0xffe8c33a)
                              : kind == Kind::all      ? deckA
                              : kind == Kind::history  ? cue
                              : kind == Kind::automix  ? play
                              : kind == Kind::wanted   ? juce::Colour (0xffe8c33a)
                              : kind == Kind::spotifyLogin || kind == Kind::spotifyUser || kind == Kind::spotifyList ? spotifyGreen
                                                       : textDim;
        drawIcon (g, kind, area.removeFromLeft (24).toFloat(), header ? textDim : iconColour);

        if (count >= 0)
        {
            g.setColour (textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (count), area.removeFromRight (44).withTrimmedRight (6), juce::Justification::centredRight);
        }

        g.setColour (header ? textDim : kind == Kind::spotifyLogin ? spotifyGreen
                                      : kind == Kind::spotifyUser ? textDim
                                      : (isSelected() ? juce::Colours::white : text));
        g.setFont (header ? juce::FontOptions (11.0f, juce::Font::bold) : juce::FontOptions (14.0f));
        g.drawText (header ? label.toUpperCase() : label, area.withTrimmedLeft (4), juce::Justification::centredLeft, true);
    }

    void itemSelectionChanged (bool isNowSelected) override
    {
        if (isNowSelected && canBeSelected())
            tree.itemChosen (view(), label);
    }

    void itemClicked (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            showMenu();
        else if (kind == Kind::spotifyLogin)
        {
            if (tree.onSpotifyLogin) tree.onSpotifyLogin();
        }
        else if (! canBeSelected())
            setOpen (! isOpen());
    }

    void itemDoubleClicked (const juce::MouseEvent&) override
    {
        if (kind == Kind::playlist)
            rename();
    }

    // Soltar canciones del navegador sobre una lista -> se añaden
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& d) override
    {
        return (kind == Kind::playlist || kind == Kind::automix) && d.description.toString().contains ("|ids:");
    }

    void itemDropped (const juce::DragAndDropTarget::SourceDetails& d, int) override
    {
        juce::Array<juce::int64> ids;
        for (auto& s : juce::StringArray::fromTokens (d.description.toString().fromFirstOccurrenceOf ("|ids:", false, false), ",", ""))
            ids.add (s.getLargeIntValue());

        if (kind == Kind::automix)
            tree.library.addToAutomix (ids);
        else
            tree.library.addToPlaylist (playlistId, ids);
    }

private:
    void rename()
    {
        auto& lib = tree.library;
        const auto id = playlistId;
        tree.askText ("Renombrar lista", label, [&lib, id] (const juce::String& name) { lib.renamePlaylist (id, name); });
    }

    void showMenu()
    {
        juce::PopupMenu m;
        auto& lib = tree.library;

        switch (kind)
        {
            case Kind::playlistsHeader:
                m.addItem ("Nueva lista...", [this] { tree.askNewPlaylist(); });
                m.addSeparator();
                m.addItem ("Importar lista (CSV / M3U)...", [this] { if (tree.onImportFile) tree.onImportFile(); });
                m.addItem ("Importar de Spotify...", [this] { if (tree.onImportSpotify) tree.onImportSpotify(); });
                break;

            case Kind::playlist:
            {
                const auto id = playlistId;
                const auto name = label;
                m.addItem ("Renombrar...", [this] { rename(); });
                m.addItem ("Enviar lista a Automix", [&lib, id] { lib.addPlaylistToAutomix (id); });
                m.addItem ("Exportar a M3U...", [this, id, name] { if (tree.onExportPlaylist) tree.onExportPlaylist (id, name); });
                m.addItem ("Eliminar lista", [&lib, id, name]
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Eliminar lista",
                        "Eliminar la lista \"" + name + "\"? Las canciones siguen en la biblioteca.", "Eliminar", "Cancelar", nullptr,
                        juce::ModalCallbackFunction::create ([&lib, id] (int r) { if (r == 1) lib.deletePlaylist (id); }));
                });
                break;
            }

            case Kind::foldersHeader:
                m.addItem (juce::String::fromUTF8 ("Añadir carpeta..."), [this] { tree.askAddFolder(); });
                m.addItem ("Volver a escanear todo", [&lib] { lib.rescanAll(); });
                break;

            case Kind::folder:
            {
                const auto f = folder;
                m.addItem ("Volver a escanear", [&lib, f] { lib.addFolder (f); });
                m.addItem ("Mostrar en el Explorador", [f] { f.revealToUser(); });
                m.addSeparator();
                m.addItem ("Dejar de vigilar esta carpeta", [&lib, f] { lib.removeFolder (f); });
                break;
            }

            case Kind::history:
                m.addItem ("Borrar historial", [&lib]
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Borrar historial",
                        juce::String::fromUTF8 ("Se borrará el historial de reproducciones."), "Borrar", "Cancelar", nullptr,
                        juce::ModalCallbackFunction::create ([&lib] (int r) { if (r == 1) lib.clearHistory(); }));
                });
                break;

            case Kind::automix:
                m.addItem ("Vaciar Automix", [&lib] { lib.clearAutomix(); });
                break;

            case Kind::wanted:
                m.addItem ("Buscar coincidencias en la biblioteca ahora", [&lib] { lib.resolveWanted(); });
                m.addItem ("Vaciar \"Por conseguir\"", [&lib] { lib.clearWanted(); });
                break;

            case Kind::all:
            case Kind::recent:
                m.addItem (juce::String::fromUTF8 ("Añadir carpeta..."), [this] { tree.askAddFolder(); });
                break;

            case Kind::spotifyHeader:
            case Kind::spotifyLogin:
            case Kind::spotifyUser:
            case Kind::spotifyList:
                if (! tree.spotifyConnected)
                {
                    m.addItem (juce::String::fromUTF8 ("Iniciar sesión con Spotify..."), [this] { if (tree.onSpotifyLogin) tree.onSpotifyLogin(); });
                    m.addItem ("Client ID y ajustes de Spotify...", [this] { if (tree.onSpotifySettings) tree.onSpotifySettings(); });
                    break;
                }
                if (kind == Kind::spotifyList)
                {
                    ExternalPlaylist info;
                    for (auto& l : tree.spotifyLists)
                        if (l.id == externalId)
                            info = l;
                    m.addItem ("Copiar a mis listas de CesarDJ", [this, info] { if (tree.onSpotifyImport) tree.onSpotifyImport (info); });
                    m.addSeparator();
                }
                m.addItem ("Actualizar listas", [this] { if (tree.onSpotifyRefresh) tree.onSpotifyRefresh(); });
                m.addItem ("Importar varias listas...", [this] { if (tree.onSpotifyImportAll) tree.onSpotifyImportAll(); });
                m.addItem ("Client ID y ajustes de Spotify...", [this] { if (tree.onSpotifySettings) tree.onSpotifySettings(); });
                m.addSeparator();
                m.addItem (juce::String::fromUTF8 ("Cerrar sesión de Spotify"), [this]
                {
                    juce::Component::SafePointer<LibraryTree> safe (&tree);
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Spotify",
                        juce::String::fromUTF8 ("¿Cerrar la sesión de ") + tree.spotifyUser + "?\n"
                        + juce::String::fromUTF8 ("Tus listas ya copiadas a CesarDJ se conservan."), juce::String::fromUTF8 ("Cerrar sesión"), "Cancelar", nullptr,
                        juce::ModalCallbackFunction::create ([safe] (int r) { if (r == 1 && safe != nullptr && safe->onSpotifyLogout) safe->onSpotifyLogout(); }));
                });
                break;

            case Kind::root:
                break;
        }

        if (m.getNumItems() > 0)
            m.showMenuAsync (juce::PopupMenu::Options());
    }

    LibraryTree& tree;
    const Kind kind;
    const juce::String label;
    const juce::int64 playlistId;
    const juce::File folder;
    const int count;
};

//==============================================================================
LibraryTree::LibraryTree (Library& lib)
    : library (lib)
{
    setRootItemVisible (false);
    setDefaultOpenness (true);
    setIndentSize (14);
    setColour (juce::TreeView::backgroundColourId, inset);
    setColour (juce::TreeView::linesColourId, juce::Colours::transparentBlack);
    setWantsKeyboardFocus (false);
    refresh();
}

LibraryTree::~LibraryTree()
{
    setRootItem (nullptr);
}

void LibraryTree::refresh()
{
    const juce::ScopedValueSetter<bool> svs (refreshing, true);
    const auto state = root != nullptr ? getOpennessState (true) : nullptr;

    auto newRoot = std::make_unique<LibraryTreeItem> (*this, Kind::root, "root");
    newRoot->addSubItem (new LibraryTreeItem (*this, Kind::all, "Todas las canciones", 0, {}, library.countTracks()));
    newRoot->addSubItem (new LibraryTreeItem (*this, Kind::recent, juce::String::fromUTF8 ("Añadidas recientemente")));
    newRoot->addSubItem (new LibraryTreeItem (*this, Kind::history, "Historial"));
    newRoot->addSubItem (new LibraryTreeItem (*this, Kind::automix, "Automix", 0, {}, library.automixCount()));
    if (const int wanted = library.wantedCount(); wanted > 0)
        newRoot->addSubItem (new LibraryTreeItem (*this, Kind::wanted, "Por conseguir", 0, {}, wanted));

    auto* lists = new LibraryTreeItem (*this, Kind::playlistsHeader, "Listas");
    for (auto& p : library.getPlaylists())
        lists->addSubItem (new LibraryTreeItem (*this, Kind::playlist, p.name, p.id, {}, p.count));
    newRoot->addSubItem (lists);

    auto* folders = new LibraryTreeItem (*this, Kind::foldersHeader, "Carpetas");
    for (auto& f : library.getFolders())
        folders->addSubItem (new LibraryTreeItem (*this, Kind::folder, f.getFileName().isEmpty() ? f.getFullPathName() : f.getFileName(), 0, f));
    newRoot->addSubItem (folders);

    auto* spotify = new LibraryTreeItem (*this, Kind::spotifyHeader, "Spotify");
    if (! spotifyConnected)
        spotify->addSubItem (new LibraryTreeItem (*this, Kind::spotifyLogin, juce::String::fromUTF8 ("Iniciar sesión con Spotify")));
    else
    {
        spotify->addSubItem (new LibraryTreeItem (*this, Kind::spotifyUser, spotifyUser.isNotEmpty() ? spotifyUser : juce::String ("Conectado")));
        for (auto& l : spotifyLists)
        {
            auto* item = new LibraryTreeItem (*this, Kind::spotifyList, l.name, 0, {}, l.count);
            item->externalId = l.id;
            spotify->addSubItem (item);
        }
    }
    newRoot->addSubItem (spotify);

    setRootItem (newRoot.get());
    root = std::move (newRoot);

    if (state != nullptr)
        restoreOpennessState (*state, true);
    else
        root->getSubItem (0)->setSelected (true, true, juce::dontSendNotification);
}

void LibraryTree::setSpotifyState (bool connected, const juce::String& user, const std::vector<ExternalPlaylist>& lists)
{
    spotifyConnected = connected;
    spotifyUser = user;
    spotifyLists = lists;
    refresh();
}

void LibraryTree::itemChosen (const LibraryView& view, const juce::String& title)
{
    if (! refreshing && onViewSelected != nullptr)
        onViewSelected (view, title);
}

void LibraryTree::selectView (const LibraryView& view)
{
    if (root == nullptr)
        return;

    std::function<bool (juce::TreeViewItem*)> find = [&] (juce::TreeViewItem* item)
    {
        if (auto* li = dynamic_cast<LibraryTreeItem*> (item))
            if (li->canBeSelected() && li->view() == view)
            {
                li->setSelected (true, true);
                return true;
            }

        for (int i = 0; i < item->getNumSubItems(); ++i)
            if (find (item->getSubItem (i)))
                return true;
        return false;
    };

    find (root.get());
}

void LibraryTree::askText (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onOk)
{
    prompt = std::make_unique<juce::AlertWindow> (title, "", juce::MessageBoxIconType::NoIcon);
    prompt->addTextEditor ("name", initial);
    prompt->addButton ("Aceptar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    prompt->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<LibraryTree> safe (this);

    prompt->enterModalState (true, juce::ModalCallbackFunction::create ([safe, onOk] (int result)
    {
        if (safe == nullptr || safe->prompt == nullptr)
            return;

        const auto entered = safe->prompt->getTextEditorContents ("name").trim();
        if (result == 1 && entered.isNotEmpty())
            onOk (entered);

        // El AlertWindow no se puede borrar dentro de su propio callback
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->prompt.reset();
        });
    }), false);

    // Texto seleccionado y con el foco: escribir reemplaza el nombre directamente
    if (auto* editor = prompt->getTextEditor ("name"))
    {
        editor->selectAll();
        editor->grabKeyboardFocus();
    }
}

void LibraryTree::askNewPlaylist (const juce::Array<juce::int64>& tracksToAdd)
{
    askText ("Nueva lista", "Nueva lista", [this, tracksToAdd] (const juce::String& name)
    {
        const auto id = library.createPlaylist (name);
        if (! tracksToAdd.isEmpty())
            library.addToPlaylist (id, tracksToAdd);
    });
}

void LibraryTree::askAddFolder()
{
    chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Elige una carpeta con música"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.isDirectory())
                                  library.addFolder (f);
                          });
}
