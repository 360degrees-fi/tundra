// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "CoreStringUtils.h"
#include "RexNetworkUtils.h"
#include "DebugOperatorNew.h"
#include "SceneImporter.h"
#include "TundraLogicModule.h"
#include "Vector3D.h"
#include "Quaternion.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Name.h"
#include "ServiceManager.h"
#include "Renderer.h"
#include "SceneDesc.h"

#include <Ogre.h>

#include <QDomDocument>
#include <QFile>

using namespace RexTypes;

namespace fs = boost::filesystem;

namespace TundraLogic
{

struct MeshInfo
{
    std::string name_;
    unsigned filesize_;
};

bool ProcessBraces(const std::string& line, int& braceLevel)
{
    if (line == "{")
    {
        ++braceLevel;
        return true;
    } 
    else if (line == "}")
    {
        --braceLevel;
        return true;
    }
    else return false;
}

SceneImporter::SceneImporter(const Scene::ScenePtr &scene) :
    scene_(scene)
{
}

SceneImporter::~SceneImporter()
{
}

Scene::EntityPtr SceneImporter::ImportMesh(const std::string& meshname, std::string in_asset_dir, std::string out_asset_dir,
                                           const Transform &worldtransform, const std::string& entity_prefab_xml, AttributeChange::Type change,
                                           bool localassets, bool inspect, const std::string &meshName)
{
    if (!scene_)
    {
        TundraLogicModule::LogError("Null scene for mesh import");
        return Scene::EntityPtr();
    }
    
    std::string prefix;
    if (localassets)
        prefix = "file://";
    
    // Create output asset path if does not exist
    if (boost::filesystem::exists(out_asset_dir) == false)
        boost::filesystem::create_directory(out_asset_dir);
    
    // If in asset dir not specified, use the branch path of the mesh
    if (in_asset_dir.empty())
    {
        boost::filesystem::path path(meshname);
        in_asset_dir = path.branch_path().string();
    }
    
    if (!in_asset_dir.empty())
    {
        char lastchar = in_asset_dir[in_asset_dir.length() - 1];
        if ((lastchar != '/') && (lastchar != '\\'))
            in_asset_dir += '/';
    }
    if (!out_asset_dir.empty())
    {
        char lastchar = out_asset_dir[out_asset_dir.length() - 1];
        if ((lastchar != '/') && (lastchar != '\\'))
            out_asset_dir += '/';
    }
    

    boost::filesystem::path path(meshname);
    std::string meshleafname = path.leaf();

    
    std::vector<std::string> material_names;
    std::string skeleton_name;
    if (inspect)
    {
        if (!ParseMeshForMaterialsAndSkeleton(meshname, material_names, skeleton_name))
            return Scene::EntityPtr();
    }
    
    std::set<std::string> material_names_set;
    for (uint i = 0; i < material_names.size(); ++i)
    {
        TundraLogicModule::LogDebug("Material ref: " + material_names[i]);
        material_names_set.insert(material_names[i]);
    }
    TundraLogicModule::LogDebug("Skeleton ref: " + skeleton_name);
    
    // Scan the asset dir for material files, because we don't actually know what material file the mesh refers to.
    std::vector<std::string> material_files;
    if (inspect)
    {
        fs::directory_iterator iter(in_asset_dir);
        fs::directory_iterator end_iter;
        for(; iter != end_iter; ++iter )
        {
            if (fs::is_regular_file(iter->status()))
            {
                std::string ext = iter->path().extension();
                boost::algorithm::to_lower(ext);
                if (ext == ".material")
                {
                    TundraLogicModule::LogDebug("Material file: " + iter->path().string());
                    material_files.push_back(iter->path().string());
                }
            }
        }
    }
    
    // Process materials
    std::set<std::string> all_textures;
    for (uint i = 0; i < material_files.size(); ++i)
    {
        std::set<std::string> textures;
        textures = ProcessMaterialFile(material_files[i], material_names_set, out_asset_dir, localassets);
        all_textures.insert(textures.begin(), textures.end());
    }
    
    // Process textures
    ProcessTextures(all_textures, in_asset_dir, out_asset_dir);
    
    // Copy mesh and skeleton
    CopyAsset(meshleafname, in_asset_dir, out_asset_dir);
    if (!skeleton_name.empty())
        CopyAsset(skeleton_name, in_asset_dir, out_asset_dir);

    // mesh copied, add mesh name inside the file
    if (!meshName.empty()) meshleafname += std::string("/") + meshName;
    
    // Create a new entity in any case, with a new ID
    Scene::EntityPtr newentity = scene_->CreateEntity(0, QStringList(), change, true);
    if (!newentity)
    {
        TundraLogicModule::LogError("Could not create entity for mesh");
        return newentity;
    }
    
    // If the prefab contains valid data, instantiate the components from there
    QDomDocument prefab;
    prefab.setContent(QString::fromStdString(entity_prefab_xml));
    QDomElement ent_elem = prefab.firstChildElement("entity");
    if (!ent_elem.isNull())
    {
        //! \todo this should possibly be a function in SceneManager
        QDomElement comp_elem = ent_elem.firstChildElement("component");
        while (!comp_elem.isNull())
        {
            QString type_name = comp_elem.attribute("type");
            QString name = comp_elem.attribute("name");
            ComponentPtr newcomp = newentity->GetOrCreateComponent(type_name, name);
            if (newcomp)
                // Trigger no signal yet when entity is in incoherent state
                newcomp->DeserializeFrom(comp_elem, AttributeChange::Disconnected);
            comp_elem = comp_elem.nextSiblingElement("component");
        }
        scene_->EmitEntityCreated(newentity, change);
    }
    
    // Fill the placeable attributes
    EC_Placeable* placeablePtr = checked_static_cast<EC_Placeable*>(newentity->GetOrCreateComponent(EC_Placeable::TypeNameStatic(), change).get());
    if (placeablePtr)
        placeablePtr->transform.Set(worldtransform, AttributeChange::Disconnected);
    else
        TundraLogicModule::LogError("No EC_Placeable was created!");
    // Fill the mesh attributes
    QVector<QVariant> materials;
    for (uint i = 0; i < material_names.size(); ++i)
        materials.push_back(QString::fromStdString(prefix + material_names[i] + ".material"));
    EC_Mesh* meshPtr = checked_static_cast<EC_Mesh*>(newentity->GetOrCreateComponent(EC_Mesh::TypeNameStatic(), change).get());
    if (meshPtr)
    {
        meshPtr->meshRef.Set(AssetReference(QString::fromStdString(prefix + meshleafname)), AttributeChange::Disconnected);
        if (!skeleton_name.empty())
            meshPtr->skeletonRef.Set(AssetReference(QString::fromStdString(prefix + skeleton_name)), AttributeChange::Disconnected);
        meshPtr->meshMaterial.Set(QList<QVariant>::fromVector(materials), AttributeChange::Disconnected);

        if (inspect)
            meshPtr->nodeTransformation.Set(Transform(Vector3df(0,0,0), Vector3df(90,0,180), Vector3df(1,1,1)), AttributeChange::Disconnected);
        else
            meshPtr->nodeTransformation.Set(Transform(Vector3df(0,0,0), Vector3df(0,0,0), Vector3df(1,1,1)), AttributeChange::Disconnected);
    }
    else
        TundraLogicModule::LogError("No EC_Mesh was created!");
    
    // All components have been loaded/modified. Trigger change for them now.
    const Scene::Entity::ComponentVector &components = newentity->GetComponentVector();
    for(uint i = 0; i < components.size(); ++i)
        components[i]->ComponentChanged(change);
    
    return newentity;
}

QList<Scene::Entity *> SceneImporter::Import(const std::string& filename, std::string in_asset_dir, std::string out_asset_dir,
    const Transform &worldtransform, AttributeChange::Type change, bool clearscene, bool localassets, bool replace)
{
    QList<Scene::Entity *> ret;
    if (!scene_)
    {
        TundraLogicModule::LogError("Null scene for import");
        return ret;
    }
    
    try
    {
        if (clearscene)
            TundraLogicModule::LogInfo("Importing scene from " + filename + " and clearing the old");
        else
            TundraLogicModule::LogInfo("Importing scene from " + filename);
        
        // Create output asset path if does not exist
        if (boost::filesystem::exists(out_asset_dir) == false)
            boost::filesystem::create_directory(out_asset_dir);
        
        // If in asset dir not specified, use the branch path of the scene
        if (in_asset_dir.empty())
        {
            boost::filesystem::path path(filename);
            in_asset_dir = path.branch_path().string();
        }
        
        if (!in_asset_dir.empty())
        {
            char lastchar = in_asset_dir[in_asset_dir.length() - 1];
            if ((lastchar != '/') && (lastchar != '\\'))
                in_asset_dir += '/';
        }
        if (!out_asset_dir.empty())
        {
            char lastchar = out_asset_dir[out_asset_dir.length() - 1];
            if ((lastchar != '/') && (lastchar != '\\'))
                out_asset_dir += '/';
        }
        
        QFile file(filename.c_str());
        if (!file.open(QFile::ReadOnly))
        {
            file.close();
            TundraLogicModule::LogError("Failed to open file");
            return QList<Scene::Entity *>();
        }
        
        QDomDocument dotscene;
        if (!dotscene.setContent(&file))
        {
            file.close();
            TundraLogicModule::LogError("Failed to parse XML content");
            return ret;
        }
        
        file.close();
        
        QDomElement scene_elem = dotscene.firstChildElement("scene");
        if (scene_elem.isNull())
        {
            TundraLogicModule::LogError("No scene element");
            return ret;
        }
        QDomElement nodes_elem = scene_elem.firstChildElement("nodes");
        if (nodes_elem.isNull())
        {
            TundraLogicModule::LogError("No nodes element");
            return ret;
        }
        
        bool flipyz = false;
        QString upaxis = scene_elem.attribute("upAxis");
        if (upaxis == "y")
            flipyz = true;
        
        if (clearscene)
            scene_->RemoveAllEntities(true, change);
        
        QDomElement node_elem = nodes_elem.firstChildElement("node");
        
        // First pass: get used assets
        TundraLogicModule::LogInfo("Processing scene for assets");
        ProcessNodeForAssets(node_elem, in_asset_dir);
        
        // Write out the needed assets
        TundraLogicModule::LogInfo("Saving needed assets");
        // By default, assume the material file is scenename.material if scene is scenename.scene. However, if an external reference exists, use that.
        std::string matfilename = ReplaceSubstring(filename, ".scene", ".material");
        QDomElement externals_elem = scene_elem.firstChildElement("externals");
        if (!externals_elem.isNull())
        {
            QDomElement item_elem = externals_elem.firstChildElement("item");
            while (!item_elem.isNull())
            {
                if (item_elem.attribute("type") == "material")
                {
                    QDomElement file_elem = item_elem.firstChildElement("file");
                    if (!file_elem.isNull())
                    {
                        matfilename = in_asset_dir + file_elem.attribute("name").toStdString();
                        break;
                    }
                }
                item_elem = item_elem.nextSiblingElement();
            }
        }
        
        ProcessAssets(matfilename, in_asset_dir, out_asset_dir, localassets);
        
        // Second pass: build scene hierarchy and actually create entities. This assumes assets are available
        TundraLogicModule::LogInfo("Creating entities");

        Quaternion rot(DEGTORAD * worldtransform.rotation.x, DEGTORAD * worldtransform.rotation.y,
            DEGTORAD * worldtransform.rotation.z);
        ProcessNodeForCreation(ret, node_elem, worldtransform.position, rot, worldtransform.scale, change, localassets, flipyz, replace);
    }
    catch (Exception& e)
    {
        TundraLogicModule::LogError(std::string("Exception while scene importing: ") + e.what());
        return QList<Scene::Entity *>();
    }
    
    TundraLogicModule::LogInfo("Finished");
    return ret;
}

bool SceneImporter::ParseMeshForMaterialsAndSkeleton(const std::string& meshname, std::vector<std::string>& material_names, std::string& skeleton_name)
{
    material_names.clear();
    
    QFile mesh_in((meshname).c_str());
    if (!mesh_in.open(QFile::ReadOnly))
    {
        TundraLogicModule::LogError("Could not open input mesh file " + meshname);
        return false;
    }
    else
    {
        QByteArray mesh_bytes = mesh_in.readAll();
        mesh_in.close();
        OgreRenderer::RendererPtr renderer = scene_->GetFramework()->GetServiceManager()->GetService<OgreRenderer::Renderer>().lock();
        if (!renderer)
        {
            TundraLogicModule::LogError("Renderer does not exist");
            return false;
        }
        
        std::string uniquename = renderer->GetUniqueObjectName();
        try
        {
            Ogre::MeshPtr tempmesh = Ogre::MeshManager::getSingleton().createManual(uniquename, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            if (tempmesh.isNull())
            {
                TundraLogicModule::LogError("Failed to create temp mesh");
                return false;
            }
            
            Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)mesh_bytes.data(), mesh_bytes.size(), false));
            Ogre::MeshSerializer serializer;
            serializer.importMesh(stream, tempmesh.getPointer());
            
            for (uint i = 0; i < tempmesh->getNumSubMeshes(); ++i)
            {
                Ogre::SubMesh* submesh = tempmesh->getSubMesh(i);
                if (submesh)
                {
                    // Replace / with _ from material name
                    std::string submeshmat = submesh->getMaterialName();
                    ReplaceCharInplace(submeshmat, '/', '_');
                    
                    material_names.push_back(submeshmat);
                }
            }
            skeleton_name = tempmesh->getSkeletonName();
            
            // Now delete the mesh as we're done with inspecting it
            tempmesh.setNull();
            Ogre::MeshManager::getSingleton().remove(uniquename);
        }
        catch (...)
        {
            TundraLogicModule::LogError("Exception while inspecting mesh " + meshname);
            return false;
        }
    }
    
    return true;
}

