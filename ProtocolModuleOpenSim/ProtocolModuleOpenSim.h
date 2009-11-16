// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_ProtocolModuleOpenSim_ProtocolModuleOpenSim_h
#define incl_ProtocolModuleOpenSim_ProtocolModuleOpenSim_h

#include "Foundation.h"
#include "ModuleInterface.h"
#include "ProtocolModuleOpenSimApi.h"
#include "OpenSimLoginThread.h"

#include "NetworkMessages/NetMessageManager.h"
#include "Interfaces/INetMessageListener.h"
#include "Interfaces/ProtocolModuleInterface.h"
#include "NetworkEvents.h"
#include "NetworkConnection.h"

#include "RexUUID.h"

namespace OpenSimProtocol
{
    /** \defgroup OpenSimProtocolClient OpenSimProtocol Client Interface
        This page lists the public interface of the OpenSimProtocol module. Use
        this module to track the server connection state as well as to 
        communicate with the server using the SLUDP protocol.

        For an introduction to how to work with this module, see
        \ref OpenSimProtocolConnection "Managing the OpenSim connection state"
        and \ref SLUDP "Interfacing with the OpenSim world using SLUDP messages."

        @{
    */

    /// OpenSimProtocolModule exposes other modules with the funtionality of
    /// communicating with the OpenSim server using the SLUDP protocol. It
    /// also handles the XMLRPC handshakes with the server.
	class OSPROTO_MODULE_API ProtocolModuleOpenSim 
		: public Foundation::ModuleInterfaceImpl, 
		  public ProtocolUtilities::INetMessageListener, 
		  public ProtocolUtilities::ProtocolModuleInterface
    {
    public: 
        ProtocolModuleOpenSim();
        virtual ~ProtocolModuleOpenSim();

        virtual void Load();
        virtual void Unload();
        virtual void Initialize();
        virtual void Uninitialize();
        virtual void Update(Core::f64 frametime);
		
        MODULE_LOGGING_FUNCTIONS

        //! Returns name of this module. Needed for logging.
        static const std::string &NameStatic() { return Foundation::Module::NameFromType(type_static_); }

        //! Returns type of this module. Needed for logging.
        static const Foundation::Module::Type type_static_ = Foundation::Module::MT_OpenSimProtocol;

        /// Passes inbound network events to listeners.
        virtual void OnNetworkMessageReceived(ProtocolUtilities::NetMsgID msgID, ProtocolUtilities::NetInMessage *msg);

        /// Passes outbound network events to listeners. Used for stats/debugging.
        virtual void OnNetworkMessageSent(const ProtocolUtilities::NetOutMessage *msg);

        /// Dumps network message to the console.
		void DumpNetworkMessage(ProtocolUtilities::NetMsgID id, ProtocolUtilities::NetInMessage *msg);

		/// Gets the modules loginworker
		/// @return loginworker_
		OpenSimLoginThread* GetLoginWorker() { return &loginWorker_; }

		////////////////////////////////////////////////
		/*** ProtocolModuleInterface implementation ***/
		
		//! Function for registering network event
		virtual void RegisterNetworkEvents();
		
		//! Function for uniregistering networking
		virtual void UnregisterNetworkEvents();

        /// Creates the UDP connection to the server.
        ///@ return True, if the connection was succesfull, false otherwise.
        virtual bool CreateUdpConnection(const char *address, int port);

        ///@return Connection::State enum of the connection state.
		virtual ProtocolUtilities::Connection::State GetConnectionState() const { return loginWorker_.GetState(); }
       
		/// Returns client parameters of current connection
		virtual const ProtocolUtilities::ClientParameters& GetClientParameters() const { return clientParameters_; }

		/// Sets new capability.
        /// @param name Name of capability.
        /// @param url URL of the capability.
        virtual void SetCapability(const std::string &name, const std::string &url);

        /// Returns URL of the requested capability, or null string if the capability doens't exist.
        /// @param name Name of the capability.
        /// @return Capability URL.
        virtual std::string GetCapability(const std::string &name);
		
		/// Sets Authentication type
		/// @params authentivation type ProtocolUtilities::AuthenticationType
		virtual void SetAuthenticationType(ProtocolUtilities::AuthenticationType aType) { authenticationType_ = aType; }

        ///@return True if connection exists.
        virtual bool IsConnected() const { return connected_; }

        /// Disconnects from a reX server.
        virtual void DisconnectFromServer();
        
		/// Start building a new outbound message.
        /// @return An empty message holder where the message can be built.
		virtual ProtocolUtilities::NetOutMessage *StartMessageBuilding(ProtocolUtilities::NetMsgID msgId);

        /// Finishes (sends) the message. The passed msg pointer will be invalidated after calling this, so don't
        /// access it or hold on to it afterwards. The user doesn't have to do any deallocation, it is all managed by
        /// this class.
        virtual void FinishMessageBuilding(ProtocolUtilities::NetOutMessage *msg);

	private:
        /// Requests capabilities from the server.
        /// @param seed Seed capability URL.
        void RequestCapabilities(const std::string &seed);

        /// Extracts capabilities from XML string
        /// @param xml XML string from the server.
        void ExtractCapabilitiesFromXml(std::string xml);

        /// Thread for the login process.
        Core::Thread thread_;

        /// Object which handles the XML-RPC login procedure.
        OpenSimLoginThread loginWorker_;

        /// Handles the UDP communications with the reX server.
		boost::shared_ptr<ProtocolUtilities::NetMessageManager> networkManager_;

        /// State of the network connection.
        bool connected_;

        /// Authentication type (Taiga/OpenSim/RealXtend)
        ProtocolUtilities::AuthenticationType authenticationType_;

        /// Event manager.
        Foundation::EventManagerPtr eventManager_;

        /// Network state event category.
        Core::event_category_id_t networkStateEventCategory_;

        /// Network event category for inbound messages.
        Core::event_category_id_t networkEventInCategory_;

        /// Network event category for outbound messages.
        Core::event_category_id_t networkEventOutCategory_;

        /// Current connection client-spesific parameters.
        ProtocolUtilities::ClientParameters clientParameters_;

        ///Typedefs for capability map.
        typedef std::map<std::string, std::string> caps_map_t;
        typedef std::map<std::string, std::string>::iterator caps_map_it_t;

        /// Server-spesific capabilities.
        caps_map_t capabilities_;
    };

    /// @}
}


#endif // incl_ProtocolModuleOpenSim_ProtocolModuleOpenSim_h
