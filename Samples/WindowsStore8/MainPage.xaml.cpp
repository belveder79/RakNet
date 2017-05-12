/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace RakNetWS10VS2015;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

/* // ORIGINAL
#include "RakPeerInterface.h"
#include "RakSleep.h"
#include "MessageIdentifiers.h"
using namespace RakNet;
#define DEFAULT_SERVER_PORT 10000
#define DEFAULT_SERVER_ADDRESS "52.59.145.248"
*/

#include "RakPeerInterface.h"
#include "RakSleep.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include "Kbhit.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakSleep.h"
#include "NatPunchthroughClient.h"
#include "NatTypeDetectionClient.h"
#include "Getche.h"
#include "GetTime.h"
#include "UDPProxyCommon.h"
#include "UDPProxyClient.h"
#include "Gets.h"
#include "Itoa.h"

using namespace RakNet;

#define DEFAULT_RAKPEER_PORT 50000
#define RAKPEER_PORT_STR "0"
#define DEFAULT_SERVER_PORT "10000"
#define DEFAULT_SERVER_ADDRESS "52.59.145.248"

enum SampleResult
{
	PENDING,
	FAILED,
	SUCCEEDED
};

#define SUPPORT_NAT_TYPE_DETECTION FAILED
#define SUPPORT_NAT_PUNCHTHROUGH PENDING
#define SUPPORT_UDP_PROXY FAILED

struct SampleFramework
{
	virtual const char * QueryName(void) = 0;
	virtual bool QueryRequiresServer(void) = 0;
	virtual const char * QueryFunction(void) = 0;
	virtual const char * QuerySuccess(void) = 0;
	virtual bool QueryQuitOnSuccess(void) = 0;
	virtual void Init(RakNet::RakPeerInterface *rakPeer) = 0;
	virtual void ProcessPacket(Packet *packet) = 0;
	virtual void Update(RakNet::RakPeerInterface *rakPeer) = 0;
	virtual void Shutdown(RakNet::RakPeerInterface *rakPeer) = 0;

	SampleResult sampleResult;
};

