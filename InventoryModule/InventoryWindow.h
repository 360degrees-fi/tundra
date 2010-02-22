// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   InventoryWindow.h
 *  @brief  Inventory window. Should be totally unaware of the underlaying inventory data model.
 */

#ifndef incl_InventoryModule_InventoryWindow_h
#define incl_InventoryModule_InventoryWindow_h

#include "RexTypes.h"

#include <boost/shared_ptr.hpp>

#include <QWidget>
#include <QPointer>
#include <QMap>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QMenu;
class QAction;
class QMessageBox;
class QModelIndex;
class QLineEdit;
QT_END_NAMESPACE

namespace Foundation
{
    class EventDataInterface;
}

namespace ProtocolUtilities
{
    class WorldStream;
    typedef boost::shared_ptr<WorldStream> WorldStreamPtr;
}

namespace UiServices
{
    class UiProxyWidget;
}

namespace Inventory
{
    class InventoryModule;
    class InventoryTreeView;
    class InventoryItemEventData;
    class InventoryItemModel;
    class AbstractInventoryDataModel;

    typedef boost::shared_ptr<AbstractInventoryDataModel> InventoryPtr;

    class InventoryWindow : public QWidget
    {
        Q_OBJECT

    public:
        /// Constructor.
        /// @param owner InventoryModule pointer
        InventoryWindow(InventoryModule *owner, QWidget *parent = 0);

        /// Destructor.
        virtual ~InventoryWindow();

    public slots:
        /// Initializes the inventory data/view model.
        /// @param inventory_model Inventory data model pointer.
        void InitInventoryTreeModel(InventoryPtr inventory_model);

        /// Resets the inventory tree model.
        void ResetInventoryTreeModel();

        /// Hides the inventory window.
        void Hide();

    private slots:
        /// Opens inventory item (folder or asset) when user double-clicks it.
        void OpenItem();

        /// Expands the folder at spesific index.
        /// @param index Folder model index.
        void ExpandFolder(const QModelIndex &index);

        /// Opens inventory item's property window.
        void OpenItemProperties();

        /// Adds new folder.
        void AddFolder();

        /// Deletes item (folder/asset).
        void DeleteItem();

        /// Renames item (folder/asset).
        void RenameItem();

        /// Opens file open dialog.
        void Upload();

        /// Uploads files.
        /// @param filenames list of filenames to upload.
        void UploadFiles(QStringList &filenames);

        /// File download.
        void Download();

        /// Copies asset reference (UUID or URL) to the clipboard.
        void CopyAssetReference();

        /// Searchs inventory items by selected text. Currently searches only by the name of item.
        /// Sets the best matching item selected.
        /// @param text Search text
//        void Search(const QString &text);

        /// Updates possible actions depending on the currently active tree view item.
        void UpdateActions();

        /// Opens download progress dialog.
        /// @param asset_id Asset id.
        /// @param name Asset name.
        void OpenDownloadProgess(const QString &asset_id, const QString &name);

        /// Aborts download.
        /// @param asset_id Asset id.
        void AbortDownload(const QString &asset_id);

        /// Closes download progress dialog.
        /// @param asset_id Asset id.
        void CloseDownloadProgess(const QString &asset_id);

/*
        /// Opens the upload progress bar window.
        /// @param file_count Number of files to be uploaded.
        void OpenUploadProgress(size_t file_count);

        /// Updates the upload progress bar window.
        /// @param filename Filename.
        void UploadStarted(const QString &filename);

        ///
//        void UploadCompleted(const QString &filename);

        ///
        void CloseUploadProgress();
*/
    signals:
        /// Use this signal to send notification to the UI.
        /// @param message Message to be shown.
        /// @param duration Duration for which the message is visible.
        void Notification(const QString &message, int duration);

    private:
        Q_DISABLE_COPY(InventoryWindow);

        /// Initializes the inventory UI.
        void InitInventoryWindow();

        /// Creates the context menu actions.
        void CreateActions();

        /// InventoryModule pointer.
        InventoryModule *owner_;

        /// Inventory view model.
        QPointer<InventoryItemModel> inventoryItemModel_;

        /// Inventory window widget.
        QWidget *mainWidget_;

        /// Layout 
        QVBoxLayout *layout_;

        /// Upload progress window widget
        QWidget *uploadWidget_;

        // Inventory tree view widget
        InventoryTreeView *treeView_;

        // Inventory search field
//        QLineEdit *lineEditSearch_;

        /// Proxy widget for ui
        UiServices::UiProxyWidget *proxyWidget_;

        /// Proxy widget for upload progress ui.
        UiServices::UiProxyWidget *uploadProxyWidget_;

        /// Action menu.
        QMenu *actionMenu_;

        /// Delete action.
        QAction *actionDelete_;

        /// Rename action.
        QAction *actionRename_;

        /// Cut action.
        QAction *actionCut_;

        /// Paste action.
        QAction *actionPaste_;

        /// New Folder action.
        QAction *actionNewFolder_;

        /// Open action.
        QAction *actionOpen_;

        /// Properties action.
        QAction *actionProperties_;

        /// Copy asset reference action.
        QAction *actionCopyAssetReference_;

        /// Upload action.
        QAction *actionUpload_;

        /// Download action.
        QAction *actionDownload_;

        /// Separator (action) for the context menu.
        QAction *actionSeparator_;

        /// Map of active download progress dialogs.
//        QMap<QString, QMessageBox *> downloadDialogs_;

        /// Offset for download dialog positions.
//        size_t offset_;

        /// Last file path used when using open file dialog.
        QString lastUsedPath_;

        /// Counter used for file upload progress window.
        size_t uploadCount_;
    };
}

#endif
