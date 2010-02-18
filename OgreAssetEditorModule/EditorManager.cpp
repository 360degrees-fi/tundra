// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   EditorManager.cpp
 *  @brief  EditorManager manages the editor windows.
 */

#include "StableHeaders.h"
#include "EditorManager.h"

namespace OgreAssetEditor
{

EditorManager::EditorManager()
{
}

EditorManager::~EditorManager()
{
    DeleteAll();
}

bool EditorManager::Add(const QString &inventory_id, RexTypes::asset_type_t asset_type, QWidget *editor)
{
    if (!editor || inventory_id.isEmpty() || asset_type == RexTypes::RexAT_None)
        return false;

    editors_[qMakePair(inventory_id, asset_type)] = editor;
    return true;
}

QWidget *EditorManager::GetEditor(const QString &inventory_id, RexTypes::asset_type_t asset_type)
{
    EditorMapKey key = qMakePair(inventory_id, asset_type);
    EditorMapIter it = editors_.find(key);
    if (it != editors_.end())
        return it.value();
    else
        return 0;
}

bool EditorManager::Exists(const QString &inventory_id, RexTypes::asset_type_t asset_type) const
{
    EditorMapKey key = qMakePair(inventory_id, asset_type);
    return editors_.contains(key);
}

bool EditorManager::Delete(const QString &inventory_id, RexTypes::asset_type_t asset_type)
{
    QWidget *editor = TakeEditor(inventory_id, asset_type);
    if (editor)
    {
        editor->deleteLater();
        return true;
    }
    else
        return false;
}

void EditorManager::DeleteAll(bool delete_later)
{
    MutableEditorMapIter it(editors_);
    while(it.hasNext())
    {
        QWidget *editor = it.next().value();
        if (delete_later)
            editor->deleteLater();
        else
            SAFE_DELETE(editor);
        it.remove();
    }
}

QWidget *EditorManager::TakeEditor(const QString &inventory_id, RexTypes::asset_type_t asset_type)
{
    EditorMapKey key = qMakePair(inventory_id, asset_type);
    EditorMapIter it = editors_.find(key);
    if (it != editors_.end())
        return editors_.take(key);
    else
        return 0;
}

}
