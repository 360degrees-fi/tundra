// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_OgreRenderer_EC_OgrePlaceable_h
#define incl_OgreRenderer_EC_OgrePlaceable_h

#include "Foundation.h"
#include "ComponentInterface.h"
#include "OgreModuleApi.h"

namespace Ogre
{
    class SceneNode;
}

namespace OgreRenderer
{
    class Renderer;
    
    typedef boost::shared_ptr<Renderer> RendererPtr;
    typedef boost::weak_ptr<Renderer> RendererWeakPtr;
    
    //! Ogre placeable (scene node) component
    /*! \ingroup OgreRenderingModuleClient
     */
    class OGRE_MODULE_API EC_OgrePlaceable : public Foundation::ComponentInterface
    {
        Q_OBJECT
        
        DECLARE_EC(EC_OgrePlaceable);
    public:
        virtual ~EC_OgrePlaceable();

        //! sets parent placeable
        /*! set null placeable to attach to scene root (the default)
            \param placeable new parent
         */
        void SetParent(Foundation::ComponentPtr placeable);
        
        //! sets position
        /*! \param position new position
         */
        void SetPosition(const Vector3df& position);
        //! sets orientation
        /*! \param orientation new orientation
         */
        void SetOrientation(const Quaternion& orientation);

        //! orients to look at a point in space
        /*! \param look_at point to look at
         */
        void LookAt(const Vector3df& look_at);        
        //! yaws the node
        /*! \param radians how many radians to yaw
         */
        void Yaw(Real radians);
        //! pitches the node
        /*! \param radians how many radians to yaw
         */
        void Pitch(Real radians);        
        //! rolls the node
        /*! \param radians how many radians to yaw
         */
        void Roll(Real radians);           
        
        //! sets scale
        /*! \param scale new scale
         */
        void SetScale(const Vector3df& scale);

        //! sets select priority
        /*! \param priority new select priority
         */
        void SetSelectPriority(int priority) { select_priority_ = priority; }
        
        //! gets parent placeable
        Foundation::ComponentPtr GetParent() { return parent_; }
        
        //! returns position
        Vector3df GetPosition() const;
        //! returns orientation
        Quaternion GetOrientation() const;
        //! returns scale
        Vector3df GetScale() const;
        
        //! returns Ogre scenenode for attaching geometry.
        /*! Do not manipulate the pos/orientation/scale of this node directly
         */
        Ogre::SceneNode* GetSceneNode() const { return scene_node_; }
       
        //! returns Ogre scenenode for linking another placeable in OpenSim compatible way.
        /*! Only used by other placeables, or in other rare cases. Do not use if GetSceneNode() works for you,
            as this doesn't take scaling into account!
         */
        Ogre::SceneNode* GetLinkSceneNode() const { return link_scene_node_; }
                       
        //! returns select priority
        int GetSelectPriority() const { return select_priority_; }
        
    private:
        //! constructor
        /*! \param module renderer module
         */
        explicit EC_OgrePlaceable(Foundation::ModuleInterface* module);
        
        //! attaches scenenode to parent
        void AttachNode();
        
        //! detaches scenenode from parent
        void DetachNode();
        
        //! renderer
        RendererWeakPtr renderer_;
        
        //! parent placeable
        Foundation::ComponentPtr parent_;
        
        //! Ogre scene node for geometry. scale is handled here
        Ogre::SceneNode* scene_node_;

        //! Ogre scene node for linking. position & orientation are handled here, and the geometry scene node is attached to this
        Ogre::SceneNode* link_scene_node_;
        
        //! attached to scene hierarchy-flag
        bool attached_;
        
        //! selection priority for picking
        int select_priority_;
    };
}

#endif
