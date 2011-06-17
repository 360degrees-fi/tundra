/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_SoundListener.cpp
 *  @brief  Entity-component which provides sound listener position for in-world 3D audio.
 *          Updates parent entity's placeable component's position to the sound system each frame.
 *  @note   Only one entity can have active sound listener at a time.
 */

#include "DebugOperatorNew.h"
#include "EC_SoundListener.h"
#include "IModule.h"
#include "Entity.h"
#include "EC_Placeable.h"
#include "Scene.h"
#include "LoggingFunctions.h"
#include "AudioAPI.h"
#include "SceneAPI.h"
#include "FrameAPI.h"
#include "Framework.h"
#include "MemoryLeakCheck.h"

EC_SoundListener::EC_SoundListener(Scene* scene):
    IComponent(scene),
    active(this, "active", false)
{
    // By default, this component is NOT network-serialized
    SetNetworkSyncEnabled(false);

    connect(this, SIGNAL(ParentEntitySet()), SLOT(RetrievePlaceable()));
    connect(framework->Frame(), SIGNAL(Updated(float)), SLOT(Update()));
    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), SLOT(OnActiveChanged()));
    connect(this, SIGNAL(ParentEntitySet()), SLOT(RegisterActions()));
}

EC_SoundListener::~EC_SoundListener()
{
}

void EC_SoundListener::RetrievePlaceable()
{
    if (!ParentEntity())
        LogError("Couldn't find an parent entity for EC_SoundListener. Cannot retrieve placeable component.");

    placeable_ = ParentEntity()->GetComponent<EC_Placeable>();
    if (!placeable_.lock())
        LogError("Couldn't find an EC_Placeable component from the parent entity.");
}

void EC_SoundListener::Update()
{
    if (active.Get() && !placeable_.expired())
        GetFramework()->Audio()->SetListener(placeable_.lock()->WorldPosition(), placeable_.lock()->WorldOrientation());
}

void EC_SoundListener::OnActiveChanged()
{
    ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
    {
        LogError("Failed on OnActiveChanged method cause default world scene wasn't set.");
        return;
    }

    if (active.Get())
    {
        // Disable all the other listeners, only one can be active at a time.
        EntityList listeners = scene->GetEntitiesWithComponent("EC_SoundListener");
        foreach(EntityPtr listener, listeners)
        {
            EC_SoundListener *ec = listener->GetComponent<EC_SoundListener>().get();
            if (ec != this)
                listener->GetComponent<EC_SoundListener>()->active.Set(false, AttributeChange::Default);
        }
    }
}

void EC_SoundListener::RegisterActions()
{
    Entity *entity = ParentEntity();
    assert(entity);
    if (entity)
    {
        entity->ConnectAction("Active", this, SLOT(OnActiveChanged()));
    }
    else
    {
        LogError("Fail to register actions cause component's parent entity is null.");
    }
}
