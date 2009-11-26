// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "EventHandlers/SceneEventHandler.h"
#include "SceneEvents.h"
#include "RexLogicModule.h"
#include "SceneManager.h"
#include "EntityComponent/EC_OpenSimPrim.h"
#include "EntityComponent/EC_NetworkPosition.h"
#include "EC_OgrePlaceable.h"

// Login class pointer being used, can this be done somehow prettier?
#include "Login/LoginUI.h" 

namespace RexLogic
{

void PopulateUpdateInfos(std::vector<ProtocolUtilities::ObjectUpdateInfo>& dest, const std::vector<Scene::EntityPtr>& src)
{
    for (Core::uint i = 0; i < src.size(); ++i)
    {        
        if (!src[i]) 
            continue;
            
        const Foundation::ComponentInterfacePtr &prim_component = src[i]->GetComponent("EC_OpenSimPrim");
        if (!prim_component) 
            continue;
        RexLogic::EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(prim_component.get());
      
        const Foundation::ComponentInterfacePtr &ogre_component = src[i]->GetComponent("EC_OgrePlaceable");
        if (!ogre_component) 
            continue;      
        OgreRenderer::EC_OgrePlaceable *ogre_pos = checked_static_cast<OgreRenderer::EC_OgrePlaceable *>(ogre_component.get());

        ProtocolUtilities::ObjectUpdateInfo new_info;
        new_info.local_id_ = prim->LocalId;
        new_info.position_ = ogre_pos->GetPosition();
        new_info.orientation_ = ogre_pos->GetOrientation();
        new_info.scale_ = ogre_pos->GetScale();
        
        dest.push_back(new_info);
    }    
}

SceneEventHandler::SceneEventHandler(Foundation::Framework *framework, RexLogicModule *rexlogicmodule) :
    framework_(framework), rexlogicmodule_(rexlogicmodule)
{
}

SceneEventHandler::~SceneEventHandler()
{
}

bool SceneEventHandler::HandleSceneEvent(Core::event_id_t event_id, Foundation::EventDataInterface* data)
{
    Scene::Events::SceneEventData *event_data = dynamic_cast<Scene::Events::SceneEventData *>(data);

    switch(event_id)
    {
    case Scene::Events::EVENT_ENTITY_SELECT:
        rexlogicmodule_->GetServerConnection()->SendObjectSelectPacket(event_data->localID);
        break;
    case Scene::Events::EVENT_ENTITY_DESELECT:
        rexlogicmodule_->GetServerConnection()->SendObjectDeselectPacket(event_data->localID);
        break;
    case Scene::Events::EVENT_ENTITY_UPDATED:
        {
            std::vector<ProtocolUtilities::ObjectUpdateInfo> update_info_list;
            PopulateUpdateInfos(update_info_list, event_data->entity_ptr_list);
            rexlogicmodule_->GetServerConnection()->SendMultipleObjectUpdatePacket(update_info_list);
        }
        break;
    case Scene::Events::EVENT_ENTITY_GRAB:
        rexlogicmodule_->GetServerConnection()->SendObjectGrabPacket(event_data->localID);
        break;
    case Scene::Events::EVENT_ENTITY_DELETED:
        HandleEntityDeletedEvent(event_data->localID);
        break;
    case Scene::Events::EVENT_ENTITY_CREATE:
    {
        Scene::Events::CreateEntityEventData *pos_data = dynamic_cast<Scene::Events::CreateEntityEventData *>(data);
        if (pos_data)
            rexlogicmodule_->GetServerConnection()->SendObjectAddPacket(pos_data->position);
        break;
    }
    case Scene::Events::EVENT_CONTROLLABLE_ENTITY:
    {
        rexlogicmodule_->GetLogin()->UpdateLoginProgressUI(QString("Downloading of terrain and avatar completed"), 100, ProtocolUtilities::Connection::STATE_ENUM_COUNT);
        break;
    }
    default:
        break;
    }

    return false;
}

void SceneEventHandler::HandleEntityDeletedEvent(Core::event_id_t entityid)
{
}

}
