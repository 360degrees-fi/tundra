// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Communication_FriendHelper_h
#define incl_Communication_FriendHelper_h

#include "Foundation.h"
#include "ui_FriendListWidget.h"
#include "ui_RequestManagerWidget.h"

#include <QObject>
#include <QMap>
#include <QString>

#include "interface.h"

namespace UiHelpers
{
    class FriendHelper : public QObject
    {

    Q_OBJECT

    public:
        FriendHelper();
        virtual ~FriendHelper();

        //! Setters
        void SetupUi(Ui::FrienListWidget *friend_list_ui) { SAFE_DELETE(friend_list_ui_); friend_list_ui_ = friend_list_ui; }

        //! Getters
        QMap<QString, Communication::ContactInterface*> GetContactsMap() { return contacts_map_; }

    public slots:
        void NewFriendRequest(Communication::FriendRequestInterface& friend_request);
        void OnNewContact(const Communication::ContactInterface& contact);
        void OnContactRemoved(const Communication::ContactInterface& contact);

        void ShowRequestManagerWidget();
        void UpdateFriendList(QMap<QString, Communication::ContactInterface*> contacts_map);

    private slots:
        void CheckPendingRequestList();

    private:
        Ui::FrienListWidget *friend_list_ui_;
        Ui::RequestManagerWidget request_manager_ui_;

        QWidget *request_manager_widget_;
        QMap<QString, Communication::ContactInterface*> contacts_map_;
        QMap<QString, Communication::FriendRequestInterface*> pending_friend_requests_;

    };
}

#endif // incl_Communication_FriendHelper_h