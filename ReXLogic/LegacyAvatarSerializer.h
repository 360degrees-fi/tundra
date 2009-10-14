// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_RexLogic_LegacyAvatarSerializer_h
#define incl_RexLogic_LegacyAvatarSerializer_h

#include "EC_AvatarAppearance.h"

class QDomDocument;
class QDomElement;

namespace RexLogic
{
    class AvatarAsset;
    
    //! Utility functions for dealing with reX legacy avatar definitions (xml data). Used by RexLogicModule::AvatarAppearance.
    class LegacyAvatarSerializer
    {
    public:
        //! Reads avatar definition into an EC_AvatarAppearance from an xml document
        /*! \param dest Destination EC_AvatarAppearance
            \param source Source XML document
            \param read_mesh Whether to read and overwrite the mesh element, default true
            \return true if mostly successful
         */
        static bool ReadAvatarAppearance(EC_AvatarAppearance& dest, const QDomDocument& source, bool read_mesh = true);
        
        //! Reads animation definitions only from an xml document
        //! \return true if successful
        static bool ReadAnimationDefinitions(AnimationDefinitionMap& dest, const QDomDocument& source);
        
        //! Writes avatar definition from an EC_AvatarAppearance into an xml document
        static void WriteAvatarAppearance(QDomDocument& dest, const EC_AvatarAppearance& source);
        
    private:
        //! Reads a bone modifier set from an xml node, and adds it to the vector
        //! \return true if successful
        static bool ReadBoneModifierSet(BoneModifierSetVector& dest, const QDomElement& elem);
        
        //! Reads a bone modifier parameter from an xml node
        /*! Actual modifiers should have been read before the parameters; this function expects a filled vector of modifier sets
            \return true if successful
         */
        static bool ReadBoneModifierParameter(BoneModifierSetVector& dest, const QDomElement& elem);
        
        //! Reads a morph modifier from an xml node, and adds it to the vector
        //! \return true if successful
        static bool ReadMorphModifier(MorphModifierVector& dest, const QDomElement& elem);
        
        //! Reads a single animation definition from an xml node, and adds it to the map
        //! \return true if successful
        static bool ReadAnimationDefinition(AnimationDefinitionMap& dest, const QDomElement& elem);
        
        //! Reads an avatar attachment from an xml node, and adds it to the vector
        //! \return true if successful
        static bool ReadAttachment(AvatarAttachmentVector& dest, const QDomElement& elem);
        
        //! Writes bone modifiers to xml document
        static void WriteBoneModifierSet(QDomDocument& dest, QDomElement& dest_elem, const BoneModifierSet& bones);
        
        //! Writes a single bone's parameters to an xml element and returns it
        static QDomElement WriteBone(QDomDocument& dest, const BoneModifier& bone);
        
        //! Writes a morph modifier to an xml element and returns it
        static QDomElement WriteMorphModifier(QDomDocument& dest, const MorphModifier& morph);
        
        //! Writes an animation definition to an xml element and returns it
        static QDomElement WriteAnimationDefinition(QDomDocument& dest, const AnimationDefinition& anim);
        
        //! Writes an avatar attachment to an xml element and returns it
        static QDomElement WriteAttachment(QDomDocument& dest, const AvatarAttachment& attachment, const AvatarAsset& mesh);
    };
}

#endif
