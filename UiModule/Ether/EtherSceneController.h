// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_EtherSceneController_h
#define incl_UiModule_EtherSceneController_h

#include <QObject>
#include <QGraphicsScene>

#include "View/EtherScene.h"
#include "View/VerticalMenu.h"

#include "Data/DataManager.h"
#include "Data/AvatarInfo.h"
#include "Data/WorldInfo.h"

#include "View/InfoCard.h"
#include "View/ControlProxyWidget.h"
#include "View/ActionProxyWidget.h"

namespace Ether
{
    namespace Logic
    {
        class EtherSceneController : public QObject
        {

        Q_OBJECT

        public:
            EtherSceneController(QObject *parent, Data::DataManager *data_manager, View::EtherScene *scene, QPair<View::EtherMenu*, View::EtherMenu*> menus,
                                 QRectF card_size, int top_items, int bottom_items);

        public slots:
            void LoadActionWidgets();
            void LoadAvatarCardsToScene(QMap<QUuid, View::InfoCard*> avatar_map, int visible_top_items, bool add_to_scene);
            void NewAvatarToScene(View::InfoCard *new_card, QMap<QUuid, View::InfoCard*> avatar_map, int visible_top_items);
            void LoadWorldCardsToScene(QMap<QUuid, View::InfoCard*> world_map, int visible_bottom_items,  bool add_to_scene);
            void NewWorldToScene(View::InfoCard *new_card, QMap<QUuid, View::InfoCard*> world_map, int visible_bottom_items);

            void UpPressed();
            void DownPressed();
            void LeftPressed() { active_menu_->moveLeft(); }
            void RightPressed() { active_menu_->moveRight(); }
            void TryStartLogin();
            void ItemActivatedWithMouse(View::InfoCard *clicked_item);

            void ActionWidgetInProgress(bool action_ongoing);

            void UpdateAvatarInfoWidget();
            void RemoveAvatar(Data::AvatarInfo *avatar_info);
            void UpdateWorldInfoWidget();
            void RemoveWorld(Data::WorldInfo *world_info);

            void RecalculateMenus();

        private slots:
            void ActiveItemChanged(View::InfoCard *);
            void SceneRectChanged(const QRectF &new_rect);

        private:
            //! Pointer to data manager
            Data::DataManager *data_manager_;

            //! Current scene
            View::EtherScene *scene_;

            //! Menus
            View::EtherMenu *active_menu_;
            View::EtherMenu *top_menu_;
            View::EtherMenu *bottom_menu_;
            int top_menu_visible_items_;
            int bottom_menu_visible_items_;
            int menu_cap_size_;

            //! Max card size
            QRectF card_size_;

            //! Active focus items
            View::InfoCard *last_active_top_card_;
            View::InfoCard *last_active_bottom_card_;

            //! Information widgets
            View::ControlProxyWidget *avatar_info_widget_;
            View::ControlProxyWidget *world_info_widget_;

            //! Control widgets
            View::ControlProxyWidget *connect_control_widget_;
            View::ControlProxyWidget *exit_control_widget_;

            //! Action widget
            View::ActionProxyWidget *action_proxy_widget_;

        signals:
            void LoginRequest(QPair<View::InfoCard*, View::InfoCard*> selected_cards);
            void ObjectRemoved(QUuid);
        };
    }
}

#endif // EtherSceneController_H
