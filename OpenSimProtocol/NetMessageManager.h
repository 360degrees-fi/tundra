// For conditions of distribution and use, see copyright notice in license.txt
#ifndef incl_NetMessageManager_h
#define incl_NetMessageManager_h

#include <list>
#include <set>
#include <boost/shared_ptr.hpp>

#include "NetworkConnection.h"
#include "NetInMessage.h"
#include "NetOutMessage.h"
#include "NetMessage.h"
#include "INetMessageListener.h"

/// Manages both in- and outbound UDP communication. Implements a packet queue, packet sequence numbering, ACKing,
/// pinging, and reliable communications. reX-protocol specific. Used internally by OpenSimProtocolModule, external
/// module users don't need to work on this.
class NetMessageManager
{
public:
	/// The message manager starts in a disconnected state.
	/// @param The filename to take the message definitions from.
	NetMessageManager(const char *messageListFilename);
	~NetMessageManager();

	/// Connects to the given server.
	bool ConnectTo(const char *serverAddress, int port);
	
	/// Disconnets from the current server.
	void Disconnect();

	/// To start building a new outbound message, call this.
	/// @return An empty message holder where the message can be built.
	NetOutMessage *StartNewMessage(NetMsgID msgId);
	
	/// To tell the manager that building the message is now finished and can be put into the outbound queue, call this.
	void FinishMessage(NetOutMessage *message);
	
	/// Reads in all inbound UDP messages and processes them forward to the application through the listener.
	/// Checks and resends any timed out reliable outbound messages. This could be moved into a separate thread, but not that timing specific so not necessary atm.
	void ProcessMessages();

	/// Interprets the given byte stream as a message and dumps it contents out to the log. Useful only for diagnostics and such.
	void DumpNetworkMessage(const uint8_t *data, size_t numBytes);
	void DumpNetworkMessage(NetMsgID id, NetInMessage *msg);

#ifndef RELEASE
	void DebugSendHardcodedTestPacket();
	void DebugSendHardcodedRandomPacket(size_t numBytes);
#endif

	/// Sets the object that receives the network packets. Replaces the old. Currently supports only one listener.
	/// \todo weakptr'ize. \todo delegate/event \todo pub/sub or something else.
	void RegisterNetworkListener(INetMessageListener *listener) { messageListener = listener; }
	void UnregisterNetworkListener(INetMessageListener *listener) { messageListener = 0; }

private:
	/// @return A new sequence number for outbound UDP messages.
	size_t GetNewSequenceNumber() { return sequenceNumber++; }

	/// Sends a PacketAck message to the server, Acking the packet with the given packetID.
	void SendPacketACK(uint32_t packetID);
	
	/// Processes a received PacketAck message.
	void ProcessPacketACK(NetInMessage *msg);
	
	/// Responds to a ping check from the server with a CompletePingCheck message.
	void SendCompletePingCheck(uint8_t pingID);

	/// Called to send out a message that is already binary-mangled to the proper final format. (packet number, zerocoding, flags, ...)
	void SendProcessedMessage(NetOutMessage *msg);

	/// Adds message to the queue of reliable outbound messages.
	void AddMessageToResendQueue(NetOutMessage *msg);
	
	/// Removes message from the queue of reliable outbound messages.
	void RemoveMessageFromResendQueue(uint32_t packetID);
	
	/// @return True, if the resend queue is empty, false otherwise.
	bool ResendQueueIsEmpty() const { return messageResendQueue.empty(); }

	/// Checks each reliable message in outbound queue and resends any of the if an Ack was not received within a time-out period.
	void ProcessResendQueue();

	NetMessageManager(const NetMessageManager &);
	void operator=(const NetMessageManager &);

	/// All incoming UDP packets are routed to this handler.
	INetMessageListener *messageListener;

	/// The socket for the UDP connection.
	boost::shared_ptr<NetworkConnection> connection;

	/// List of messages this manager can handle.
	boost::shared_ptr<NetMessageList> messageList;

	/// A pool of allocated unused NetOutMessage structures. Used to avoid unnecessary allocations at runtime.
	std::list<NetOutMessage*> unusedMessagePool;

	/// A pool of NetOutMessage structures, which have been handed out to the application and are currently being built.
	std::list<NetOutMessage*> usedMessagePool;

	typedef std::list<std::pair<time_t, NetOutMessage*> > MessageResendList;
	/// A pool of NetOutMessages that are in the outbound queue. Need to keep the unacked reliable messages in
	/// memory for possible resending.
	MessageResendList messageResendQueue;
	
	/// A running sequence number for outbound messages.
	size_t sequenceNumber;
	
	/// A set of received messages' sequence numbers.
	std::set<uint32_t> receivedSequenceNumbers;
};

#endif