SystemAddress SelectAmongConnectedSystems(RakNet::RakPeerInterface *rakPeer, const char *hostName)
{
	DataStructures::List<SystemAddress> addresses;
	DataStructures::List<RakNetGUID> guids;
	rakPeer->GetSystemList(addresses, guids);
	if (addresses.Size() == 0)
	{
		return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}
	if (addresses.Size()>1)
	{
		printf("Select IP address for %s.\n", hostName);
		char buff[64];
		for (unsigned int i = 0; i < addresses.Size(); i++)
		{
			addresses[i].ToString(true, buff);
			printf("%i. %s\n", i + 1, buff);
		}
		Gets(buff, sizeof(buff));
		if (buff[0] == 0)
		{
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		unsigned int idx = atoi(buff);
		if (idx <= 0 || idx > addresses.Size())
		{
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		return addresses[idx - 1];
	}
	else
		return addresses[0];
};
SystemAddress ConnectBlocking(RakNet::RakPeerInterface *rakPeer, const char *hostName, const char *defaultAddress, const char *defaultPort)
{
	char ipAddr[64];
	if (defaultAddress == 0 || defaultAddress[0] == 0)
		printf("Enter IP of system %s is running on: ", hostName);
	else
		printf("Enter IP of system %s, or press enter for default: ", hostName);
	Gets(ipAddr, sizeof(ipAddr));
	if (ipAddr[0] == 0)
	{
		if (defaultAddress == 0 || defaultAddress[0] == 0)
		{
			printf("Failed. No address entered for %s.\n", hostName);
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		else
		{
			strcpy(ipAddr, defaultAddress);
		}
	}
	char port[64];
	if (defaultAddress == 0 || defaultAddress[0] == 0)
		printf("Enter port of system %s is running on: ", hostName);
	else
		printf("Enter port of system %s, or press enter for default: ", hostName);
	Gets(port, sizeof(port));
	if (port[0] == 0)
	{
		if (defaultPort == 0 || defaultPort[0] == 0)
		{
			printf("Failed. No port entered for %s.\n", hostName);
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		else
		{
			strcpy(port, defaultPort);
		}
	}
	if (rakPeer->Connect(ipAddr, atoi(port), 0, 0) != RakNet::CONNECTION_ATTEMPT_STARTED)
	{
		printf("Failed connect call for %s.\n", hostName);
		return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}
	printf("Connecting...\n");
	RakNet::Packet *packet;
	while (1)
	{
		for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
		{
			if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
			{
				return packet->systemAddress;
			}
			else if (packet->data[0] == ID_NO_FREE_INCOMING_CONNECTIONS)
			{
				printf("ID_NO_FREE_INCOMING_CONNECTIONS");
				return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
			}
			else
			{
				return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
			}
			RakSleep(100);
		}
	}
}

struct NatTypeDetectionFramework : public SampleFramework
{
	// Set to FAILED to skip this test
	NatTypeDetectionFramework() { sampleResult = SUPPORT_NAT_TYPE_DETECTION; ntdc = 0; }
	virtual const char * QueryName(void) { return "NatTypeDetectionFramework"; }
	virtual bool QueryRequiresServer(void) { return true; }
	virtual const char * QueryFunction(void) { return "Determines router type to avoid NAT punch attempts that cannot\nsucceed."; }
	virtual const char * QuerySuccess(void) { return "If our NAT type is Symmetric, we can skip NAT punch to other symmetric NATs."; }
	virtual bool QueryQuitOnSuccess(void) { return false; }
	virtual void Init(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		SystemAddress serverAddress = SelectAmongConnectedSystems(rakPeer, "NatTypeDetectionServer");
		if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			serverAddress = ConnectBlocking(rakPeer, "NatTypeDetectionServer", DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT);
			if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			{
				printf("Failed to connect to a server.\n");
				sampleResult = FAILED;
				return;
			}
		}
		ntdc = new NatTypeDetectionClient;
		rakPeer->AttachPlugin(ntdc);
		ntdc->DetectNATType(serverAddress);
		timeout = RakNet::GetTimeMS() + 5000;
	}

	virtual void ProcessPacket(Packet *packet)
	{
		if (packet->data[0] == ID_NAT_TYPE_DETECTION_RESULT)
		{
			RakNet::NATTypeDetectionResult r = (RakNet::NATTypeDetectionResult) packet->data[1];
			printf("NAT Type is %s (%s)\n", NATTypeDetectionResultToString(r), NATTypeDetectionResultToStringFriendly(r));
			printf("Using NATPunchthrough can connect to systems using:\n");
			for (int i = 0; i < (int)RakNet::NAT_TYPE_COUNT; i++)
			{
				if (CanConnect(r, (RakNet::NATTypeDetectionResult)i))
				{
					if (i != 0)
						printf(", ");
					printf("%s", NATTypeDetectionResultToString((RakNet::NATTypeDetectionResult)i));
				}
			}
			printf("\n");
			if (r == RakNet::NAT_TYPE_PORT_RESTRICTED || r == RakNet::NAT_TYPE_SYMMETRIC)
			{
				// For UPNP, see Samples\UDPProxy
				printf("Note: Your router must support UPNP or have the user manually forward ports.\n");
				printf("Otherwise NATPunchthrough may not always succeed.\n");
			}

			sampleResult = SUCCEEDED;
		}
	}
	virtual void Update(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		if (sampleResult == PENDING && RakNet::GetTimeMS()>timeout)
		{
			printf("No response from the server, probably not running NatTypeDetectionServer plugin.\n");
			sampleResult = FAILED;
		}
	}
	virtual void Shutdown(RakNet::RakPeerInterface *rakPeer)
	{
		delete ntdc;
		ntdc = 0;
	}

	NatTypeDetectionClient *ntdc;
	RakNet::TimeMS timeout;
};

struct NatPunchthoughClientFramework : public SampleFramework, public NatPunchthroughDebugInterface_Printf
{
	SystemAddress serverAddress;

	// Set to FAILED to skip this test
	NatPunchthoughClientFramework() { sampleResult = SUPPORT_NAT_PUNCHTHROUGH; npClient = 0; }
	virtual const char * QueryName(void) { return "NatPunchthoughClientFramework"; }
	virtual bool QueryRequiresServer(void) { return true; }
	virtual const char * QueryFunction(void) { return "Causes two systems to try to connect to each other at the same\ntime, to get through routers."; }
	virtual const char * QuerySuccess(void) { return "We can now communicate with the other system, including connecting."; }
	virtual bool QueryQuitOnSuccess(void) { return true; }
	virtual void Init(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		serverAddress = SelectAmongConnectedSystems(rakPeer, "NatPunchthroughServer");
		if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			serverAddress = ConnectBlocking(rakPeer, "NatPunchthroughServer", DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT);
			if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			{
				printf("Failed to connect to a server.\n");
				sampleResult = FAILED;
				return;
			}
		}

		npClient = new NatPunchthroughClient;
		npClient->SetDebugInterface(this);
		rakPeer->AttachPlugin(npClient);


		char guid[128];
		printf("Enter RakNetGuid of the remote system, which should have already connected\nto the server.\nOr press enter to just listen.\n");
		Gets(guid, sizeof(guid));
		if (guid[0])
		{
			RakNetGUID remoteSystemGuid;
			remoteSystemGuid.FromString(guid);
			npClient->OpenNAT(remoteSystemGuid, serverAddress);
			isListening = false;

			timeout = RakNet::GetTimeMS() + 10000;
		}
		else
		{
			printf("Listening\n");
			printf("My GUID is %s\n", rakPeer->GetMyGUID().ToString());
			isListening = true;

			// Find the stride of our router in advance
			npClient->FindRouterPortStride(serverAddress);

		}
	}

	virtual void ProcessPacket(Packet *packet)
	{
		if (
			packet->data[0] == ID_NAT_TARGET_NOT_CONNECTED ||
			packet->data[0] == ID_NAT_TARGET_UNRESPONSIVE ||
			packet->data[0] == ID_NAT_CONNECTION_TO_TARGET_LOST ||
			packet->data[0] == ID_NAT_PUNCHTHROUGH_FAILED
			)
		{
			RakNetGUID guid;
			if (packet->data[0] == ID_NAT_PUNCHTHROUGH_FAILED)
			{
				guid = packet->guid;
			}
			else
			{
				RakNet::BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(1);
				bool b = bs.Read(guid);
				RakAssert(b);
			}

			switch (packet->data[0])
			{
			case ID_NAT_TARGET_NOT_CONNECTED:
				printf("Failed: ID_NAT_TARGET_NOT_CONNECTED\n");
				break;
			case ID_NAT_TARGET_UNRESPONSIVE:
				printf("Failed: ID_NAT_TARGET_UNRESPONSIVE\n");
				break;
			case ID_NAT_CONNECTION_TO_TARGET_LOST:
				printf("Failed: ID_NAT_CONNECTION_TO_TARGET_LOST\n");
				break;
			case ID_NAT_PUNCHTHROUGH_FAILED:
				printf("Failed: ID_NAT_PUNCHTHROUGH_FAILED\n");
				break;
			}

			sampleResult = FAILED;
			return;
		}
		else if (packet->data[0] == ID_NAT_PUNCHTHROUGH_SUCCEEDED)
		{
			unsigned char weAreTheSender = packet->data[1];
			if (weAreTheSender)
				printf("NAT punch success to remote system %s.\n", packet->systemAddress.ToString(true));
			else
				printf("NAT punch success from remote system %s.\n", packet->systemAddress.ToString(true));

			char guid[128];
			printf("Enter RakNetGuid of the remote system, which should have already connected.\nOr press enter to quit.\n");
			Gets(guid, sizeof(guid));
			if (guid[0])
			{
				RakNetGUID remoteSystemGuid;
				remoteSystemGuid.FromString(guid);
				npClient->OpenNAT(remoteSystemGuid, serverAddress);

				timeout = RakNet::GetTimeMS() + 10000;
			}
			else
			{
				sampleResult = SUCCEEDED;
			}
		}
	}
	virtual void Update(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		if (sampleResult == PENDING && RakNet::GetTimeMS()>timeout && isListening == false)
		{
			printf("No response from the server, probably not running NatPunchthroughServer plugin.\n");
			sampleResult = FAILED;
		}
	}
	virtual void Shutdown(RakNet::RakPeerInterface *rakPeer)
	{
		delete npClient;
		npClient = 0;
	}

	NatPunchthroughClient *npClient;
	RakNet::TimeMS timeout;
	bool isListening;
};

struct UDPProxyClientFramework : public SampleFramework, public UDPProxyClientResultHandler
{
	// Set to FAILED to skip this test
	UDPProxyClientFramework() { sampleResult = SUPPORT_UDP_PROXY; udpProxy = 0; }
	virtual const char * QueryName(void) { return "UDPProxyClientFramework"; }
	virtual bool QueryRequiresServer(void) { return true; }
	virtual const char * QueryFunction(void) { return "Connect to a peer using a shared server connection."; }
	virtual const char * QuerySuccess(void) { return "We can now communicate with the other system, including connecting, within 5 seconds."; }
	virtual bool QueryQuitOnSuccess(void) { return false; }
	virtual void Init(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		SystemAddress serverAddress = SelectAmongConnectedSystems(rakPeer, "UDPProxyCoordinator");
		if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			serverAddress = ConnectBlocking(rakPeer, "UDPProxyCoordinator", DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT);
			if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			{
				printf("Failed to connect to a server.\n");
				sampleResult = FAILED;
				return;
			}
		}
		udpProxy = new UDPProxyClient;
		rakPeer->AttachPlugin(udpProxy);
		udpProxy->SetResultHandler(this);

		char guid[128];
		printf("Enter RakNetGuid of the remote system, which should have already connected\nto the server.\nOr press enter to just listen.\n");
		Gets(guid, sizeof(guid));
		RakNetGUID targetGuid;
		targetGuid.FromString(guid);

		if (guid[0])
		{
			RakNetGUID remoteSystemGuid;
			remoteSystemGuid.FromString(guid);
			udpProxy->RequestForwarding(serverAddress, UNASSIGNED_SYSTEM_ADDRESS, targetGuid, UDP_FORWARDER_MAXIMUM_TIMEOUT, 0);
			isListening = false;
		}
		else
		{
			printf("Listening\n");
			printf("My GUID is %s\n", rakPeer->GetGuidFromSystemAddress(UNASSIGNED_SYSTEM_ADDRESS).ToString());
			isListening = true;
		}

		timeout = RakNet::GetTimeMS() + 5000;
	}
	virtual void ProcessPacket(Packet *packet)
	{
	}
	virtual void Update(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		if (sampleResult == PENDING && RakNet::GetTimeMS()>timeout && isListening == false)
		{
			printf("No response from the server, probably not running UDPProxyCoordinator plugin.\n");
			sampleResult = FAILED;
		}
	}
	virtual void Shutdown(RakNet::RakPeerInterface *rakPeer)
	{
		delete udpProxy;
		udpProxy = 0;
	}

	virtual void OnForwardingSuccess(const char *proxyIPAddress, unsigned short proxyPort,
		SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Datagrams forwarded by proxy %s:%i to target %s.\n", proxyIPAddress, proxyPort, targetAddress.ToString(false));
		printf("Connecting to proxy, which will be received by target.\n");
		ConnectionAttemptResult car = proxyClientPlugin->GetRakPeerInterface()->Connect(proxyIPAddress, proxyPort, 0, 0);
		RakAssert(car == CONNECTION_ATTEMPT_STARTED);
		sampleResult = SUCCEEDED;
	}
	virtual void OnForwardingNotification(const char *proxyIPAddress, unsigned short proxyPort,
		SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Source %s has setup forwarding to us through proxy %s:%i.\n", sourceAddress.ToString(false), proxyIPAddress, proxyPort);

		sampleResult = SUCCEEDED;
	}
	virtual void OnNoServersOnline(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Failure: No servers logged into coordinator.\n");
		sampleResult = FAILED;
	}
	virtual void OnRecipientNotConnected(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Failure: Recipient not connected to coordinator.\n");
		sampleResult = FAILED;
	}
	virtual void OnAllServersBusy(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Failure: No servers have available forwarding ports.\n");
		sampleResult = FAILED;
	}
	virtual void OnForwardingInProgress(const char *proxyIPAddress, unsigned short proxyPort, SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
	{
		printf("Notification: Forwarding already in progress.\n");
	}

	UDPProxyClient *udpProxy;
	RakNet::TimeMS timeout;
	bool isListening;
};
void PrintPacketMessages(Packet *packet, RakPeerInterface *rakPeer)
{

	switch (packet->data[0])
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED\n");
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		printf("ID_CONNECTION_LOST from %s\n", packet->systemAddress.ToString(true));
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", rakPeer->GetExternalID(packet->systemAddress).ToString(true));
		break;
	}
}

enum FeatureList
{
	_NatTypeDetectionFramework,
	_NatPunchthoughFramework,
	_UDPProxyClientFramework,
	FEATURE_LIST_COUNT
};



MainPage::MainPage()
{
	InitializeComponent();











// #error "add raknet init code, what happened to windows phone?"
/* // ORIGINAL!!!
	RakPeerInterface *rakPeer = RakPeerInterface::GetInstance();
	SocketDescriptor sd;
	StartupResult sr = rakPeer->Startup(1, &sd, 1);
	assert(sr==RAKNET_STARTED);
	ConnectionAttemptResult car = rakPeer->Connect(DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT, 0, 0);
	assert(car==CONNECTION_ATTEMPT_STARTED);
	RakSleep(1000);
	Packet *packet;
	packet=rakPeer->Receive();
	if (packet)
	{
		RakAssert(packet->data[0]==ID_CONNECTION_REQUEST_ACCEPTED);
	}
*/
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.  The Parameter
/// property is typically used to configure the page.</param>
void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	(void) e;	// Unused parameter
}