SceneDesc SceneImporter::GetSceneDescription(const QString &filename) const
{
    SceneDesc sceneDesc;
    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
    {
        file.close();
        TundraLogicModule::LogError("Failed to open file " + filename.toStdString());
        return sceneDesc;
    }

    //if (!IsSupportedFileType())
    // LogError, return

    QDomDocument dotscene;
    if (!dotscene.setContent(&file))
    {
        file.close();
        TundraLogicModule::LogError("Failed to parse XML content");
        return sceneDesc;
    }
    
    file.close();
    
    QDomElement scene_elem = dotscene.firstChildElement("scene");
    if (scene_elem.isNull())
    {
        TundraLogicModule::LogError("No scene element");
        return sceneDesc;
    }

    QDomElement nodes_elem = scene_elem.firstChildElement("nodes");
    if (nodes_elem.isNull())
    {
        TundraLogicModule::LogError("No nodes element");
        return sceneDesc;
    }

    QDomElement node_elem = nodes_elem.firstChildElement("node");
    while (!node_elem.isNull())
    {
        // Process entity node, if any
        QDomElement entity_elem = node_elem.firstChildElement("entity");
        if (!entity_elem.isNull())
        {
            EntityDesc entityDesc;
            entityDesc.name = entity_elem.attribute("name");
            //info.id = scene->GetNextFreeId();

            // Store the original name. Later we fix duplicates.
            QString mesh_name = entity_elem.attribute("meshFile");
            ComponentDesc compDesc;

            // Attribute for mesh asset reference.
            AttributeDesc meshAttrDesc = { "assetreference", "mesh", mesh_name };
            compDesc.attributes.append(meshAttrDesc);

            //mesh_names_[mesh_name] = mesh_name;
            QDomElement subentities_elem = entity_elem.firstChildElement("subentities");
            if (!subentities_elem.isNull())
            {
                QDomElement subentity_elem = subentities_elem.firstChildElement("subentity");
                while (!subentity_elem.isNull())
                {
                    QString material_name = subentity_elem.attribute("materialName");
                    // Attribute for material asset reference.
                    AttributeDesc matAttrDesc = { "assetreference", "material", material_name };
                    compDesc.attributes.append(matAttrDesc);
                    //material_names_.insert(material_name);
                    subentity_elem = subentity_elem.nextSiblingElement("subentity");
                }
            }
            else
            {
                // If no subentity element, have to interrogate the mesh.
                /*
                std::vector<std::string> material_names;
                std::string skeleton_name;
                ParseMeshForMaterialsAndSkeleton(in_asset_dir + mesh_name, material_names, skeleton_name);
                for (uint i = 0; i < material_names.size(); ++i)
                    material_names_.insert(material_names[i]);
                mesh_default_materials_[mesh_name] = material_names;
                */
            }

            entityDesc.components.append(compDesc);
            sceneDesc.entities.append(entityDesc);
        }
        
        // Process child nodes
        /*
        QDomElement childnode_elem = node_elem.firstChildElement("node");
        if (!childnode_elem.isNull())
            ProcessNodeForAssets(childnode_elem, in_asset_dir);
        */
        // Process siblings
        node_elem = node_elem.nextSiblingElement("node");
    }

    return sceneDesc;
}

