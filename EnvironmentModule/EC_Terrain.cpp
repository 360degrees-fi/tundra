// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "EC_Terrain.h"

#include <Ogre.h>
#include "Renderer.h"

namespace Environment
{
    EC_Terrain::EC_Terrain(Foundation::ModuleInterface* module)
    :Foundation::ComponentInterface(module->GetFramework()),
    framework_(module->GetFramework())
    {
        assert(framework_);    
    }

    EC_Terrain::~EC_Terrain()
    {
        Destroy();
    }

    void EC_Terrain::Destroy()
    {
        assert(framework_);

        boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetServiceManager()->GetService
            <OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
        if (!renderer) // Oops! Inconvenient dtor order - can't delete our own stuff since we can't get an instance to the owner.
            return;
            
        Ogre::SceneManager *sceneMgr = renderer->GetSceneManager();
        if (!sceneMgr) // Oops! Same as above.
            return;

        for(int y = 0; y < cNumPatchesPerEdge; ++y)
            for(int x = 0; x < cNumPatchesPerEdge; ++x)
            {
                Ogre::SceneNode *node = GetPatch(x, y).node;
                if (!node)
                    continue;

                sceneMgr->getRootSceneNode()->removeChild(node);
//                sceneMgr->destroyManualObject(dynamic_cast<Ogre::ManualObject*>(node->getAttachedObject(0)));
                node->detachAllObjects();
                sceneMgr->destroySceneNode(node);
            }
    }
    
    float EC_Terrain::GetPoint(int x, int y) const
    {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= cNumPatchesPerEdge * 16) x = cNumPatchesPerEdge * 16 - 1;
        if (y >= cNumPatchesPerEdge * 16) y = cNumPatchesPerEdge * 16 - 1;

        return GetPatch(x / cPatchSize, y / cPatchSize).heightData[(y % cPatchSize) * cPatchSize + (x % cPatchSize)];
    }
    
    Core::Vector3df EC_Terrain::CalculateNormal(int x, int y, int xinside, int yinside)
    {
        int px = x * cPatchSize + xinside;
        int py = y * cPatchSize + yinside;
        
        int xNext = Core::clamp(px+1, 0, cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge - 1);
        int yNext = Core::clamp(py+1, 0, cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge - 1);
        int xPrev = Core::clamp(px-1, 0, cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge - 1);
        int yPrev = Core::clamp(py-1, 0, cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge - 1);

        float x_slope = GetPoint(xPrev, py) - GetPoint(xNext, py);
        if ((px <= 0) || (px >= cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge))
            x_slope *= 2;
        float y_slope = GetPoint(px, yPrev) - GetPoint(px, yNext);
        if ((py <= 0) || (py >= cNumPatchesPerEdge * Patch::cNumVerticesPerPatchEdge))
            y_slope *= 2;
        
        Core::Vector3df normal(x_slope, y_slope, 2.0);
        normal.normalize();
        return normal;
    }
}
