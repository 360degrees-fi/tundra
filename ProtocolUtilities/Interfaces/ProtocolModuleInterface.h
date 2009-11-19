// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_ProtocolUtilities_ProtocolModuleInterface_h
#define incl_ProtocolUtilities_ProtocolModuleInterface_h

#include "Foundation.h"
#include "CoreDefines.h"
#include "NetworkEvents.h"

namespace ProtocolUtilities
{
    class MODULE_API ProtocolModuleInterface
    {

    public:
        //! Function for registering network event
        virtual void RegisterNetworkEvents() = 0;

        //! Function for uniregistering networking
        virtual void UnregisterNetworkEvents() = 0;

        /// Creates the UDP connection to the server.
        ///@ return True, if the connection was succesfull, false otherwise.
        virtual bool CreateUdpConnection(const char *address, int port) = 0;

        /// @return Connection::State enum of the connection state.
        virtual Connection::State GetConnectionState() const = 0;

        /// @return Client parameters of current connection
        virtual const ClientParameters& GetClientParameters() const = 0;
        
        /// Sets new capability.
        /// @param name Name of capability.
        /// @param url URL of the capability.
        virtual void SetCapability(const std::string &name, const std::string &url) = 0;

        /// Returns URL of the requested capability, or null string if the capability doens't exist.
        /// @param name Name of the capability.
        /// @return Capability URL.
        virtual std::string GetCapability(const std::string &name) = 0;

        /// Sets Authentication type
        /// @params authentivation type ProtocolUtilities::AuthenticationType
        virtual void SetAuthenticationType(AuthenticationType aType) = 0;

        ///@return True if connection exists.
        virtual bool IsConnected() const = 0;

        /// Disconnects from the server.
        virtual void DisconnectFromServer() = 0;

        /// Start building a new outbound message.
        /// @return An empty message holder where the message can be built.
        virtual NetOutMessage *StartMessageBuilding(NetMsgID msgId) = 0;

        /// Finishes (sends) the message. The passed msg pointer will be invalidated after calling this, so don't
        /// access it or hold on to it afterwards. The user doesn't have to do any deallocation, it is all managed by
        /// this class.
        virtual void FinishMessageBuilding(NetOutMessage *msg) = 0;
    };
}

#endif // incl_ProtocolUtilities_ProtocolModuleInterface_h