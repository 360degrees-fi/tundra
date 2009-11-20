// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file WebDavInventoryDataModel.cpp
 *  @brief Data model representing the WebDAV inventory model.
 */

#include "StableHeaders.h"
#include "WebDavInventoryDataModel.h"
#include "InventoryAsset.h"
#include "InventoryModule.h"
#include "RexUUID.h"

#include <QDir>

namespace Inventory
{

WebDavInventoryDataModel::WebDavInventoryDataModel(const QString &identityUrl, const QString &hostUrl) :
    identityUrl_(identityUrl), hostUrl_(hostUrl), rootFolder_(0)
{
    if (InitPythonQt())
    {
        if (FetchWebdavUrlWithIdentity())
            FetchRootFolder();
        else
            ErrorOccurredCreateEmptyRootFolder();
    }
    else
        ErrorOccurredCreateEmptyRootFolder();
}

WebDavInventoryDataModel::~WebDavInventoryDataModel()
{
    SAFE_DELETE(rootFolder_);
}

/// AbstractInventoryDataModel INTERFACE
AbstractInventoryItem *WebDavInventoryDataModel::GetFirstChildFolderByName(const QString &searchName) const
{
    InventoryModule::LogInfo("Webdav | You are in GetFirstChildFolderByName() that is not implemented yet");
    return 0;
}

AbstractInventoryItem *WebDavInventoryDataModel::GetChildFolderById(const QString &searchId) const
{
    if (RexUUID::IsValid(searchId.toStdString()))
        return 0;
    else
        return rootFolder_->GetChildFolderById(searchId);
}

AbstractInventoryItem *WebDavInventoryDataModel::GetChildAssetById(const QString &searchId) const
{
    InventoryModule::LogInfo("Webdav | You are in GetChildAssetById() that is not implemented yet");
    return 0;
}

AbstractInventoryItem *WebDavInventoryDataModel::GetChildById(const QString &searchId) const
{
    return rootFolder_->GetChildById(searchId);
}

AbstractInventoryItem *WebDavInventoryDataModel::GetOrCreateNewFolder(const QString &id, AbstractInventoryItem &parentFolder,
        const QString &name, const bool &notify_server)
{
    InventoryFolder *parent = dynamic_cast<InventoryFolder *>(&parentFolder);
    if (!parent)
        return 0;

    if (RexUUID::IsValid(id.toStdString()))
    {
        // This is a new folder if the id is RexUUID 
        // RexUUID generated in data model, cant do nothing about this...
        QString parentPath = parent->GetID();
        QString newFolderName = name;
        QStringList result = webdavclient_.call("createDirectory", QVariantList() << parentPath << newFolderName).toStringList();
        if (result.count() >= 1)
        {
            if (result[0] == "True")
            {
                //ItemSelectedFetchContent(&parentFolder);
                FetchInventoryDescendents(parent);
                InventoryModule::LogInfo(QString("Webdav | Created folder named %1 to path %2\n").arg(newFolderName, parentPath).toStdString());
            }
            else
                InventoryModule::LogInfo(QString("Webdav | Could not create folder named %1 to path %2\n").arg(newFolderName, parentPath).toStdString());
        }
    }
    else
    {
        // If its not RexUUID or and existing item in this folder,
        // then its a move command. Lets do that to the webdav server then...
        InventoryFolder *existingItem = parent->GetChildFolderById(id);
        if (!existingItem)
        {
            InventoryFolder *currentFolder = rootFolder_->GetChildFolderById(name);
            if (!currentFolder)
                return 0;

            QString currentPath = ValidateFolderPath(currentFolder->GetParent()->GetID());
            QString newPath = ValidateFolderPath(parent->GetID());
            QString folderName = name;
            QString deepCopy = "False";
            if (currentFolder->GetChildList().count() > 0)
                deepCopy = "True";
            QStringList result = webdavclient_.call("moveResource", QVariantList() << currentPath << newPath << folderName).toStringList();
            if (result.count() >= 1)
            {
                if (result[0] == "True")
                {
                    //ItemSelectedFetchContent(&parentFolder);
                    FetchInventoryDescendents(parent);
                    InventoryModule::LogInfo(QString("Webdav | Moved folder %1 from %2 to %3\nNote: This fucntionality is experimental,").append(
                        "dont assume it went succesfull\n").arg(folderName, currentPath, newPath).toStdString());
                }
                else
                {
                    InventoryModule::LogInfo(QString("Webdav | Failed to move folder %1 from %2 to %3\n").arg(
                        folderName, currentPath, newPath).toStdString());
                }
            }
        }
        else
            return existingItem;
    }

    // Return 0 to data model, we just updated 
    // folder content from webdav, no need to return the real item
    return 0;
}

AbstractInventoryItem *WebDavInventoryDataModel::GetOrCreateNewAsset(const QString &inventory_id, const QString &asset_id,
        AbstractInventoryItem &parentFolder, const QString &name)
{
    InventoryModule::LogInfo("Webdav | You are in GetOrCreateNewAsset() that is not implemented yet");
    return 0;
}

void WebDavInventoryDataModel::FetchInventoryDescendents(AbstractInventoryItem *item)
{
    InventoryFolder *selected = dynamic_cast<InventoryFolder *>(item);
    if (!selected)
        return;

    // Delete children
    selected->GetChildList().clear();
    //QListIterator<AbstractInventoryItem *> it(selected->GetChildList());
    //while(it.hasNext())
    //{
    //    AbstractInventoryItem *item = it.next();
    //    if (item)
    //        delete item;
    //}

    QString itemPath = selected->GetID();
    QStringList children = webdavclient_.call("listResources", QVariantList() << itemPath).toStringList();
    if ( children.count() >=1 )
    {
        // Process child list to map
        QMap<QString, QString> childMap;
        for (int index=0; index<=children.count(); index++)
        {
            childMap[children.value(index)] = children.value(index+1);
            index++;
        }

        AbstractInventoryItem *newItem = 0;
        QString path, name, type;
        for (QMap<QString, QString>::iterator iter = childMap.begin(); iter!=childMap.end(); ++iter)
        {
            path = iter.key();
            name = path.midRef(path.lastIndexOf("/")+1).toString();
            type = iter.value();
            if (name != "")
            {
                if (type == "resource")
                    newItem = new InventoryAsset(path, "", name, selected);
                else
                    newItem = new InventoryFolder(path, name, selected, true);
                selected->AddChild(newItem);
            }
        }

        InventoryModule::LogInfo(QString("Webdav | Fetched %1 children to path /%2").arg(QString::number(childMap.count()), itemPath).toStdString());
    }
}

void WebDavInventoryDataModel::NotifyServerAboutItemMove(AbstractInventoryItem *item)
{
    InventoryModule::LogInfo("Webdav | You are in NotifyServerAboutItemMove() that is not implemented yet");
}

void WebDavInventoryDataModel::NotifyServerAboutItemCopy(AbstractInventoryItem *item)
{
    InventoryModule::LogInfo("Webdav | You are in NotifyServerAboutItemCopy() that is not implemented yet");
}

void WebDavInventoryDataModel::NotifyServerAboutItemRemove(AbstractInventoryItem *item)
{
    QString parentPath = ValidateFolderPath(item->GetParent()->GetID());
    QString itemName = item->GetName();
    QString methodName;
    QStringList result;

    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
        methodName = "deleteDirectory";
    else if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
        methodName = "deleteResource";
    else
        return;

    result = webdavclient_.call(methodName, QVariantList() << parentPath << itemName).toStringList();
    if (result.count() >= 1)
    {
        if (result[0] == "True")
        {
            InventoryModule::LogInfo(QString("Webdav | Removed item from %1\n").arg(item->GetID()).toStdString());
            //ItemSelectedFetchContent(item->GetParent());
            FetchInventoryDescendents(item->GetParent());
        }
        else
            InventoryModule::LogInfo(QString("Webdav | Could not remove item from %1\n").arg(item->GetID()).toStdString());
    }
}

void WebDavInventoryDataModel::NotifyServerAboutItemUpdate(AbstractInventoryItem *item, const QString &old_name)
{
    QString parentPath = ValidateFolderPath(item->GetParent()->GetID());
    QString newName = item->GetName();
    QString oldName = old_name;

    QStringList result = webdavclient_.call("renameResource", QVariantList() << parentPath << newName << oldName).toStringList();
    if (result.count() >= 1)
    {
        if (result[0] == "True")
        {
            //ItemSelectedFetchContent(item->GetParent());
            FetchInventoryDescendents(item->GetParent());
            InventoryModule::LogInfo(QString("Webdav | Renamed folder from %0 to %1 in path %3\n").arg(oldName, newName, parentPath).toStdString());
        }
        else
            InventoryModule::LogInfo(QString("Webdav | Could not rename folder from %0 to %1 in path %3\n").arg(oldName, newName, parentPath).toStdString());
    }
}

/// PUBLIC SLOTS
/*
void WebDavInventoryDataModel::ItemSelectedFetchContent(AbstractInventoryItem *item)
{
    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
    {
        InventoryFolder *selected = dynamic_cast<InventoryFolder *>(item);
        if (selected)
        {
            // Delete children
            selected->GetChildList().clear();
            //QListIterator<AbstractInventoryItem *> it(selected->GetChildList());
            //while(it.hasNext())
            //{
            //    AbstractInventoryItem *item = it.next();
            //    if (item)
            //        delete item;
            //}

            QString itemPath = selected->GetID();
            QStringList children = webdavclient_.call("listResources", QVariantList() << itemPath).toStringList();
            if ( children.count() >=1 )
            {
                // Process child list to map
                QMap<QString, QString> childMap;
                for (int index=0; index<=children.count(); index++)
                {
                    childMap[children.value(index)] = children.value(index+1);
                    index++;
                }

                AbstractInventoryItem *newItem;
                QString path, name, type;
                for (QMap<QString, QString>::iterator iter = childMap.begin(); iter!=childMap.end(); ++iter)
                {
                    path = iter.key();
                    name = path.midRef(path.lastIndexOf("/")+1).toString();
                    type = iter.value();
                    if (name != "")
                    {
                        if (type == "resource")
                            newItem = new InventoryAsset(path, "", name, selected);
                        else
                            newItem = new InventoryFolder(path, name, selected, true);
                        selected->AddChild(newItem);
                    }
                }
                InventoryModule::LogInfo(QString("Webdav | Fetched %1 children to path /%2").arg(QString::number(childMap.count()), itemPath).toStdString());
            }
        }
    }
}
*/

void WebDavInventoryDataModel::UploadFile(const QString &file_path, AbstractInventoryItem *parent_folder)
{
    InventoryFolder *parentFolder = dynamic_cast<InventoryFolder *>(parent_folder);
    if (!parentFolder)
        return;

    QString filePath = file_path;
    QString filename = filePath.split("/").last();
    QString parentPath = ValidateFolderPath(parentFolder->GetID());
    QStringList result = webdavclient_.call("uploadFile", QVariantList() << filePath << parentPath << filename).toStringList();
    if (result.count() >= 1)
    {
        if (result[0] == "True")
        {
            //ItemSelectedFetchContent(parent_folder);
            FetchInventoryDescendents(parent_folder);
            InventoryModule::LogInfo(QString("Webdav | Upload of file %1 to path %2%3 succeeded\n").arg(filePath, parentPath, filename).toStdString());
        }
        else
            InventoryModule::LogInfo(QString("Webdav | Upload of file %1 to path %2%3 failed\n").arg(filePath, parentPath, filename).toStdString());
    }
}

void WebDavInventoryDataModel::UploadFiles(QStringList &filenames, AbstractInventoryItem *parent_folder)
{
    InventoryModule::LogInfo("Webdav | You are in UploadFiles() that is not implemented yet");
}

void WebDavInventoryDataModel::UploadFilesFromBuffer(QStringList &filenames, QVector<QVector<uchar> > &buffers,
    AbstractInventoryItem *parent_folder)
{
    InventoryModule::LogInfo("Webdav | You are in UploadFilesFromBuffer() that is not implemented yet");
}

void WebDavInventoryDataModel::DownloadFile(const QString &store_folder, AbstractInventoryItem *selected_item)
{
    InventoryAsset *item = dynamic_cast<InventoryAsset *>(selected_item);
    if (!item)
        return;

    QString storePath = store_folder;
    QString parentPath = ValidateFolderPath(item->GetParent()->GetID());
    QString filename = item->GetName();
    QStringList result = webdavclient_.call("downloadFile", QVariantList() << storePath << parentPath << filename).toStringList();
    if (result.count() >= 1)
    {
        if (result[0] == "True")
            InventoryModule::LogInfo(QString("Webdav | Downloaded file %1%2 to path %3\n").arg(parentPath, filename, storePath).toStdString());
        else
            InventoryModule::LogInfo(QString("Webdav | Downloaded of file %1%2 to path %3 failed\n").arg(parentPath, filename, storePath).toStdString());
    }
}

/// WEBDAV RELATED (private)

bool WebDavInventoryDataModel::InitPythonQt()
{
    QString myPath = QString("%1/pymodules/webdavinventory").arg(QDir::currentPath());

    pythonQtMainModule_ = PythonQt::self()->getMainModule();
    pythonQtMainModule_.evalScript(QString("print '[PythonQt] Webdav inventory fetched me...'"));
    pythonQtMainModule_.evalScript(QString("import sys\n"));
    pythonQtMainModule_.evalScript(QString("sys.path.append('%1')\n").arg(myPath));
    pythonQtMainModule_.evalScript(QString("print '[PythonQt] Added %1 to sys.path for imports'").arg(myPath));

    Q_ASSERT(!pythonQtMainModule_.isNull());
    return true;
}

bool WebDavInventoryDataModel::FetchWebdavUrlWithIdentity()
{
    pythonQtMainModule_.evalScript("import connection\n");
    PythonQtObjectPtr httpclient = pythonQtMainModule_.evalScript("connection.HTTPClient()\n", Py_eval_input);

    // Some url verification, remove http:// and everything after the port
    int index = hostUrl_.indexOf("http://");
    if (index != -1)
        hostUrl_ = hostUrl_.midRef(index+7).toString();
    index = hostUrl_.indexOf("/");
    if (index != -1)
        hostUrl_ = hostUrl_.midRef(0, index).toString();

    // Set up HTTP connection to Taiga WorldServer
    httpclient.call("setupConnection", QVariantList() << hostUrl_ << "openid" << identityUrl_);
    // Get needed webdav access urls from Taiga WorldServer
    QStringList resultList = httpclient.call("requestIdentityAndWebDavURL").toStringList();
    // Store results
    if (resultList.count() < 1)
        return false;

    webdavIdentityUrl_ = resultList.value(0);
    webdavUrl_ = resultList.value(1);

    return true;
}

void WebDavInventoryDataModel::FetchRootFolder()
{
    webdavclient_ = pythonQtMainModule_.evalScript("connection.WebDavClient()\n", Py_eval_input);
    if (!webdavclient_)
    {
        ErrorOccurredCreateEmptyRootFolder();
        return;
    }

    // Set urls
    webdavclient_.call("setHostAndUser", QVariantList() << webdavIdentityUrl_ << webdavUrl_);
    // Connect to webdav
    webdavclient_.call("setupConnection");
    // Fetch root resources
    QStringList rootResources = webdavclient_.call("listResources").toStringList();
    if (rootResources.count() < 1)
    {
        ErrorOccurredCreateEmptyRootFolder();
        return;
    }

    QMap<QString, QString> folders;
    InventoryFolder *parentFolder;
    for (int index=0; index<=rootResources.count(); index++)
    {
        folders[rootResources.value(index)] = rootResources.value(index+1);
        index++;
    }

    if (!rootFolder_)
    {
        rootFolder_ = new InventoryFolder("root", "Webdav Inventory", false, 0);
        parentFolder = new InventoryFolder("", QString("My Inventory"), false, rootFolder_);
        rootFolder_->AddChild(parentFolder);
        rootFolder_->SetDirty(true);
    }

    AbstractInventoryItem *newItem = 0;
    QString path;
    QString name;
    QString type;

    for (QMap<QString, QString>::iterator iter = folders.begin(); iter!=folders.end(); ++iter)
    {
        path = iter.key();
        name = path.midRef(0, path.lastIndexOf("/")).toString();
        name = path.midRef(path.lastIndexOf("/")).toString();
        type = iter.value();
        if (name != "")
        {
            if (type == "resource")
                newItem = new InventoryAsset(path, "", name, parentFolder);
            else
                newItem = new InventoryFolder(path, name, parentFolder, true);
            parentFolder->AddChild(newItem);
        }
    }
}

void WebDavInventoryDataModel::ErrorOccurredCreateEmptyRootFolder()
{
    if (!rootFolder_)
        rootFolder_ = new InventoryFolder("root", "Error while fetching Webdav Inventory", false, 0);
    InventoryFolder *parentFolder = new InventoryFolder("/", QString("My Inventory"), false, rootFolder_);
    rootFolder_->AddChild(parentFolder);
    rootFolder_->SetDirty(true);
}

QString WebDavInventoryDataModel::ValidateFolderPath(QString path)
{
    if (!path.endsWith("/") && !path.isEmpty())
        path.append("/");

    return path;
}

}
