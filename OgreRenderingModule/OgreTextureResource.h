// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_OgreRenderer_OgreTextureResource_h
#define incl_OgreRenderer_OgreTextureResource_h

#include "TextureInterface.h"
#include "OgreModuleApi.h"
 
#include <OgreTexture.h>

namespace OgreRenderer
{
    class OgreTextureResource;
    typedef boost::shared_ptr<OgreTextureResource> OgreTextureResourcePtr;

    //! An Ogre-specific texture resource
    /*! \ingroup OgreRenderingModuleClient
     */
    class OGRE_MODULE_API OgreTextureResource : public Foundation::ResourceInterface
    {
    public:
        //! constructor
        /*! \param id texture id
         */
        OgreTextureResource(const std::string& id);
        
        //! constructor
        /*! \param id texture id
            \param source source raw texture data
        */
        OgreTextureResource(const std::string& id, Foundation::TexturePtr source);

        //! destructor
        virtual ~OgreTextureResource();

        //! returns resource type in text form
        virtual const std::string& GetType() const;

        //! returns Ogre texture
        /*! may be null if no data successfully set yet
         */
        Ogre::TexturePtr GetTexture() const { return ogre_texture_; }

        //! returns quality level
        int GetLevel() const { return level_; }

        //! sets contents from raw source texture
        /*! \param source source raw texture data
            \return true if successful
        */
        bool SetData(Foundation::TexturePtr source);

        //! returns resource type in text form (static)
        static const std::string& GetTypeStatic();

    private:
        //! Ogre texture
        Ogre::TexturePtr ogre_texture_;

        //! quality level, 0 = highest, -1 = no content yet
        int level_;
    };
}
#endif
