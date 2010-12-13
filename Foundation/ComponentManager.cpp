// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "ComponentManager.h"
#include "CoreStringUtils.h"
#include "Framework.h"
#include "IComponentFactory.h"
#include "IComponent.h"
#include "IAttribute.h"
#include "Transform.h"
#include "AssetReference.h"

#include <QVariant>

#include "MemoryLeakCheck.h"

ComponentManager::ComponentManager(Foundation::Framework *framework) : framework_(framework)
{
    attributeTypes_.push_back("string");
    attributeTypes_.push_back("int");
    attributeTypes_.push_back("real");
    attributeTypes_.push_back("color");
    attributeTypes_.push_back("vector3df");
    attributeTypes_.push_back("bool");
    attributeTypes_.push_back("uint");
    attributeTypes_.push_back("quaternion");
    attributeTypes_.push_back("assetreference");
    attributeTypes_.push_back("qvariant");
    attributeTypes_.push_back("qvariantlist");
    attributeTypes_.push_back("transform");
}

void ComponentManager::RegisterFactory(const QString &component, const ComponentFactoryPtr &factory)
{
    if (factories_.find(component) == factories_.end())
    {
        uint hash = GetHash(component);
        
        // Check for hash collision
        ComponentFactoryMap::const_iterator i = factories_.begin();
        while (i != factories_.end())
        {
            if ((GetHash(i->first) == hash) && (i->first != component))
            {
                throw Exception(std::string("Fatal: component factory hash collision: " + i->first.toStdString() + " vs " + component.toStdString()).c_str());
            }
            ++i;
        }
        
        factories_[component] = factory;
        factories_hash_[hash] = factory;
        hashToTypeName_[hash] = component;
    }
}

void ComponentManager::UnregisterFactory(const QString &component)
{
    uint hash = GetHash(component);
    factories_.erase(component);
    factories_hash_.erase(hash);
    hashToTypeName_.erase(hash);
}

bool ComponentManager::CanCreate(const QString &type_name)
{
    return (factories_.find(type_name) != factories_.end());
}

bool ComponentManager::CanCreate(uint type_hash)
{
    return (factories_hash_.find(type_hash) != factories_hash_.end());
}

ComponentPtr ComponentManager::CreateComponent(const QString &type_name)
{
    ComponentFactoryMap::const_iterator iter = factories_.find(type_name);
    if (iter == factories_.end())
        return ComponentPtr();

    ComponentPtr component = (*iter->second.get())();
    return component;
}

ComponentPtr ComponentManager::CreateComponent(uint type_hash)
{
    ComponentFactoryHashMap::const_iterator iter = factories_hash_.find(type_hash);
    if (iter == factories_hash_.end())
        return ComponentPtr();

    ComponentPtr component = (*iter->second.get())();
    return component;
}

ComponentPtr ComponentManager::CreateComponent(const QString &type_name, const QString &name)
{
    ComponentFactoryMap::const_iterator iter = factories_.find(type_name);
    if (iter == factories_.end())
        return ComponentPtr();

    ComponentPtr component = (*iter->second.get())();
    component->SetName(name);
    return component;
}

ComponentPtr ComponentManager::CreateComponent(uint type_hash, const QString &name)
{
    ComponentFactoryHashMap::const_iterator iter = factories_hash_.find(type_hash);
    if (iter == factories_hash_.end())
        return ComponentPtr();

    ComponentPtr component = (*iter->second.get())();
    component->SetName(name);
    return component;
}

ComponentPtr ComponentManager::CloneComponent(const ComponentPtr &component)
{
    ComponentFactoryMap::const_iterator iter = factories_.find(component->TypeName());
    if (iter == factories_.end())
        return ComponentPtr();

    ComponentPtr newComponent = (*iter->second.get())(component);
    return newComponent;
}

IAttribute *ComponentManager::CreateAttribute(IComponent*owner, const std::string &typeName, const std::string &name)
{
    // The dynamically created attributes are deleted at the EC_DynamicComponent dtor.
    IAttribute *attribute = 0;
    if(typeName == "string")
        attribute = new Attribute<QString>(owner, name.c_str());
    else if(typeName == "int")
        attribute = new Attribute<int>(owner, name.c_str());
    else if(typeName == "real")
        attribute = new Attribute<float>(owner, name.c_str());
    else if(typeName == "color")
        attribute = new Attribute<Color>(owner, name.c_str());
    else if(typeName == "vector3df")
        attribute = new Attribute<Vector3df>(owner, name.c_str());
    else if(typeName == "bool")
        attribute = new Attribute<bool>(owner, name.c_str());
    else if(typeName == "uint")
        attribute = new Attribute<uint>(owner, name.c_str());
    else if(typeName == "quaternion")
        attribute = new Attribute<Quaternion>(owner, name.c_str());
    else if(typeName == "assetreference")
        attribute = new Attribute<AssetReference>(owner, name.c_str());
    else if(typeName == "qvariant")
        attribute = new Attribute<QVariant>(owner, name.c_str());
    else if(typeName == "qvariantlist")
        attribute = new Attribute<QVariantList >(owner, name.c_str());
    else if(typeName == "transform")
        attribute = new Attribute<Transform>(owner, name.c_str());
    return attribute;
}

QStringList ComponentManager::GetAttributeTypes() const
{
    return attributeTypes_;
}

QStringList ComponentManager::GetAvailableComponentTypeNames() const
{
    QStringList ret;

    ComponentManager::ComponentFactoryMap::const_iterator i = factories_.begin();
    while(i != factories_.end())
    {
        ret.append(i->first);
        ++i;
    }

    return ret;
}

const QString& ComponentManager::GetComponentTypeName(uint type_hash) const
{
    static const QString empty;
    std::map<uint, QString>::const_iterator i = hashToTypeName_.find(type_hash);
    if (i == hashToTypeName_.end())
        return empty;
    else
        return i->second;
}