void SceneImporter::ProcessNodeForAssets(QDomElement node_elem, const std::string& in_asset_dir)
{
    while (!node_elem.isNull())
    {
        // Process entity node, if any
        QDomElement entity_elem = node_elem.firstChildElement("entity");
        if (!entity_elem.isNull())
        {
            std::string mesh_name = entity_elem.attribute("meshFile").toStdString();
            // Store the original name. Later we fix duplicates.
            mesh_names_[mesh_name] = mesh_name;
            QDomElement subentities_elem = entity_elem.firstChildElement("subentities");
            if (!subentities_elem.isNull())
            {
                QDomElement subentity_elem = subentities_elem.firstChildElement("subentity");
                while (!subentity_elem.isNull())
                {
                    std::string material_name = subentity_elem.attribute("materialName").toStdString();
                    material_names_.insert(material_name);
                    subentity_elem = subentity_elem.nextSiblingElement("subentity");
                }
            }
            else
            {
                // If no subentity element, have to interrogate the mesh.
                std::vector<std::string> material_names;
                std::string skeleton_name;
                ParseMeshForMaterialsAndSkeleton(in_asset_dir + mesh_name, material_names, skeleton_name);
                for (uint i = 0; i < material_names.size(); ++i)
                    material_names_.insert(material_names[i]);
                mesh_default_materials_[mesh_name] = material_names;
            }
        }
        
        // Process child nodes
        QDomElement childnode_elem = node_elem.firstChildElement("node");
        if (!childnode_elem.isNull())
            ProcessNodeForAssets(childnode_elem, in_asset_dir);
        
        // Process siblings
        node_elem = node_elem.nextSiblingElement("node");
    }
}

