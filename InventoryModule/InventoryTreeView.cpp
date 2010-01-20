// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file InventoryTreeView.cpp
 *  @brief Inventory tree view UI widget.
 */

#include "StableHeaders.h"
#include "InventoryTreeView.h"
#include "InventoryWindow.h"
#include "InventoryItemModel.h"
#include "AbstractInventoryDataModel.h"

#include <QWidget>
#include <QDragEnterEvent>
#include <QUrl>
#include <QMenu>

namespace Inventory
{

InventoryTreeView::InventoryTreeView(QWidget *parent) : QTreeView(parent)
{
    setEditTriggers(QAbstractItemView::NoEditTriggers/*EditKeyPressed*/);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setAnimated(true);
    setAllColumnsShowFocus(true);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setStyleSheet(
    "QTreeView::branch:has-siblings:!adjoins-item"
    "{"
        "border-image: url(:/images/iconBranchVLine.png) 0;"
    "}"
    "QTreeView::branch:has-siblings:adjoins-item"
    "{"
        "border-image: url(:/images/iconBranchMore.png) 0;"
    "}"
    "QTreeView::branch:!has-children:!has-siblings:adjoins-item"
    "{"
        "border-image: url(:/images/iconBranchEnd.png) 0;"
    "}"
    "QTreeView::branch:has-children:!has-siblings:closed,"
    "QTreeView::branch:closed:has-children:has-siblings"
    "{"
        "border-image: none;"
        "image: url(:/images/iconBranchClosed.png);"
    "}"
    "QTreeView::branch:open:has-children:!has-siblings,"
    "QTreeView::branch:open:has-children:has-siblings"
    "{"
        "border-image: none;"
        "image: url(:/images/iconBranchOpen.png);"
    "}");
}

// virtual
InventoryTreeView::~InventoryTreeView()
{
}

void InventoryTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    // Do mousePressEvent so that the item gets selected first.
    QMouseEvent mouseEvent(QEvent::MouseButtonPress, event->pos(), event->globalPos(),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    mousePressEvent(&mouseEvent);

    QModelIndex index = selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    QMenu *menu = new QMenu(this);
    QListIterator<QAction *> it(actions());
    while(it.hasNext())
    {
        QAction *action = it.next();
        if (action->isEnabled())
            menu->addAction(action);
    }

    if (menu->actions().size() > 1) // separator "action" is always enabled
        menu->popup(event->globalPos());
}

void InventoryTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/vnd.inventory.item"))
    {
        if (event->source() == this)
        {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else
            event->acceptProposedAction();
    }
    else if(event->mimeData()->hasUrls())
        event->accept();
    else
        event->ignore();
}

void InventoryTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/vnd.inventory.item"))
    {
        if (event->source() == this)
        {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        else
            event->acceptProposedAction();
    }
    else if(event->mimeData()->hasUrls())
        event->accept();
    else
        event->ignore();
}

void InventoryTreeView::dropEvent(QDropEvent *event)
{
    const QMimeData *data = event->mimeData();
    if (data->hasUrls())
    {
        InventoryItemModel *itemModel = dynamic_cast<InventoryItemModel *>(model());
        if (!itemModel)
        {
            event->ignore();
            return;
        }

        AbstractInventoryDataModel *m = itemModel->GetInventory();
        if (!m)
        {
            event->ignore();
            return;
        }

        QStringList filenames, itemnames;
        QListIterator<QUrl> it(data->urls());
        while(it.hasNext())
            filenames << it.next().path().remove(0, 1); // remove '/' from the beginning

        if (!filenames.isEmpty())
            m->UploadFiles(filenames, itemnames, 0);
    }
    else
        QTreeView::dropEvent(event);
}

}
