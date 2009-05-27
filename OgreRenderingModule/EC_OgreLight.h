// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_OgreRenderer_EC_OgreLight_h
#define incl_OgreRenderer_EC_OgreLight_h

#include "ComponentInterface.h"
#include "Foundation.h"
#include "OgreModuleApi.h"

namespace Ogre
{
    class Light;
}

namespace OgreRenderer
{
    class Renderer;
    class EC_OgrePlaceable;
    
    typedef boost::shared_ptr<Renderer> RendererPtr;
    
    //! Ogre light component
    /*! A light can optionally be attached to a placeable (ie. a scene node) but it can also exist without one.
        \ingroup OgreRenderingModuleClient
     */
    class OGRE_MODULE_API EC_OgreLight : public Foundation::ComponentInterface
    {
        DECLARE_EC(EC_OgreLight);
    public:
        //! light type enumeration
        enum Type
        {
            LT_Point,
            LT_Spot,
            LT_Directional
        };
        
        //! Destructor.
        virtual ~EC_OgreLight();
    
        //! gets placeable component
        Foundation::ComponentPtr GetPlaceable() const { return placeable_; }
        
        //! sets placeable component
        /*! set a null placeable (or do not set a placeable) to have a detached light
            \param placeable placeable component
         */
        void SetPlaceable(Foundation::ComponentPtr placeable);
        
        //! sets type of light
        /*! \param type light type - point, directional or spot
         */
        void SetType(Type type);
        
        //! sets diffuse color of light
        /*! \param color diffuse color value
         */
        void SetColor(const Core::Color& color);

        //! sets light attenuation parameters
        /*! \param range maximum range of light
            \param constant constant attenuation
            \param linear linear attenuation
            \param quad quadratic attenuation
         */
        void SetAttenuation(float range, float constant, float linear, float quad);
        
        //! sets light direction
        /*! does not affect point lights
            \param direction light direction
         */
        void SetDirection(const Core::Vector3df& direction);
        
        //! Whether the light casts shadows or not.
        //! @param enabled Whether the light casts shadows or not.
        void SetCastShadows(const bool &enabled);

        //! @return Ogre light pointer
        Ogre::Light* GetLight() const { return light_; }
        
    private:
        //! constructor
        /*! \param module renderer module
         */
        EC_OgreLight(Foundation::ModuleInterface* module);
        
        //! attaches light to placeable
        void AttachLight();
        
        //! detaches light from placeable
        void DetachLight();
        
        //! placeable component, optional
        Foundation::ComponentPtr placeable_;
        
        //! renderer
        RendererPtr renderer_;
        
        //! Ogre light
        Ogre::Light* light_;
        
        //! light attached to placeable -flag
        bool attached_;
    };
}

#endif
