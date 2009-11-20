// For conditions of distribution and use, see copyright notice in license.txt
// Based on Mesmerizer.cs in libopenmetaverse

/*
 * Copyright (c) 2008, openmetaverse.org
 * All rights reserved.
 *
 * - Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Neither the name of the openmetaverse.org nor the names 
 *   of its contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 * This code comes from the OpenSim project. Meshmerizer is written by dahlia
 * <dahliatrimble@gmail.com>
 */

#include "StableHeaders.h"
#include "Environment/PrimMesher.h"
#include "RexTypes.h"
#include "RexLogicModule.h"
#include "EntityComponent/EC_OpenSimPrim.h"
#include "OgreMaterialUtils.h"
#include "OgreMaterialResource.h"
#include "Renderer.h"

#include <Ogre.h>

namespace RexLogic
{
    void CreatePrimGeometry(Foundation::Framework* framework, Ogre::ManualObject* object, EC_OpenSimPrim& primitive)
    {
        if (!primitive.HasPrimShapeData)
            return;
            
        std::string mat_override;
        if ((primitive.Materials[0].Type == RexTypes::RexAT_MaterialScript) && (!RexTypes::IsNull(primitive.Materials[0].asset_id)))
        {
            mat_override = primitive.Materials[0].asset_id;

            // If cannot find the override material, use default
            // We will probably get resource ready event later for the material & redo this prim
            boost::shared_ptr<OgreRenderer::Renderer> renderer = framework->GetServiceManager()->
                GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
            if (!renderer->GetResource(mat_override, OgreRenderer::OgreMaterialResource::GetTypeStatic()))
            {
                mat_override = "LitTextured";
            }
        }
            
        try
        {
            float profileBegin = primitive.ProfileBegin;
            float profileEnd = 1.0f - primitive.ProfileEnd;
            float profileHollow = primitive.ProfileHollow;

            int sides = 4;
            if ((primitive.ProfileCurve & 0x07) == RexTypes::SHAPE_EQUILATERAL_TRIANGLE)
                sides = 3;
            else if ((primitive.ProfileCurve & 0x07) == RexTypes::SHAPE_CIRCLE)
                sides = 24;
            else if ((primitive.ProfileCurve & 0x07) == RexTypes::SHAPE_HALF_CIRCLE)
            {
                // half circle, prim is a sphere
                sides = 24;

                profileBegin = 0.5f * profileBegin + 0.5f;
                profileEnd = 0.5f * profileEnd + 0.5f;
            }

            int hollowSides = sides;
            if ((primitive.ProfileCurve & 0xf0) == RexTypes::HOLLOW_CIRCLE)
                hollowSides = 24;
            else if ((primitive.ProfileCurve & 0xf0) == RexTypes::HOLLOW_SQUARE)
                hollowSides = 4;
            else if ((primitive.ProfileCurve & 0xf0) == RexTypes::HOLLOW_TRIANGLE)
                hollowSides = 3;

            PrimMesher::PrimMesh primMesh(sides, profileBegin, profileEnd, profileHollow, hollowSides);
            primMesh.topShearX = primitive.PathShearX;
            primMesh.topShearY = primitive.PathShearY;
            primMesh.pathCutBegin = primitive.PathBegin;
            primMesh.pathCutEnd = 1.0f - primitive.PathEnd;

            if (primitive.PathCurve == RexTypes::EXTRUSION_STRAIGHT)
            {
                primMesh.twistBegin = primitive.PathTwistBegin * 180;
                primMesh.twistEnd = primitive.PathTwist * 180;
                primMesh.taperX = primitive.PathScaleX - 1.0f;
                primMesh.taperY = primitive.PathScaleY - 1.0f;
                primMesh.ExtrudeLinear();
            }
            else
            {
                primMesh.holeSizeX = (2.0f - primitive.PathScaleX);
                primMesh.holeSizeY = (2.0f - primitive.PathScaleY);
                primMesh.radius = primitive.PathRadiusOffset;
                primMesh.revolutions = primitive.PathRevolutions;
                primMesh.skew = primitive.PathSkew;
                primMesh.twistBegin = primitive.PathTwistBegin * 360;
                primMesh.twistEnd = primitive.PathTwist * 360;
                primMesh.taperX = primitive.PathTaperX;
                primMesh.taperY = primitive.PathTaperY;
                primMesh.ExtrudeCircular();
            }

            if (object)
            {
                object->clear();
                
                RexTypes::RexAssetID texture_id; 
                RexTypes::RexAssetID prev_texture_id;
                
                Core::uint indices = 0;
                bool first_face = true;
                
                //! \bug: for boxes, up and down faces are swapped, judging from applying a different texture to all faces. Seems to be a PrimMesher issue.
                for (int i = 0; i < primMesh.viewerFaces.size(); ++i)
                {
                    Core::Color color = primitive.PrimDefaultColor;
                    ColorMap::const_iterator c = primitive.PrimColors.find(primMesh.viewerFaces[i].primFaceNumber);
                    if (c != primitive.PrimColors.end())
                        color = c->second;

                    // Skip face if very transparent
                    if (color.a <= 0.11f)
                        continue;
                    
                    if (!mat_override.empty())
                        texture_id = mat_override;
                    else
                    {
                        unsigned variation = OgreRenderer::LEGACYMAT_VERTEXCOL;
                        
                        // Check for transparency
                        if (color.a < 1.0f)
                            variation = OgreRenderer::LEGACYMAT_VERTEXCOLALPHA;
                        
                        // Check for fullbright
                        bool fullbright = (primitive.PrimDefaultMaterialType & RexTypes::MATERIALTYPE_FULLBRIGHT) != 0;
                        MaterialTypeMap::const_iterator mt = primitive.PrimMaterialTypes.find(primMesh.viewerFaces[i].primFaceNumber);
                        if (mt != primitive.PrimMaterialTypes.end())
                            fullbright = (mt->second & RexTypes::MATERIALTYPE_FULLBRIGHT) != 0;
                        if (fullbright)
                            variation |= OgreRenderer::LEGACYMAT_FULLBRIGHT;
                        
                        std::string suffix = OgreRenderer::GetMaterialSuffix(variation);
                        
                        // Try to find face's texture in texturemap, use default if not found
                        texture_id = primitive.PrimDefaultTextureID + suffix;
                        TextureMap::const_iterator t = primitive.PrimTextures.find(primMesh.viewerFaces[i].primFaceNumber);
                        if (t != primitive.PrimTextures.end())
                            texture_id = t->second + suffix;
                        // Actually create the material here if texture yet missing, the material will be
                        // updated later
                        OgreRenderer::CreateLegacyMaterials(texture_id);
                    }
                    
                    if ((first_face) || (texture_id != prev_texture_id))
                    {
                        if (indices)
                            object->end();

                        indices = 0;
                        
                        object->begin(texture_id, Ogre::RenderOperation::OT_TRIANGLE_LIST);
                        prev_texture_id = texture_id;
                        first_face = false;
                    }
                    
                    Ogre::Vector3 pos1(primMesh.viewerFaces[i].v1.X, primMesh.viewerFaces[i].v1.Y, primMesh.viewerFaces[i].v1.Z);
                    Ogre::Vector3 pos2(primMesh.viewerFaces[i].v2.X, primMesh.viewerFaces[i].v2.Y, primMesh.viewerFaces[i].v2.Z);
                    Ogre::Vector3 pos3(primMesh.viewerFaces[i].v3.X, primMesh.viewerFaces[i].v3.Y, primMesh.viewerFaces[i].v3.Z);

                    Ogre::Vector3 n1(primMesh.viewerFaces[i].n1.X, primMesh.viewerFaces[i].n1.Y, primMesh.viewerFaces[i].n1.Z);
                    Ogre::Vector3 n2(primMesh.viewerFaces[i].n2.X, primMesh.viewerFaces[i].n2.Y, primMesh.viewerFaces[i].n2.Z);
                    Ogre::Vector3 n3(primMesh.viewerFaces[i].n3.X, primMesh.viewerFaces[i].n3.Y, primMesh.viewerFaces[i].n3.Z);
                    
                    Ogre::Vector2 uv1(primMesh.viewerFaces[i].uv1.U, primMesh.viewerFaces[i].uv1.V);
                    Ogre::Vector2 uv2(primMesh.viewerFaces[i].uv2.U, primMesh.viewerFaces[i].uv2.V);
                    Ogre::Vector2 uv3(primMesh.viewerFaces[i].uv3.U, primMesh.viewerFaces[i].uv3.V);

                    object->position(pos1);
                    object->normal(n1);
                    object->textureCoord(uv1);
                    object->colour(color.r, color.g, color.b, color.a);
                    
                    object->position(pos2);
                    object->normal(n2);
                    object->textureCoord(uv2);
                    object->colour(color.r, color.g, color.b, color.a);
                    
                    object->position(pos3);
                    object->normal(n3);
                    object->textureCoord(uv3);
                    object->colour(color.r, color.g, color.b, color.a);
                    
                    object->index(indices++);
                    object->index(indices++);
                    object->index(indices++);
                }
                
                // End last subsection
                if (indices)
                    object->end();
            }
            else
            {
                RexLogicModule::LogError(std::string("Null manualobject passed to CreatePrimGeometry"));
            }
        }
        catch (Core::Exception& e)
        {
            RexLogicModule::LogError(std::string("Exception while creating primitive geometry: ") + e.what());
        }
    }
}