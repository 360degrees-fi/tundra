// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_NetworkEventHandler_h
#define incl_NetworkEventHandler_h

#include "ComponentInterface.h"
#include "Foundation.h"
#include "NetworkEvents.h"

namespace ProtocolUtilities
{
    class ProtocolModuleInterface;
}

namespace RexLogic
{
    struct DecodedTerrainPatch;
    class RexLogicModule;

    /// Handles incoming SLUDP network events in a reX-specific way. \todo Break down into more logical functions.
    class NetworkEventHandler
    {
    public:
        NetworkEventHandler(Foundation::Framework *framework, RexLogicModule *rexlogicmodule);
        virtual ~NetworkEventHandler();

        // !Handle network events coming from OpenSimProtocolModule
        bool HandleOpenSimNetworkEvent(Core::event_id_t event_id, Foundation::EventDataInterface* data);
    private:
        // !Handler functions for Opensim network events
        bool HandleOSNE_AgentMovementComplete(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_GenericMessage(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_ImprovedTerseObjectUpdate(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_KillObject(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_LogoutReply(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_ObjectUpdate(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_RegionHandshake(ProtocolUtilities::NetworkEventInboundData* data);
        bool HandleOSNE_InventoryDescendents(ProtocolUtilities::NetworkEventInboundData* data);

        //! Handler functions for GenericMessages

        void DebugCreateTerrainVisData(const DecodedTerrainPatch &heightData, int patchSize);

        Foundation::Framework *framework_;

		boost::weak_ptr<ProtocolUtilities::ProtocolModuleInterface> protocolModule_;

        RexLogicModule *rexlogicmodule_;
    };
}

#endif