void SceneImporter::ProcessAssets(const std::string& matfilename, const std::string& in_asset_dir, const std::string& out_asset_dir, bool localassets)
{
    std::set<std::string> used_textures = ProcessMaterialFile(matfilename, material_names_, out_asset_dir, localassets);
    ProcessTextures(used_textures, in_asset_dir, out_asset_dir);
    
    // Copy meshes
    std::vector<MeshInfo> created_meshes;
    std::map<std::string, std::string>::iterator m = mesh_names_.begin();
    while (m != mesh_names_.end())
    {
        std::string meshname = m->first;
        QFile mesh_in((in_asset_dir + meshname).c_str());
        if (!mesh_in.open(QFile::ReadOnly))
            TundraLogicModule::LogError("Could not open input mesh file " + meshname);
        else
        {
            QByteArray bytes = mesh_in.readAll();
            mesh_in.close();
            
            // Loop through all already created meshes and check for duplicate
            bool duplicate_found = false;
            for (unsigned j = 0; j < created_meshes.size(); ++j)
            {
                if (created_meshes[j].filesize_ == (unsigned)bytes.size())
                {
                    QFile mesh_compare((out_asset_dir + created_meshes[j].name_).c_str());
                    if (!mesh_compare.open(QFile::ReadOnly))
                        continue;
                    QByteArray compare_bytes = mesh_compare.readAll();
                    mesh_compare.close();
                    if (compare_bytes.size() != bytes.size())
                        continue;
                    unsigned matches = 0;
                    for (unsigned k = 0; k < bytes.size(); ++k)
                    {
                        if (bytes[k] == compare_bytes[k])
                            matches++;
                    }
                    // If we match over 75% of the bytes at correct places, assume mesh is same
                    // (due to rounding errors in exporting, there may be meshes which differ slightly with their coordinates)
                    if (matches > (unsigned)(bytes.size() * 0.75f))
                    {
                        duplicate_found = true;
                        // Duplicate was found, adjust the map to refer to the original
                        m->second = created_meshes[j].name_;
                        break;
                    }
                }
            }
            
            if (!duplicate_found)
            {
                QFile mesh_out((out_asset_dir + meshname).c_str());
                if (!mesh_out.open(QFile::WriteOnly))
                    TundraLogicModule::LogError("Could not open output mesh file " + meshname);
                else
                {
                    mesh_out.write(bytes);
                    mesh_out.close();
                    
                    MeshInfo newinfo;
                    newinfo.name_ = meshname;
                    newinfo.filesize_ = bytes.size();
                    created_meshes.push_back(newinfo);
                }
            }
        }
        ++m;
    }
}

