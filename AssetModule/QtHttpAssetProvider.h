// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Asset_QtHttpAssetProvider_h
#define incl_Asset_QtHttpAssetProvider_h

#include "Foundation.h"
#include "AssetModuleApi.h"
#include "IAssetProvider.h"
#include "QtHttpAssetTransfer.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QMap>
#include <QPair>
#include <QUrl>

namespace Asset
{
    class ASSET_MODULE_API QtHttpAssetProvider : public QObject, public IAssetProvider
    {

    Q_OBJECT

    public:
        QtHttpAssetProvider(Foundation::Framework *framework);
        virtual ~QtHttpAssetProvider();

        void SetGetTextureCap(std::string url);
        void ClearAllTransfers();

        //! Interface implementation
        void Update(f64 frametime);

        const QString &Name();
        bool IsValidRef(QString assetRef, QString assetType);
        
        bool RequestAsset(const std::string& asset_id, const std::string& asset_type, request_tag_t tag);

        virtual AssetTransferPtr RequestAsset(QString assetRef, QString assetType);
        virtual std::vector<AssetStoragePtr> GetStorages() const;
        virtual IAssetUploadTransfer *UploadAssetFromFile(const char *filename, AssetStoragePtr destination, const char *assetName);
        virtual IAssetUploadTransfer *UploadAssetFromFileInMemory(const u8 *data, size_t numBytes, AssetStoragePtr destination, const char *assetName);

        bool InProgress(const std::string& asset_id);
        bool QueryAssetStatus(const std::string& asset_id, uint& size, uint& received, uint& received_continuous);

        Foundation::AssetInterfacePtr GetIncompleteAsset(const std::string& asset_id, const std::string& asset_type, uint received);
        Foundation::AssetTransferInfoVector GetTransferInfo();
    
    private slots:
        QUrl CreateUrl(QString assed_id);
        void TranferCompleted(QNetworkReply *reply);
        void RemoveFinishedTransfers(QString asset_transfer_key, QUrl metadata_transfer_key);
        void StartTransferFromQueue();

        bool CheckRequestQueue(QString assed_id);
        bool IsAcceptableAssetType(const std::string& asset_type);

    private:
        Foundation::Framework *framework_;
        EventManager *event_manager_;
        QString name_;
        QNetworkAccessManager *network_manager_;
        
        event_category_id_t asset_event_category_;
        f64 asset_timeout_;

        QMap<QString, QtHttpAssetTransfer *> assetid_to_transfer_map_;
        QMap<QUrl, QPair<HttpAssetTransferInfo, Foundation::AssetInterfacePtr> > metadata_to_assetptr_;
        QList<QtHttpAssetTransfer *> pending_request_queue_;

        bool filling_stack_;
        bool fake_metadata_fetch_;
        QUrl fake_metadata_url_;

        QUrl get_texture_cap_;

    };
}

#endif
