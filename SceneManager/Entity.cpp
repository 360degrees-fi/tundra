// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "ComponentInterface.h"
#include "Entity.h"
#include "Framework.h"
#include "SceneManager.h"

namespace Scene
{
    Entity::Entity(Foundation::Framework* framework) : 
        framework_(framework)
    {
    }
    
    Entity::Entity(Foundation::Framework* framework, uint id) : 
        framework_(framework),
        id_(id)
    {
    }

    Entity::~Entity()
    {
        // If components still alive, they become free-floating
        for (size_t i=0 ; i<components_.size() ; ++i)
            components_[i]->SetParentEntity(0);
    }
    
    void Entity::SetNewId(entity_id_t id)
    {
        id_ = id;
    }
    
    void Entity::AddComponent(const Foundation::ComponentInterfacePtr &component)
    {
        // Must exist and be free
        if (component && component->GetParentEntity() == 0)
        {
            component->SetParentEntity(this);
            components_.push_back(component);
        
            ///\todo Ali: send event
        }
    }

    void Entity::RemoveComponent(const Foundation::ComponentInterfacePtr &component)
    {
        if (component)
        {
            ComponentVector::iterator iter = std::find(components_.begin(), components_.end(), component);
            if (iter != components_.end())
            {
                (*iter)->SetParentEntity(0);
                components_.erase(iter);
                ///\todo Ali: send event
            } else
            {
                Foundation::RootLogWarning("Failed to remove component: " + component->Name() + " from entity: " + ToString(GetId()));
            }
        }
    }

    Foundation::ComponentInterfacePtr Entity::GetOrCreateComponent(const std::string &name)
    {
        for (size_t i=0 ; i<components_.size() ; ++i)
            if (components_[i]->Name() == name)
                return components_[i];

        // If component was not found, try to create
        Foundation::ComponentInterfacePtr new_comp = framework_->GetComponentManager()->CreateComponent(name);
        if (new_comp)
        {
            AddComponent(new_comp);
            return new_comp;
        }

        // Could not be created
        return Foundation::ComponentInterfacePtr();
    }
    
    Foundation::ComponentInterfacePtr Entity::GetComponent(const std::string &name) const
    {
        for (size_t i=0 ; i<components_.size() ; ++i)
            if (components_[i]->Name() == name)
                return components_[i];

        return Foundation::ComponentInterfacePtr();
    }

    const Entity::ComponentVector &Entity::GetComponentVector() const { return components_; }

}
