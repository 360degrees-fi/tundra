// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   OpenSimInventoryDataModel.cpp
 *  @brief  Data model providing the OpenSim inventory model backend functionality.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "OpenSimInventoryDataModel.h"
#include "InventoryModule.h"
#include "InventoryFolder.h"
#include "InventoryAsset.h"
#include "J2kEncoder.h"

#include "UiModule.h"
#include "Inventory/InventorySkeleton.h"
#include "Inventory/InventoryEvents.h"
#include "AssetEvents.h"
#include "ResourceInterface.h"
#include "TextureServiceInterface.h"
#include "TextureInterface.h"
#include "AssetServiceInterface.h"
#include "HttpRequest.h"
#include "LLSDUtilities.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QStringList>
#include <QTime>

#include <OgreImage.h>
#include <OgreException.h>
//#include "MemoryLeakCheck.h"

namespace Inventory
{

OpenSimInventoryDataModel::OpenSimInventoryDataModel(
    Foundation::Framework *framework,
    ProtocolUtilities::InventorySkeleton *inventory_skeleton) :
    framework_(framework),
    rootFolder_(0),
    worldLibraryOwnerId_("")
{
    SetupModelData(inventory_skeleton);

    boost::shared_ptr<UiServices::UiModule> ui_module = framework_->GetModuleManager()->GetModule<UiServices::UiModule>(Foundation::Module::MT_UiServices).lock();
    if (ui_module.get())
        connect(this, SIGNAL(Notification(const QString &, int)), ui_module->GetNotificationManager(),SLOT(ShowInformationString(const QString &, int)));
}

OpenSimInventoryDataModel::~OpenSimInventoryDataModel()
{
    SAFE_DELETE(rootFolder_);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetFirstChildFolderByName(const QString &searchName) const
{
    return rootFolder_->GetFirstChildFolderByName(searchName);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetChildFolderById(const QString &searchId) const
{
    return rootFolder_->GetChildFolderById(searchId);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetChildAssetById(const QString &searchId) const
{
    return rootFolder_->GetChildAssetById(searchId);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetChildById(const QString &searchId) const
{
    return rootFolder_->GetChildById(searchId);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetRoot() const
{
    return rootFolder_;
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetTrashFolder() const
{
    return rootFolder_->GetFirstChildFolderByName("Trash");
}

InventoryFolder *OpenSimInventoryDataModel::GetMyInventoryFolder() const
{
    return rootFolder_->GetFirstChildFolderByName("My Inventory");
}

InventoryFolder *OpenSimInventoryDataModel::GetOpenSimLibraryFolder() const
{
    return rootFolder_->GetFirstChildFolderByName("OpenSim Library");
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetOrCreateNewFolder(
    const QString &id,
    AbstractInventoryItem &parentFolder,
    const QString &name,
    const bool &notify_server)
{
    InventoryFolder *parent = dynamic_cast<InventoryFolder *>(&parentFolder);
    if (!parent)
        return 0;

    // Return an existing folder if one with the given id is present.
    InventoryFolder *existing = dynamic_cast<InventoryFolder *>(parent->GetChildFolderById(id));
    if (existing)
        return existing;

    // Create a new folder.
    InventoryFolder *newFolder = new InventoryFolder(id, name, parent);

    if (GetOpenSimLibraryFolder())
        if (parent->IsDescendentOf(GetOpenSimLibraryFolder()))
            newFolder->SetIsLibraryItem(true);

    // Inform the server.
    // We don't want to notify server if we're creating folders "ordered" by server via InventoryDescecendents packet.
    if (notify_server)
        currentWorldStream_->SendCreateInventoryFolderPacket(
            RexUUID(parent->GetID().toStdString()), RexUUID(newFolder->GetID().toStdString()),
            255, newFolder->GetName().toStdString().c_str());

    return parent->AddChild(newFolder);
}

AbstractInventoryItem *OpenSimInventoryDataModel::GetOrCreateNewAsset(
    const QString &inventory_id,
    const QString &asset_id,
    AbstractInventoryItem &parentFolder,
    const QString &name)
{
    InventoryFolder *parent = static_cast<InventoryFolder *>(&parentFolder);
    if (!parent)
        return 0;

    // Return an existing asset if one with the given id is present.
    InventoryAsset *existing = dynamic_cast<InventoryAsset *>(parent->GetChildAssetById(inventory_id));
    if (existing)
        return existing;

    // Create a new asset.
    InventoryAsset *newAsset = new InventoryAsset(inventory_id, asset_id, name, parent);

    if (parent->IsDescendentOf(GetOpenSimLibraryFolder()))
        newAsset->SetIsLibraryItem(true);

    return parent->AddChild(newAsset);
}

bool OpenSimInventoryDataModel::FetchInventoryDescendents(AbstractInventoryItem *item)
{
    if (item->GetItemType() != AbstractInventoryItem::Type_Folder)
        return false;

    ///\note    Due to some server-side mystery behaviour we must send the same packet twice: once
    ///         with fetch_folders = true & fetch_items = false and once with fetch_folders = false & fetch_items = true
    ///         in order to reveice the inventory item information correctly (asset&inventory types at least).
    if (item->IsDescendentOf(GetOpenSimLibraryFolder()))
    {
        currentWorldStream_->SendFetchInventoryDescendentsPacket(QSTR_TO_UUID(item->GetID()),
            QSTR_TO_UUID(worldLibraryOwnerId_), 0, true, false);
        currentWorldStream_->SendFetchInventoryDescendentsPacket(QSTR_TO_UUID(item->GetID()),
            QSTR_TO_UUID(worldLibraryOwnerId_), 0, false, true);
    }
    else
    {
        currentWorldStream_->SendFetchInventoryDescendentsPacket(QSTR_TO_UUID(item->GetID()), RexUUID(), 0, true, false);
        currentWorldStream_->SendFetchInventoryDescendentsPacket(QSTR_TO_UUID(item->GetID()), RexUUID(), 0, false, true);
    }

    return true;
}

void OpenSimInventoryDataModel::NotifyServerAboutItemMove(AbstractInventoryItem *item)
{
    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
        currentWorldStream_->SendMoveInventoryFolderPacket(QSTR_TO_UUID(item->GetID()),
            QSTR_TO_UUID(item->GetParent()->GetID()));

    if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
        currentWorldStream_->SendMoveInventoryItemPacket(QSTR_TO_UUID(item->GetID()),
            QSTR_TO_UUID(item->GetParent()->GetID()), item->GetName().toStdString());
}

void OpenSimInventoryDataModel::NotifyServerAboutItemCopy(AbstractInventoryItem *item)
{
    if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
        currentWorldStream_->SendCopyInventoryItemPacket(QSTR_TO_UUID(worldLibraryOwnerId_),
            QSTR_TO_UUID(item->GetID()), QSTR_TO_UUID(item->GetParent()->GetID()), item->GetName().toStdString());
}

void OpenSimInventoryDataModel::NotifyServerAboutItemRemove(AbstractInventoryItem *item)
{
    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
        currentWorldStream_->SendRemoveInventoryFolderPacket(QSTR_TO_UUID(item->GetID()));

    if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
        currentWorldStream_->SendRemoveInventoryItemPacket(QSTR_TO_UUID(item->GetID()));
}

void OpenSimInventoryDataModel::NotifyServerAboutItemUpdate(AbstractInventoryItem *item, const QString &old_name)
{
    if (item->GetItemType() == AbstractInventoryItem::Type_Folder)
        currentWorldStream_->SendUpdateInventoryFolderPacket(QSTR_TO_UUID(item->GetID()),
            QSTR_TO_UUID(item->GetParent()->GetID()), 127, item->GetName().toStdString());

    if (item->GetItemType() == AbstractInventoryItem::Type_Asset)
    {
        InventoryAsset *asset = static_cast<InventoryAsset *>(item);
        currentWorldStream_->SendUpdateInventoryItemPacket(QSTR_TO_UUID(asset->GetID()),
            QSTR_TO_UUID(asset->GetParent()->GetID()), asset->GetAssetType(), asset->GetInventoryType(),
            asset->GetName().toStdString(), asset->GetDescription().toStdString());
    }
}

bool OpenSimInventoryDataModel::OpenItem(AbstractInventoryItem *item)
{
    using namespace Foundation;

    InventoryAsset *asset = dynamic_cast<InventoryAsset *>(item);
    if (!asset)
        return false;

    // Get asset service interface.
    ServiceManagerPtr service_manager = framework_->GetServiceManager();
    if (!service_manager->IsRegistered(Foundation::Service::ST_Asset))
    {
        InventoryModule::LogError("Asset service doesn't exist.");
        return false;
    }

    boost::shared_ptr<Foundation::AssetServiceInterface> asset_service = 
        service_manager->GetService<Foundation::AssetServiceInterface>(Foundation::Service::ST_Asset).lock();

    // Get event manager.
    EventManagerPtr event_mgr = framework_->GetEventManager();
    event_category_id_t event_category = event_mgr->QueryEventCategory("Inventory");
    if (event_category == 0)
        return false;

    std::string asset_id = asset->GetAssetReference().toStdString();
    asset_type_t asset_type = asset->GetAssetType();
    request_tag_t tag = 0;

    // Check out if the asset already exists in the cache.
    Foundation::AssetPtr assetPtr = asset_service->GetAsset(asset_id, GetTypeNameFromAssetType(asset_type));
    if(assetPtr.get() && assetPtr->GetSize() > 0)
    {
        // Send InventoryItemDownloadedEventData event.
        InventoryItemDownloadedEventData itemDownloaded;
        itemDownloaded.inventoryId = QSTR_TO_UUID(asset->GetID());
        itemDownloaded.asset = assetPtr;
        itemDownloaded.requestTag = tag;
        itemDownloaded.assetType = asset_type;
        itemDownloaded.name = asset->GetName().toStdString();
        event_mgr->SendEvent(event_category, Inventory::Events::EVENT_INVENTORY_ITEM_DOWNLOADED, &itemDownloaded);

        ///\todo Show basic info dialog. Name, desc, file size etc.
        //if (!itemDownloaded.handled)
    }
    else
    {
        // If not, request asset from asset system.
        switch(asset_type)
        {
        case RexAT_MaterialScript:
        case RexAT_ParticleScript:
            tag = asset_service->RequestAsset(asset_id, GetTypeNameFromAssetType(asset_type));
            break;
        case RexAT_Texture:
        case RexAT_SoundVorbis:
        case RexAT_SoundWav:
        case RexAT_Mesh:
        case RexAT_Skeleton:
        case RexAT_GenericAvatarXml:
        case RexAT_FlashAnimation:
            InventoryModule::LogError("Non-supported asset type for opening: " + GetTypeNameFromAssetType(asset_type));
            break;
        case RexAT_None:
        default:
            InventoryModule::LogError("Invalid asset type for opening.");
            break;
        }

        if (!tag)
            return false;

        // Send InventoryItemOpen event.
        InventoryItemOpenEventData itemOpen;
        itemOpen.requestTag = tag;
        itemOpen.inventoryId = QSTR_TO_UUID(asset->GetID());
        itemOpen.assetId = QSTR_TO_UUID(asset->GetAssetReference());
        itemOpen.assetType = asset_type;
        itemOpen.inventoryType = asset->GetInventoryType();
        itemOpen.name = asset->GetName().toStdString();
        event_mgr->SendEvent(event_category, Inventory::Events::EVENT_INVENTORY_ITEM_OPEN, &itemOpen);

        ///\todo Open a generic download progress dialog if no handler found.
        if (!itemOpen.overrideDefaultHandler)
        {
            emit DownloadStarted(asset_id.c_str(), asset->GetName());
            QPair<request_tag_t, QString> key = qMakePair(tag, asset->GetAssetReference());
            openRequests_[key] = asset->GetName();
        }
    }

    return true;
}

void OpenSimInventoryDataModel::UploadFile(const QString &filename, AbstractInventoryItem *parent_folder)
{
    CreateRexInventoryFolders();

    if (!HasUploadCapability())
    {
        std::string upload_url = currentWorldStream_->GetCapability("NewFileAgentInventory");
        if (upload_url == "")
        {
            InventoryModule::LogError("Could not get upload capability for asset uploader. Uploading not possible");
            return;
        }

        SetUploadCapability(upload_url);
    }

    QStringList filenames, names;
    filenames << filename;
    Thread thread(boost::bind(&OpenSimInventoryDataModel::ThreadedUploadFiles, this, filenames, names));
}

void OpenSimInventoryDataModel::UploadFiles(QStringList &filenames, QStringList &names, AbstractInventoryItem *parent_folder)
{
    CreateRexInventoryFolders();

    if (!HasUploadCapability())
    {
        std::string upload_url = currentWorldStream_->GetCapability("NewFileAgentInventory");
        if (upload_url == "")
        {
            InventoryModule::LogError("Could not get upload capability for asset uploader. Uploading not possible");
            return;
        }

        SetUploadCapability(upload_url);
    }

    emit MultiUploadStarted(filenames.size());
    Thread thread(boost::bind(&OpenSimInventoryDataModel::ThreadedUploadFiles, this, filenames, names));
}

void OpenSimInventoryDataModel::UploadFilesFromBuffer(QStringList &filenames, QVector<QVector<uchar> > &buffers,
    AbstractInventoryItem *parent_folder)
{
    CreateRexInventoryFolders();

    if (!HasUploadCapability())
    {
        std::string upload_url = currentWorldStream_->GetCapability("NewFileAgentInventory");
        if (upload_url == "")
        {
            InventoryModule::LogError("Could not get upload capability for asset uploader. Uploading not possible");
            return;
        }

        SetUploadCapability(upload_url);
    }

    Thread thread(boost::bind(&OpenSimInventoryDataModel::ThreadedUploadBuffers, this, filenames, buffers));
}

void OpenSimInventoryDataModel::DownloadFile(const QString &store_folder, AbstractInventoryItem *selected_item)
{
    using namespace Foundation;

    InventoryAsset *asset = dynamic_cast<InventoryAsset *>(selected_item);
    if (!asset)
        return;

    std::string id = asset->GetAssetReference().toStdString();
    asset_type_t asset_type = asset->GetAssetType();

    // Create full filename.
    QString fullFilename = store_folder; 
    fullFilename += "/";
    fullFilename += asset->GetName();
    fullFilename += QString(RexTypes::GetFileExtensionFromAssetType(asset_type).c_str());

    ServiceManagerPtr service_manager = framework_->GetServiceManager();
    switch(asset_type)
    {
    case RexAT_Texture:
    case RexAT_SoundVorbis:
    case RexAT_SoundWav:
    case RexAT_Mesh:
    case RexAT_Skeleton:
    case RexAT_MaterialScript:
    case RexAT_ParticleScript:
    case RexAT_GenericAvatarXml:
    case RexAT_FlashAnimation:
    {
        // Request assets from asset service.
        if (service_manager->IsRegistered(Foundation::Service::ST_Asset))
        {
            boost::shared_ptr<Foundation::AssetServiceInterface> asset_service = 
                service_manager->GetService<Foundation::AssetServiceInterface>(Foundation::Service::ST_Asset).lock();

            request_tag_t tag = asset_service->RequestAsset(id, GetTypeNameFromAssetType(asset_type));
            if (tag)
            {
                QPair<request_tag_t, QString> key = qMakePair(tag, asset->GetAssetReference());
                downloadRequests_[key] = fullFilename;
                emit DownloadStarted(id.c_str(), asset->GetName());
            }
        }
        break;
    }
    case RexAT_None:
    default:
        InventoryModule::LogError("Invalid asset type for download.");
        break;
    }
}

/*
void OpenSimInventoryDataModel::HandleResourceReady(Foundation::EventDataInterface *data)
{
    ///\todo    It seems that we don't necessarily need to handle ResourceReady.
    ///         Ogre seems to be able to create files from AssetReady events.

    Resource::Events::ResourceReady* resourceReady = checked_static_cast<Resource::Events::ResourceReady *>(data);
    RexTypes::asset_type_t asset_type = RexAT_Texture;
    request_tag_t tag = resourceReady->tag_;
//    QString asset_id = resourceReady->id_.c_str();

    AssetRequestMap::iterator i = downloadRequests_.find(qMakePair(tag, asset_type));
    if (i == downloadRequests_.end())
        return;

    Foundation::TextureInterface *tex = dynamic_cast<Foundation::TextureInterface *>(resourceReady->resource_.get());
    if (!tex || tex->GetLevel() != 0)
        return;

///\todo Use QImage?
//    QImage img = QImage::fromData(tex->GetData(), size);
//    img.save(i.value());

    Ogre::Image image;
    try
    {
        uint size = tex->GetWidth() * tex->GetHeight() * tex->GetComponents();
        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)tex->GetData(), size, false));
        image.load(stream);
        image.save(i.value().toStdString());
    }
    catch(Ogre::Exception &ex)
    {
        InventoryModule::LogError("Could not create image file " + i.value().toStdString() + ". Reason: " + ex.what() + ".");
        downloadRequests_.erase(i);
        return;
    }

    InventoryModule::LogInfo("File " + i.value().toStdString() + " succesfully saved.");

    downloadRequests_.erase(i);
}
*/

void OpenSimInventoryDataModel::HandleAssetReadyForDownload(Foundation::EventDataInterface *data)
{
    Asset::Events::AssetReady *assetReady = checked_static_cast<Asset::Events::AssetReady*>(data);
    request_tag_t tag = assetReady->tag_;
    asset_type_t asset_type = RexTypes::GetAssetTypeFromTypeName(assetReady->asset_type_);

    QPair<request_tag_t, QString> key = qMakePair(tag, STD_TO_QSTR(assetReady->asset_id_));
    AssetRequestMap::iterator i = downloadRequests_.find(key);
    if (i == downloadRequests_.end())
        return;

    emit DownloadCompleted(assetReady->asset_id_.c_str());

    Foundation::AssetPtr asset = assetReady->asset_;
    if (asset_type == RexAT_Texture)
    {
        Ogre::Image image;
        try
        {
            Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)asset->GetData(), asset->GetSize(), false));
            image.load(stream);
            image.save(i.value().toStdString());
        }
        catch(Ogre::Exception &ex)
        {
            InventoryModule::LogError("Could not create image file " + i.value().toStdString() + ". Reason: " + ex.what() + ".");
            downloadRequests_.erase(i);
            return;
        }
    }
    else
    {
        QFile file(i.value());
        file.open(QIODevice::WriteOnly);
        file.write((const char*)asset->GetData(), asset->GetSize());
        file.close();
    }

    QString real_path;
    QString real_filename = i.value();
    int last_sep_index = real_filename.lastIndexOf(QDir::separator());
    // There is something fishy here, sometimes the paths (in filename, should not happen!) include 
    // unix separators even when uploading from windows. Lets do a fallback here
    if (last_sep_index != -1)
    {
        real_path = real_filename.midRef(0, last_sep_index).toString();
        real_filename = real_filename.midRef(last_sep_index+1).toString();
    }
    else
    {
        last_sep_index = real_filename.lastIndexOf("/");
        real_path = real_filename.midRef(0, last_sep_index).toString();
        real_filename = real_filename.midRef(last_sep_index+1).toString();
    }

    emit Notification(QString("%1 downloaded succesfully to %2").arg(real_filename, real_path), 9000);
    InventoryModule::LogInfo("File " + i.value().toStdString() + " succesfully saved.");

    downloadRequests_.erase(i);
}

void OpenSimInventoryDataModel::HandleAssetReadyForOpen(Foundation::EventDataInterface *data)
{
    Asset::Events::AssetReady *assetReady = checked_static_cast<Asset::Events::AssetReady*>(data);
    request_tag_t tag = assetReady->tag_;
    asset_type_t asset_type = RexTypes::GetAssetTypeFromTypeName(assetReady->asset_type_);

    QPair<request_tag_t, QString> key = qMakePair(tag, STD_TO_QSTR(assetReady->asset_id_));
    AssetRequestMap::iterator i = openRequests_.find(key);
    if (i == openRequests_.end())
        return;

    emit DownloadCompleted(assetReady->asset_id_.c_str());

    // Send InventoryItemDownloaded event.
    Foundation::EventManagerPtr event_mgr = framework_->GetEventManager();
    event_category_id_t event_category = event_mgr->QueryEventCategory("Inventory");
    if (event_category == 0)
        return;

    InventoryItemDownloadedEventData itemDownloaded;
    itemDownloaded.inventoryId = QSTR_TO_UUID(i.value());
    itemDownloaded.asset = assetReady->asset_;
    itemDownloaded.requestTag = tag;
    itemDownloaded.assetType = asset_type;
    itemDownloaded.name = i.value().toStdString();
    event_mgr->SendEvent(event_category, Inventory::Events::EVENT_INVENTORY_ITEM_DOWNLOADED, &itemDownloaded);

    ///\todo If no asset editor module handled the above event, show the generic editor window for the asset.
    //if (!itemDownloaded.handled)

    openRequests_.erase(i);
}

void OpenSimInventoryDataModel::HandleInventoryDescendents(Foundation::EventDataInterface *data)
{
    InventoryItemEventData *item_data = checked_static_cast<InventoryItemEventData *>(data);

    AbstractInventoryItem *parentFolder = GetChildFolderById(STD_TO_QSTR(item_data->parentId.ToString()));
    if (!parentFolder)
        return;

    AbstractInventoryItem *existing = GetChildById(STD_TO_QSTR(item_data->id.ToString()));
    if (existing)
        return;

    if (item_data->item_type == IIT_Folder)
    {
        InventoryFolder *newFolder = static_cast<InventoryFolder *>(GetOrCreateNewFolder(
            STD_TO_QSTR(item_data->id.ToString()), *parentFolder, false));

        newFolder->SetName(STD_TO_QSTR(item_data->name));
        ///\todo newFolder->SetType(item_data->type);
        newFolder->SetDirty(true);
    }
    if (item_data->item_type == IIT_Asset)
    {
        InventoryAsset *newAsset = static_cast<InventoryAsset *>(GetOrCreateNewAsset(
            STD_TO_QSTR(item_data->id.ToString()), STD_TO_QSTR(item_data->assetId.ToString()),
            *parentFolder, STD_TO_QSTR(item_data->name)));

        newAsset->SetDescription(STD_TO_QSTR(item_data->description));
        newAsset->SetInventoryType(item_data->inventoryType);
        newAsset->SetAssetType(item_data->assetType);
    }
}

bool OpenSimInventoryDataModel::UploadBuffer(
    const RexTypes::asset_type_t &asset_type,
    const std::string& filename,
    const std::string& name,
    const std::string& description,
    const RexUUID& folder_id,
    const QVector<uchar>& buffer)
{
    if (uploadCapability_ == "")
    {
        InventoryModule::LogError("Upload capability not set! Uploading not possible.");
        return false;
    }

    // Create the asset uploading info XML message.
    std::string it_str = RexTypes::GetInventoryTypeString(asset_type);
    std::string at_str = RexTypes::GetAssetTypeString(asset_type);
    std::string asset_xml = CreateNewFileAgentInventoryXML(at_str, it_str, folder_id.ToString(), name, description);

    // Post NewFileAgentInventory message informing the server about upcoming asset upload.
    HttpUtilities::HttpRequest request;
    request.SetUrl(uploadCapability_);
    request.SetMethod(HttpUtilities::HttpRequest::Post);
    request.SetRequestData("application/xml", asset_xml);
    request.Perform();

    if (!request.GetSuccess())
    {
        InventoryModule::LogError(request.GetReason());
        return false;
    }

    std::vector<u8> response = request.GetResponseData();
    if (response.size() == 0)
    {
        InventoryModule::LogError("Size of the response data to \"NewFileAgentInventory\" message was zero.");
        return false;
    }

    response.push_back('\0');
    std::string response_str = (char *)&response[0];

    // Parse the upload url from the response.
    std::map<std::string, std::string> llsd_map = RexTypes::ParseLLSDMap(response_str);
    std::string upload_url = llsd_map["uploader"];
    if (upload_url == "")
    {
        InventoryModule::LogError("Invalid response data for uploading an asset.");
        return false;
    }

    HttpUtilities::HttpRequest request2;
    request2.SetUrl(upload_url);
    request2.SetMethod(HttpUtilities::HttpRequest::Post);

    // If the file is texture, use Ogre image and J2k encoding.
    if (asset_type == RexTypes::RexAT_Texture)
    {
        Ogre::Image image;
        try
        {
            Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)&buffer[0], buffer.size(), false));
            image.load(stream);
        }
        catch (Ogre::Exception &e)
        {
            InventoryModule::LogError("Error loading image: " + std::string(e.what()));
            return false;
        }

        std::vector<u8> encoded_buffer;
        bool success = J2k::J2kEncode(image, encoded_buffer, false);
        if (!success)
        {
            InventoryModule::LogError("Could not J2k encode the image file.");
            return false;
        }
        
        request2.SetRequestData("application/octet-stream", encoded_buffer);
    }
    else
    {
        // Other assets can be uploaded as raw data.
        request2.SetRequestData("application/octet-stream", buffer.toStdVector());
    }

    response.clear();
    response_str.clear();

    request2.Perform();
    
    if (!request2.GetSuccess())
    {
        InventoryModule::LogError("HTTP POST asset upload did not succeed: " + request.GetReason());
        return false;
    }

    response = request2.GetResponseData();

    if (response.size() == 0)
    {
        InventoryModule::LogError("Size of the response data to file upload was zero.");
        return false;
    }

    response.push_back('\0');
    response_str = (char *)&response[0];
    llsd_map.clear();

    llsd_map = RexTypes::ParseLLSDMap(response_str);
    std::string asset_id = llsd_map["new_asset"];
    std::string inventory_id = llsd_map["new_inventory_item"];
    if (asset_id == "" || inventory_id == "")
    {
        InventoryModule::LogError("Invalid XML response data for uploading an asset.");
        return false;
    }

    // Send event, if applicable.
    // Note: sent as delayed, to be thread-safe
    Foundation::EventManagerPtr event_mgr = framework_->GetEventManager();
    event_category_id_t event_category = event_mgr->QueryEventCategory("Inventory");
    if (event_category != 0)
    {
        boost::shared_ptr<InventoryItemEventData> asset_data(new InventoryItemEventData(IIT_Asset));
        asset_data->id = RexUUID(inventory_id);
        asset_data->parentId = folder_id;
        asset_data->assetId = RexUUID(asset_id);
        asset_data->assetType = asset_type;
        asset_data->inventoryType = RexTypes::GetInventoryTypeFromAssetType(asset_type);
        asset_data->name = name;
        asset_data->description = description;
        asset_data->fileName = filename;

        event_mgr->SendDelayedEvent<InventoryItemEventData>(event_category, Inventory::Events::EVENT_INVENTORY_DESCENDENT, asset_data);
    }

    InventoryModule::LogInfo("Upload succesfull. Asset id: " + asset_id + ", inventory id: " + inventory_id + ".");
    return true;
}

bool OpenSimInventoryDataModel::UploadFile(
    const RexTypes::asset_type_t &asset_type,
    std::string &filename,
    const std::string &name,
    const std::string &description,
    const RexUUID &folder_id)
{
    if (!HasUploadCapability())
    {
        std::string upload_url = currentWorldStream_->GetCapability("NewFileAgentInventory");
        if (upload_url == "")
        {
            InventoryModule::LogError("Could not get upload capability for asset uploader. Uploading not possible");
            return false;
        }

        SetUploadCapability(upload_url);
    }

    // Open the file.
#ifdef Q_WS_WIN
    // Remove leading '/' on Windows environment, if it exists.
    if (filename.find('/',0) == 0)
        filename.erase(0, 1);
#endif
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        InventoryModule::LogError("Could not open the file: " + filename + ".");
        return false;
    }

    QVector<uchar> buffer;

    std::filebuf *pbuf = file.rdbuf();
    size_t size = pbuf->pubseekoff(0, std::ios::end, std::ios::in);
    buffer.resize(size);
    pbuf->pubseekpos(0, std::ios::in);
    pbuf->sgetn((char *)&buffer[0], size);
    file.close();

    return UploadBuffer(asset_type, filename, name, description, folder_id, buffer);
}

QString OpenSimInventoryDataModel::CreateNameFromFilename(QString filename)
{
    std::string name = "asset";
    bool no_path = false;
    size_t name_start_pos = filename.toStdString().find_last_of('/');
    // If the filename doesn't have the full path, start from index 0.
    if (name_start_pos == std::string::npos)
    {
        name_start_pos = 0;
        no_path = true;
    }

    size_t name_end_pos = filename.toStdString().find_last_of('.');
    if (name_end_pos != std::string::npos)
    {
        if (no_path)
            name = filename.toStdString().substr(name_start_pos, name_end_pos - name_start_pos);
        else
            name = filename.toStdString().substr(name_start_pos + 1, name_end_pos - name_start_pos - 1);
    }

    return name.c_str();
}

void OpenSimInventoryDataModel::CreateNewFolderFromFolderSkeleton(
    InventoryFolder *parent_folder,
    ProtocolUtilities::InventoryFolderSkeleton *folder_skeleton)
{
    using namespace ProtocolUtilities;

    InventoryFolder *newFolder = new InventoryFolder(STD_TO_QSTR(folder_skeleton->id.ToString()),
        STD_TO_QSTR(folder_skeleton->name), parent_folder, folder_skeleton->editable);
    //if (!folder_skeleton->HasChildren())
    newFolder->SetDirty(true);

    if (!rootFolder_ && !parent_folder)
        rootFolder_ = newFolder;

    if (parent_folder)
    {
        parent_folder->AddChild(newFolder);

        if (newFolder == GetOpenSimLibraryFolder())
            newFolder->SetIsLibraryItem(true);

        // Flag Library folders. They have some special behavior.
        if (GetOpenSimLibraryFolder())
            if (newFolder->IsDescendentOf(GetOpenSimLibraryFolder()))
                newFolder->SetIsLibraryItem(true);
    }

    InventoryFolderSkeleton::FolderIter iter = folder_skeleton->children.begin();
    while(iter != folder_skeleton->children.end())
    {
        parent_folder = newFolder;
        CreateNewFolderFromFolderSkeleton(parent_folder, &*iter);
        ++iter;
    }
}

void OpenSimInventoryDataModel::SetupModelData(ProtocolUtilities::InventorySkeleton *inventory_skeleton)
{
    if (!inventory_skeleton && !inventory_skeleton->GetRoot())
    {
        InventoryModule::LogError("Couldn't find inventory root folder skeleton. Can't create OpenSim inventory data model.");
        return;
    }

    worldLibraryOwnerId_ = STD_TO_QSTR(inventory_skeleton->worldLibraryOwnerId.ToString());

    CreateNewFolderFromFolderSkeleton(0, inventory_skeleton->GetRoot());
}

void OpenSimInventoryDataModel::ThreadedUploadFiles(QStringList &filenames, QStringList &item_names)
{
    // For notifications
    QTime timer;

    // Iterate trought every asset.
    int asset_count = 0;
    QStringList::iterator name_it = item_names.begin();
    for(QStringList::iterator it = filenames.begin(); it != filenames.end(); ++it)
    {
        QString filename = *it;
        QString real_filename = filename;
        real_filename = real_filename.midRef(real_filename.lastIndexOf(QDir::separator())+1).toString();

        emit UploadStarted(real_filename);

        RexTypes::asset_type_t asset_type = RexTypes::GetAssetTypeFromFilename(filename.toStdString());
        if (asset_type == RexAT_None)
        {
            emit Notification(QString("Could not upload %1 Invalid file type").arg(real_filename), 9000);
            emit UploadFailed(real_filename);
            InventoryModule::LogError("Invalid file extension. File can't be uploaded: " + filename.toStdString());
            continue;
        }

        // Create the asset uploading info XML message.
        std::string it_str = RexTypes::GetInventoryTypeString(asset_type);
        std::string at_str = RexTypes::GetAssetTypeString(asset_type);
        std::string cat_name = RexTypes::GetCategoryNameForAssetType(asset_type);
        
        ///\todo User-defined name and desc when we got the UI.
        QString name;
        if (name_it != item_names.end())
        {
            name = *name_it;
            ++name_it;
        }
        else
            name = CreateNameFromFilename(filename);

        std::string description = "(No Description)";

        RexUUID folder_id(GetFirstChildFolderByName(cat_name.c_str())->GetID().toStdString());
        if (folder_id.IsNull())
        {
            InventoryModule::LogError("Inventory folder for this type of file doesn't exists. File can't be uploaded.");
            continue;
        }

        // Start notification
        emit Notification(QString("Uploading %1").arg(filename), 9000);
        timer.start();

        if (UploadFile(asset_type, filename.toStdString(), name.toStdString(), description, folder_id))
        {
            // Send succesfull notification
            qreal elapsed(timer.elapsed());
            emit Notification(QString("%1 uploaded succesfully to %2 in %3 seconds").arg(
                real_filename, QString::fromStdString(cat_name), QString::number(elapsed/1000, 102, 2)), 9000);
            emit UploadCompleted(real_filename);
            ++asset_count;
        }
        else
        {
            // Send fail notification
            emit Notification(QString("Upload failed for %1").arg(real_filename), 9000);
            emit UploadFailed(real_filename);
        }
    }

    emit MultiUploadCompleted();
    InventoryModule::LogInfo("Multiupload:" + ToString(asset_count) + " assets succesfully uploaded.");
}

void OpenSimInventoryDataModel::ThreadedUploadBuffers(QStringList filenames, QVector<QVector<uchar> > buffers)
{
    if (filenames.size() != buffers.size())
    {
        InventoryModule::LogError("Not as many data buffers as filenames!");
        return;
    }

    // Iterate trought every asset.
    int asset_count = 0;
    QVector<QVector<uchar> >::iterator it2 = buffers.begin();

    QStringListIterator it(filenames);
    while(it.hasNext())
    {
        QString filename = it.next();
        RexTypes::asset_type_t asset_type = RexTypes::GetAssetTypeFromFilename(filename.toStdString());
        if (asset_type == RexAT_None)
        {
            InventoryModule::LogError("Invalid file extension. File can't be uploaded: " + filename.toStdString());
            continue;
        }

        // Create the asset uploading info XML message.
        std::string it_str = RexTypes::GetInventoryTypeString(asset_type);
        std::string at_str = RexTypes::GetAssetTypeString(asset_type);
        std::string cat_name = RexTypes::GetCategoryNameForAssetType(asset_type);

        ///\todo User-defined name and desc when we got the UI.
        QString name = CreateNameFromFilename(filename);
        std::string description = "(No Description)";

        RexUUID folder_id(GetFirstChildFolderByName(cat_name.c_str())->GetID().toStdString());
        if (folder_id.IsNull())
        {
            InventoryModule::LogError("Inventory folder for this type of file doesn't exists. File can't be uploaded.");
            continue;
        }

        if (UploadBuffer(asset_type, filename.toStdString(), name.toStdString(), description, folder_id, *it2))
            ++asset_count;

        ++it2;
    }

    InventoryModule::LogInfo("Multiupload:" + ToString(asset_count) + " assets succesfully uploaded.");
}

std::string OpenSimInventoryDataModel::CreateNewFileAgentInventoryXML(
    const std::string &asset_type,
    const std::string &inventory_type,
    const std::string &folder_id,
    const std::string &name,
    const std::string &description)
{
    std::string xml = "<llsd><map><key>asset_type</key><string>";
    xml += asset_type;
    xml += "</string><key>description</key><string>";
    xml += description;
    xml += "</string><key>folder_id</key><uuid>";
    xml += folder_id;
    xml += "</uuid><key>inventory_type</key><string>";
    xml += inventory_type;
    xml += "</string><key>name</key><string>";
    xml += name;
    xml += "</string></map></llsd>";
    return xml;
}

void OpenSimInventoryDataModel::CreateRexInventoryFolders()
{
    using namespace RexTypes;

    const char *asset_types[] = { "Texture", "Mesh", "Skeleton", "MaterialScript", "ParticleScript", "FlashAnimation", "GenericAvatarXml" };
    asset_type_t asset_type;
    for(int i = 0; i < NUMELEMS(asset_types); ++i)
    {
        asset_type = GetAssetTypeFromTypeName(asset_types[i]);
        std::string cat_name = GetCategoryNameForAssetType(asset_type);

        // Check out if this inventory category exists. If not, create id.
        if (!GetFirstChildFolderByName(STD_TO_QSTR(cat_name)))
            AbstractInventoryItem *newFolder = GetOrCreateNewFolder(
                QString(RexUUID::CreateRandom().ToString().c_str()), *GetMyInventoryFolder(), STD_TO_QSTR(cat_name));
    }
}

} // namespace Inventory