void SceneImporter::ProcessNodeForCreation(QList<Scene::Entity* > &entities, QDomElement node_elem, Vector3df pos, Quaternion rot, Vector3df scale,
    AttributeChange::Type change, bool localassets, bool flipyz, bool replace)
{
    while (!node_elem.isNull())
    {
        QDomElement pos_elem = node_elem.firstChildElement("position");
        QDomElement rot_elem = node_elem.firstChildElement("rotation");
        QDomElement quat_elem = node_elem.firstChildElement("quaternion");
        QDomElement scale_elem = node_elem.firstChildElement("scale");
        float posx, posy, posz, rotx = 0.0f, roty = 0.0f, rotz = 0.0f, rotw = 1.0f, scalex, scaley, scalez;
        
        posx = ParseString<float>(pos_elem.attribute("x").toStdString(), 0.0f);
        posy = ParseString<float>(pos_elem.attribute("y").toStdString(), 0.0f);
        posz = ParseString<float>(pos_elem.attribute("z").toStdString(), 0.0f);
        
        if (!rot_elem.isNull())
        {
            rotx = ParseString<float>(rot_elem.attribute("qx").toStdString(), 0.0f);
            roty = ParseString<float>(rot_elem.attribute("qy").toStdString(), 0.0f);
            rotz = ParseString<float>(rot_elem.attribute("qz").toStdString(), 0.0f);
            rotw = ParseString<float>(rot_elem.attribute("qw").toStdString(), 1.0f);
        }
        if (!quat_elem.isNull())
        {
            rotx = ParseString<float>(quat_elem.attribute("x").toStdString(), 0.0f);
            roty = ParseString<float>(quat_elem.attribute("y").toStdString(), 0.0f);
            rotz = ParseString<float>(quat_elem.attribute("z").toStdString(), 0.0f);
            rotw = ParseString<float>(quat_elem.attribute("w").toStdString(), 1.0f);
        }
        
        scalex = ParseString<float>(scale_elem.attribute("x").toStdString(), 1.0f);
        scaley = ParseString<float>(scale_elem.attribute("y").toStdString(), 1.0f);
        scalez = ParseString<float>(scale_elem.attribute("z").toStdString(), 1.0f);
        
        Vector3df newpos(posx, posy, posz);
        Quaternion newrot(rotx, roty, rotz, rotw);
        Vector3df newscale(fabsf(scalex), fabsf(scaley), fabsf(scalez));
        
        // Transform by the parent transform
        newrot = rot * newrot;
        newscale = scale * newscale;
        newpos = rot * (scale * newpos);
        newpos += pos;
        
        // Process entity node, if any
        QDomElement entity_elem = node_elem.firstChildElement("entity");
        if (!entity_elem.isNull())
        {
            // Enforce uniqueness for node names, which may not be guaranteed by artists
            std::string base_node_name = node_elem.attribute("name").toStdString();
            if (base_node_name.empty())
                base_node_name = "object";
            std::string node_name = base_node_name;
            int append_num = 1;
            while (node_names_.find(node_name) != node_names_.end())
            {
                node_name = base_node_name + "_" + ToString<int>(append_num);
                ++append_num;
            }
            node_names_.insert(node_name);
            
            // Get mesh name from map
            std::string orig_mesh_name = entity_elem.attribute("meshFile").toStdString();
            QString mesh_name = QString::fromStdString(mesh_names_[orig_mesh_name]);
            
            bool cast_shadows = ParseBool(entity_elem.attribute("castShadows").toStdString());
            
            if (localassets)
                mesh_name = "file://" + mesh_name;
            
            Scene::EntityPtr entity;
            bool new_entity = false;
            QString node_name_qstr = QString::fromStdString(node_name);
            
            // Try to find existing entity by name
            if (replace)
                entity = scene_->GetEntity(node_name_qstr);

            if (!entity)
            {
                entity = scene_->CreateEntity(scene_->GetNextFreeId());
                new_entity = true;
            }
            else
            {
                TundraLogicModule::LogInfo("Updating existing entity " + node_name);
            }
            
            EC_Mesh* meshPtr = 0;
            EC_Name* namePtr = 0;
            EC_Placeable* placeablePtr = 0;
            
            if (entity)
            {
                meshPtr = checked_static_cast<EC_Mesh*>(entity->GetOrCreateComponent(EC_Mesh::TypeNameStatic(), change).get());
                namePtr = checked_static_cast<EC_Name*>(entity->GetOrCreateComponent(EC_Name::TypeNameStatic(), change).get());
                placeablePtr = checked_static_cast<EC_Placeable*>(entity->GetOrCreateComponent(EC_Placeable::TypeNameStatic(), change).get());
                
                if ((meshPtr) && (namePtr) && (placeablePtr))
                {
                    namePtr->name.Set(node_name_qstr, change);
                    
                    QVector<QVariant> materials;
                    QDomElement subentities_elem = entity_elem.firstChildElement("subentities");
                    if (!subentities_elem.isNull())
                    {
                        QDomElement subentity_elem = subentities_elem.firstChildElement("subentity");
                        while (!subentity_elem.isNull())
                        {
                            QString material_name = subentity_elem.attribute("materialName") + ".material";
                            material_name.replace('/', '_');
                            
                            int index = ParseString<int>(subentity_elem.attribute("index").toStdString());
                            if (localassets)
                                material_name = "file://" + material_name;
                            if (index >= materials.size())
                                materials.resize(index + 1);
                            materials[index] = material_name;

                            subentity_elem = subentity_elem.nextSiblingElement("subentity");
                        }
                    }
                    else
                    {
                        // If no subentity element, use the inspected material names we stored earlier
                        const std::vector<std::string>& default_materials = mesh_default_materials_[orig_mesh_name];
                        materials.resize(default_materials.size());
                        for (uint i = 0; i < default_materials.size(); ++i)
                        {
                            QString material_name = QString::fromStdString(default_materials[i]) + ".material";
                            if (localassets)
                                material_name = "file://" + material_name;
                            materials[i] = material_name;
                        }
                    }
                    
                    Transform entity_transform;
                    
                    if (!flipyz)
                    {
                        //! \todo it's unpleasant having to do this kind of coordinate mutilations. Possibly move to native Ogre coordinate system?
                        Vector3df rot_euler;
                        Quaternion adjustedrot = Quaternion(0, 0, PI) * newrot;
                        adjustedrot.toEuler(rot_euler);
                        entity_transform.SetPos(newpos.x, newpos.y, newpos.z);
                        entity_transform.SetRot(rot_euler.x * RADTODEG, rot_euler.y * RADTODEG, rot_euler.z * RADTODEG);
                        entity_transform.SetScale(newscale.x, newscale.z, newscale.y);
                    }
                    else
                    {
                        //! \todo it's unpleasant having to do this kind of coordinate mutilations. Possibly move to native Ogre coordinate system?
                        Vector3df rot_euler;
                        Quaternion adjustedrot(-newrot.x, newrot.z, newrot.y, newrot.w);
                        adjustedrot.toEuler(rot_euler);
                        entity_transform.SetPos(-newpos.x, newpos.z, newpos.y);
                        entity_transform.SetRot(rot_euler.x * RADTODEG, rot_euler.y * RADTODEG, rot_euler.z * RADTODEG);
                        entity_transform.SetScale(newscale.x, newscale.y, newscale.z);
                    }
                    
                    meshPtr->nodeTransformation.Set(Transform(Vector3df(0,0,0), Vector3df(90,0,180), Vector3df(1,1,1)), change);
                    
                    placeablePtr->transform.Set(entity_transform, change);
                    meshPtr->meshRef.Set(AssetReference(mesh_name), change);
                    meshPtr->meshMaterial.Set(QList<QVariant>::fromVector(materials), change);
                    meshPtr->castShadows.Set(cast_shadows, change);

                    if (new_entity)
                        scene_->EmitEntityCreated(entity, change);
                    placeablePtr->ComponentChanged(change);
                    meshPtr->ComponentChanged(change);
                    namePtr->ComponentChanged(change);

                    entities.append(entity.get());
                }
                else
                    TundraLogicModule::LogError("Could not create mesh, placeable, name components");
            }
        }
        
        // Process child nodes
        QDomElement childnode_elem = node_elem.firstChildElement("node");
        if (!childnode_elem.isNull())
            ProcessNodeForCreation(entities, childnode_elem, newpos, newrot, newscale, change, localassets, flipyz, replace);
        
        // Process siblings
        node_elem = node_elem.nextSiblingElement("node");
    }
}

