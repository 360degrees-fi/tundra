/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SceneTreeView.h
 *  @brief  Tree widget showing the scene structure.
 */

#ifndef incl_SceneStructureModule_SceneTreeWidget_h
#define incl_SceneStructureModule_SceneTreeWidget_h

#include "CoreTypes.h"
#include "ForwardDefines.h"

#include <QTreeWidget>
#include <QPointer>

namespace ECEditor
{
    class ECEditorWindow;
}

class QWidget;
class QFileDialog;

/// Tree widget item representing entity.
class EntityItem : public QTreeWidgetItem
{
public:
    /// Constructor.
    /** @param entity Entity pointer.
    */
    explicit EntityItem(const Scene::EntityPtr &entity);

    /// Entity ID associated with this tree widget item.
    entity_id_t id;

    /// Returns pointer to the entity this item represents.
    Scene::EntityPtr Entity() const;

private:
    /// Weak pointer to the component this item represents.
    Scene::EntityWeakPtr ptr;
};

/// Tree widget item representing component.
class ComponentItem : public QTreeWidgetItem
{
public:
    /// Constructor.
    /** @param comp Component pointer.
        @param parent Parent entity item.
    */
    ComponentItem(const ComponentPtr &comp, EntityItem *parent);

    /// Type name.
    QString typeName;

    ///  Name, if applicable.
    QString name;

    /// Returns pointer to the entity this item represents.
    ComponentPtr Component() const;

    /// Returns the parent entity item.
    EntityItem *Parent() const;

private:
    /// Weak pointer to the component this item represents.
    ComponentWeakPtr ptr;

    /// Parent entity item.
    EntityItem *parentItem;
};

/// Tree widget item representing asset reference.
class AssetItem : public QTreeWidgetItem
{
public:
    /// Constructor.
    /** @param i ID.
        @param t Type.
        @param parent Parent item.
    */
    AssetItem(const QString &i, const QString &t, QTreeWidgetItem *parent = 0) :
        QTreeWidgetItem(parent), id(i), type(t) {}

    /// ID.
    QString id;

    /// Type.
    QString type;
};

/// Represents selection of selected scene tree widget items.
struct Selection
{
    /// Returns true if no entity or component items selected.
    bool IsEmpty() const;

    /// Returns true if selection contains entities;
    bool HasEntities() const;

    /// Returns true if selected contains components.
    bool HasComponents() const;

    /// Returns set containing entity ID's of both selected entities and parent entities of selected components
    QSet<entity_id_t> EntityIds() const;

    /// List of selected entities.
    QList<EntityItem *> entities;

    /// List of selected components.
    QList<ComponentItem *> components;
};

/// Tree widget showing the scene structure.
class SceneTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    /// Constructor.
    /** @param fw Framework pointer.
        @param parent Parent widget.
    */
    SceneTreeWidget(Foundation::Framework *fw, QWidget *parent = 0);

    /// Destructor.
    virtual ~SceneTreeWidget();

    /// Do we show components in the tree widget or not.
    bool showComponents;

protected:
    /// QAbstractItemView override.
    void contextMenuEvent(QContextMenuEvent *e);

    /// QAbstractItemView override.
    void dragEnterEvent(QDragEnterEvent *e);

    /// QAbstractItemView override.
    void dragMoveEvent(QDragMoveEvent *e);

    /// QAbstractItemView override.
    void dropEvent(QDropEvent *e);

private:
    /// Creates right-click context menu actions.
    /** @param [out] menu Context menu.
    */
    void AddAvailableActions(QMenu *menu);

    /// Framework pointer.
    Foundation::Framework *framework;

    /// This widget's "own" EC editor.
    QPointer<ECEditor::ECEditorWindow> ecEditor;

    /// Returns selected items as Selection struct, which contains both selected entities and components.
    Selection GetSelection() const;

    /// Returns currently selected entities as XML string.
    QString GetSelectionAsXml() const;

    /// Keeps track of the latest opened file save/open dialog, so that we won't multiple open at the same time.
    QPointer<QFileDialog> fileDialog;

private slots:
    /// Opens selected entities in EC editor window. An exisiting editor window is used if possible.
    void Edit();

    /// Opens selected entities in EC editor window. New editor window is created each time.
    void EditInNew();

    /// Renames selected entity.
    void Rename();

    /// Sets new name for item (entity or component) when it's renamed.
    /** @item The item which was renamed.
    */
    void OnItemEdited(QTreeWidgetItem *item, int);

    void CloseEditor(QTreeWidgetItem *,QTreeWidgetItem *);

    /// Creates a new entity.
    void NewEntity();

    /// Creates a new component.
    void NewComponent();

    /// Called by Add Component dialog when it's closed.
    /** @param result Result of dialog closure. OK is 1, Cancel is 0.
    */
    void ComponentDialogFinished(int result);

    /// Deletes an existing entity or component.
    void Delete();

    /// Copies selected entities as XML to clipboard.
    void Copy();

    /// Adds clipboard contents to scene as XML.
    void Paste();

    /// Saves selected entities as XML or binary file.
    void SaveAs();

    /// Saves entire scene as XML or binary file.
    void SaveSceneAs();

    /// Imports OGRE or Naali scene file.
    void Import();

    /// Loads new scene.
    void OpenNewScene();

    /// Opens Entity Action dialog.
    void OpenEntityActionDialog();

    /// Called by Entity Action dialog when it's closed.
    /** @param result Result of dialog closure. Close is 0, Execute and Close is 1, Execute is 2.
    */
    void EntityActionDialogClosed(int result);

    /// Opens Function dialog.
    void OpenFunctionDialog();

    /// Called by Function dialog when it's closed.
    /** @param result Result of dialog closure. Close is 0, Execute and Close is 1, Execute is 2.
    */
    void FunctionDialogClosed(int result);

    /// Called by "Save Selection" save file dialog when it's closed.
    /** @param result Result of dialog closure. Save is 1, Cancel is 0.
    */
    void SaveSelectionDialogClosed(int result);

    /// Called by "Save Scene" save file dialog when it's closed.
    /** @param result Result of dialog closure. Save is 1, Cancel is 0.
    */
    void SaveSceneDialogClosed(int result);

    /// Called by open file dialog when it's closed.
    /** @param result Result of dialog closure. Open is 1, Cancel is 0.
    */
    void OpenFileDialogClosed(int result);
};

#endif
