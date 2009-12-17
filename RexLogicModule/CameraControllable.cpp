
// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "CameraControllable.h"
#include "EntityComponent/EC_NetworkPosition.h"
#include "SceneEvents.h"
#include "Entity.h"
#include "SceneManager.h"
#include "EC_OgrePlaceable.h"
#include "Renderer.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgreMesh.h"
#include "EntityComponent/EC_AvatarAppearance.h"
//#include "RexTypes.h"
#include "InputEvents.h"
#include "InputServiceInterface.h"
#include <Ogre.h>


namespace RexLogic
{
    CameraControllable::CameraControllable(Foundation::Framework *fw) : 
        framework_(fw)
      , action_event_category_(fw->GetEventManager()->QueryEventCategory("Action"))
      , current_state_(ThirdPerson)
      , firstperson_pitch_(0)
      , firstperson_yaw_(0)
      , drag_pitch_(0)
      , drag_yaw_(0)
    {
        camera_distance_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "default_distance", 20.f);
        camera_min_distance_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "min_distance", 1.f);
        camera_max_distance_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "max_distance", 50.f);

        
        camera_offset_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "third_person_offset", ToString(Vector3df(0, 0, 1.8f))));
        
        camera_offset_firstperson_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "first_person_offset", ToString(Vector3df(0.5f, 0, 0.8f))));

        sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "translation_sensitivity", 25.f);
        zoom_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "zoom_sensitivity", 0.015f);
        firstperson_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "mouselook_rotation_sensitivity", 1.3f);
        
        action_trans_[RexTypes::Actions::MoveForward] = Vector3df::NEGATIVE_UNIT_Z;
        action_trans_[RexTypes::Actions::MoveBackward] = Vector3df::UNIT_Z;
        action_trans_[RexTypes::Actions::MoveLeft] = Vector3df::NEGATIVE_UNIT_X;
        action_trans_[RexTypes::Actions::MoveRight] = Vector3df::UNIT_X;
        action_trans_[RexTypes::Actions::MoveUp] = Vector3df::UNIT_Y;
        action_trans_[RexTypes::Actions::MoveDown] = Vector3df::NEGATIVE_UNIT_Y;
    }

    bool CameraControllable::HandleSceneEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        //! \todo This is where our user agent model design breaks down. We assume only one controllable entity exists and that it is a target for the camera.
        //!       Should be changed so in some way the target can be changed and is not automatically assigned. -cm
        if (event_id == Scene::Events::EVENT_CONTROLLABLE_ENTITY)
            target_entity_ = checked_static_cast<Scene::Events::EntityEventData*>(data)->entity;
        
        return false;
    }

    bool CameraControllable::HandleInputEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (event_id == Input::Events::INPUTSTATE_THIRDPERSON && current_state_ != ThirdPerson)
        {
            current_state_ = ThirdPerson;
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::INPUTSTATE_FIRSTPERSON && current_state_ != FirstPerson)
        {
            current_state_ = FirstPerson;         
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::INPUTSTATE_FREECAMERA && current_state_ != FreeLook)
        {
            current_state_ = FreeLook;
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::SCROLL)
        {
            CameraZoomEvent event_data;
            //event_data.entity = entity_.lock(); // no entity for camera, :( -cm
            event_data.amount = checked_static_cast<Input::Events::SingleAxisMovement*>(data)->z_.rel_;
            //if (event_data.entity) // only send the event if we have an existing entity, no point otherwise
            framework_->GetEventManager()->SendEvent(action_event_category_, RexTypes::Actions::Zoom, &event_data);
        }

        return false;
    }

    bool CameraControllable::HandleActionEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (event_id == RexTypes::Actions::Zoom)
        {
            Real value = checked_static_cast<CameraZoomEvent*>(data)->amount;

            camera_distance_ -= (value * zoom_sensitivity_);
            camera_distance_ = clamp(camera_distance_, camera_min_distance_, camera_max_distance_);
        }

        if (current_state_ == FreeLook)
        {
            ActionTransMap::const_iterator it = action_trans_.find(event_id);
            if (it != action_trans_.end())
            {
                // start movement
                const Vector3df &vec = it->second;
                free_translation_.x = ( vec.x == 0 ) ? free_translation_.x : vec.x;
                free_translation_.y = ( vec.y == 0 ) ? free_translation_.y : vec.y;
                free_translation_.z = ( vec.z == 0 ) ? free_translation_.z : vec.z;
            }
            it = action_trans_.find(event_id - 1);
            if (it != action_trans_.end())
            {
                // stop movement
                const Vector3df &vec = it->second;
                free_translation_.x = ( vec.x == 0 ) ? free_translation_.x : 0;
                free_translation_.y = ( vec.y == 0 ) ? free_translation_.y : 0;
                free_translation_.z = ( vec.z == 0 ) ? free_translation_.z : 0;
            }
            normalized_free_translation_ = free_translation_;
            normalized_free_translation_.normalize();
        }

        return false;
    }

    void CameraControllable::AddTime(f64 frametime)
    {
        boost::shared_ptr<Input::InputServiceInterface> input = framework_->GetService<Input::InputServiceInterface>(Foundation::Service::ST_Input).lock();
        if (input)
        {
            boost::optional<const Input::Events::Movement&> movement = input->PollSlider(Input::Events::MOUSELOOK);
            if (movement)
            {
                drag_yaw_ = static_cast<Real>(movement->x_.rel_) * -0.005f;
                drag_pitch_ = static_cast<Real>(movement->y_.rel_) * -0.005f;
            } else if (drag_pitch_ != 0 || drag_yaw_ != 0)
            {
                drag_yaw_ = 0;
                drag_pitch_ = 0;
            }
        }

        boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetServiceManager()->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
        Scene::EntityPtr target = target_entity_.lock();

        if (renderer && target)
        {
            Ogre::Camera *camera = renderer->GetCurrentCamera();

            // for smoothness, we apparently need to get rotation from network position and position from placeable. Go figure. -cm
            EC_NetworkPosition *netpos = checked_static_cast<EC_NetworkPosition*>(target->GetComponent(EC_NetworkPosition::NameStatic()).get());
            OgreRenderer::EC_OgrePlaceable *placeable = 
                checked_static_cast<OgreRenderer::EC_OgrePlaceable*>(target->GetComponent(OgreRenderer::EC_OgrePlaceable::NameStatic()).get());
            if (netpos && placeable)
            {
                Vector3df avatar_pos = placeable->GetPosition();
                Quaternion avatar_orientation = netpos->orientation_; 

                if (current_state_ == FirstPerson || current_state_ == ThirdPerson)
                {
                    // this is mostly for third person camera, but also needed by first person camera since it sets proper initial orientation for the camera
                    Vector3df pos = avatar_pos;
                    pos += (avatar_orientation * Vector3df::NEGATIVE_UNIT_X * camera_distance_);
                    pos += (avatar_orientation * camera_offset_);
                    camera->setPosition(pos.x, pos.y, pos.z);
                    
                    Vector3df lookat = avatar_pos + avatar_orientation * camera_offset_;
                    camera->lookAt(lookat.x, lookat.y, lookat.z);
                }
                
                if (current_state_ == FirstPerson)
                {
                    bool fallback = true;
                    // Try to use head bone from target entity to get the first person camera position
                    Foundation::ComponentPtr mesh_ptr = target->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
                    Foundation::ComponentPtr appearance_ptr = target->GetComponent(EC_AvatarAppearance::NameStatic());
                    if (mesh_ptr && appearance_ptr)
                    {
                        OgreRenderer::EC_OgreMesh& mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(mesh_ptr.get());
                        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(appearance_ptr.get());
                        Ogre::Entity* ent = mesh.GetEntity();
                        if (ent)
                        {
                            Ogre::SkeletonInstance* skel = ent->getSkeleton();
                            
                            std::string view_bone_name;
                            Real adjustheight = mesh.GetAdjustPosition().z;
                            
                            if (appearance.HasProperty("viewbone"))
                            {
                                // This bone property is exclusively for view tracking & assumed to be correct position, no offset
                                view_bone_name = appearance.GetProperty("viewbone");
                            }
                            else if (appearance.HasProperty("headbone"))
                            {
                                view_bone_name = appearance.GetProperty("headbone");
                                // The biped head bone is anchored at the neck usually. Therefore a guessed fixed offset is needed,
                                // which is not preferable, but necessary
                                adjustheight += 0.15;
                            }
                            
                            if (!view_bone_name.empty())
                            {
                                if (skel && skel->hasBone(view_bone_name))
                                {
                                    // Hack: force Ogre to update skeleton with current animation state, even if avatar invisible
                                    if (ent->getAllAnimationStates())
                                        skel->setAnimationState(*ent->getAllAnimationStates());
                                    
                                    Ogre::Bone* bone = skel->getBone(view_bone_name);
                                    Ogre::Vector3 headpos = bone->_getDerivedPosition();
                                    Vector3df ourheadpos(-headpos.z + 0.5f, -headpos.x, headpos.y + adjustheight);
                                    RexTypes::Vector3 campos = avatar_pos + (avatar_orientation * ourheadpos);
                                    camera->setPosition(campos.x, campos.y, campos.z);
                                    fallback = false;
                                }
                            }
                        }
                    }
                    // Fallback using fixed position
                    if (fallback)
                    {
                        RexTypes::Vector3 campos = avatar_pos + (avatar_orientation * camera_offset_firstperson_);
                        camera->setPosition(campos.x, campos.y, campos.z);
                    }

                    // update camera pitch
                    if (drag_pitch_ != 0)
                    {
                        firstperson_pitch_ += drag_pitch_ * firstperson_sensitivity_;
                        firstperson_pitch_ = clamp(firstperson_pitch_, -HALF_PI, HALF_PI);
                    }
                    camera->pitch(Ogre::Radian(firstperson_pitch_));
                }

                if (current_state_ == FreeLook)
                {
                    const float trans_dt = (float)frametime * sensitivity_;

                    Ogre::Vector3 pos = camera->getPosition();
                    pos += camera->getOrientation() * Ogre::Vector3(normalized_free_translation_.x, normalized_free_translation_.y, normalized_free_translation_.z) * trans_dt;
                    camera->setPosition(pos);

                    camera->pitch(Ogre::Radian(drag_pitch_ * firstperson_sensitivity_));
                    camera->yaw(Ogre::Radian(drag_yaw_ * firstperson_sensitivity_));
                }
            }
        }
        
        // switch between first and third person modes, depending on how close we are to the avatar
        switch (current_state_)
        {
        case FirstPerson:
            {
                if (camera_distance_ != camera_min_distance_)
                {
                    current_state_ = ThirdPerson;
                    event_category_id_t event_category = framework_->GetEventManager()->QueryEventCategory("Input");
                    framework_->GetEventManager()->SendEvent(event_category, Input::Events::INPUTSTATE_THIRDPERSON, 0);
                    
                    firstperson_pitch_ = 0.0f;
                }
                break;
            }
        case ThirdPerson:
            {
                if (camera_distance_ == camera_min_distance_)
                {
                    event_category_id_t event_category = framework_->GetEventManager()->QueryEventCategory("Input");
                    framework_->GetEventManager()->SendEvent(event_category, Input::Events::INPUTSTATE_FIRSTPERSON, 0);
                    current_state_ = FirstPerson;
                    
                    firstperson_pitch_ = 0.0f;
                }
                break;
            }
		}
    }

	//experimental for py api
	void CameraControllable::SetYawPitch(Real newyaw, Real newpitch)
	{
		boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetServiceManager()->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
		Ogre::Camera *camera = renderer->GetCurrentCamera();

		firstperson_yaw_ = newyaw;
		firstperson_pitch_ = newpitch;
		camera->yaw(Ogre::Radian(firstperson_yaw_));
		camera->pitch(Ogre::Radian(firstperson_pitch_));
	}
}