std::set<std::string> SceneImporter::ProcessMaterialFile(const std::string& matfilename, const std::set<std::string>& used_materials, const std::string& out_asset_dir, bool localassets)
{
    std::set<std::string> used_textures;
    
    bool known_material = false;
    
    // Read the material file
    QFile matfile(matfilename.c_str());
    if (!matfile.open(QFile::ReadOnly))
    {
        TundraLogicModule::LogError("Could not open material file " + matfilename);
        return used_textures;
    }
    else
    {
        QByteArray bytes = matfile.readAll();
        matfile.close();
        
        QFile matoutfile;
        
        if (bytes.size())
        {
            int num_materials = 0;
            int brace_level = 0;
            bool skip_until_next = false;
            int skip_brace_level = 0;
            Ogre::DataStreamPtr data = Ogre::DataStreamPtr(new Ogre::MemoryDataStream(bytes.data(), bytes.size()));
            
            while (!data->eof())
            {
                std::string line = data->getLine();
                
                // Skip empty lines & comments
                if ((line.length()) && (line.substr(0, 2) != "//"))
                {
                    // Process opening/closing braces
                    if (!ProcessBraces(line, brace_level))
                    {
                        // If not a brace and on level 0, it should be a new material
                        if ((brace_level == 0) && (line.substr(0, 8) == "material") && (line.length() > 8))
                        {
                            // Close the previous material file
                            if (matoutfile.isOpen())
                                matoutfile.close();
                            
                            std::string matname = line.substr(9);
                            ReplaceCharInplace(matname, '/', '_');
                            line = "material " + matname;
                            if (used_materials.find(matname) == used_materials.end())
                            {
                                TundraLogicModule::LogDebug("Encountered unused material " + matname + " in the material file");
                                known_material = false;
                            }
                            else
                            {
                                known_material = true;
                                ++num_materials;
                                matoutfile.setFileName(QString::fromStdString(out_asset_dir + matname + ".material"));
                                if (!matoutfile.open(QFile::WriteOnly))
                                    TundraLogicModule::LogError("Could not open material file " + matname + ".material for writing");
                            }
                        }
                        else
                        {
                            // Check for textures
                            if (known_material)
                            {
                                if ((line.substr(0, 8) == "texture ") && (line.length() > 8))
                                {
                                    std::string texname = line.substr(8);
                                    used_textures.insert(texname);
                                    if (localassets)
                                        line = "texture file://" + texname;
                                }
                            }
                        }
                        // Write line to the modified copy
                        if ((!skip_until_next) && (matoutfile.isOpen()))
                        {
                            matoutfile.write(line.c_str());
                            matoutfile.write("\n");
                        }
                    }
                    else
                    {
                        // Write line to the modified copy
                        if ((!skip_until_next) && (matoutfile.isOpen()))
                        {
                            matoutfile.write(line.c_str());
                            matoutfile.write("\n");
                        }
                        if (brace_level <= skip_brace_level)
                            skip_until_next = false;
                    }
                }
            }
            
            matoutfile.close();
        }
    }
    
    return used_textures;
}

void SceneImporter::ProcessTextures(const std::set<std::string>& used_textures, const std::string& in_asset_dir, const std::string& out_asset_dir)
{
    // Copy textures
    std::set<std::string>::const_iterator i = used_textures.begin();
    while (i != used_textures.end())
    {
        std::string texname = *i;
        CopyAsset(texname, in_asset_dir, out_asset_dir);
        ++i;
    }
}

bool SceneImporter::CopyAsset(const std::string& name, const std::string& in_asset_dir, const std::string& out_asset_dir)
{
    QFile asset_in((in_asset_dir + name).c_str());
    if (!asset_in.open(QFile::ReadOnly))
    {
        TundraLogicModule::LogError("Could not open input asset file " + name);
        return false;
    }
    else
    {
        QByteArray bytes = asset_in.readAll();
        asset_in.close();
        
        QFile asset_out((out_asset_dir + name).c_str());
        if (!asset_out.open(QFile::WriteOnly))
        {
            TundraLogicModule::LogError("Could not open output asset file " + name);
            return false;
        }
        else
        {
            asset_out.write(bytes);
            asset_out.close();
        }
    }
    
    return true;
}

}

