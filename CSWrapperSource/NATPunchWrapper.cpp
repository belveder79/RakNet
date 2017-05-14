/*
*  Copyright (c) 2014, Oculus VR, Inc.
*  All rights reserved.
*
*  This source code is licensed under the BSD-style license found in the
*  LICENSE file in the root directory of this source tree. An additional grant
*  of patent rights can be found in the PATENTS file in the same directory.
*
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
#include "Gets.h"
#include "Itoa.h"

using namespace RakNet;

#define DEFAULT_RAKPEER_PORT 50000
#define RAKPEER_PORT_STR "0"
#define DEFAULT_SERVER_PORT "10000"
#define DEFAULT_SERVER_ADDRESS "52.59.145.248"

// defines a callback to the c# managed DLL
typedef int(__stdcall * Callback)(const char* text);
Callback Handler = 0;

//=============
// LOGGER

#include <mutex>
#include <atomic>
#include <vector>
#include <thread>
#include <sstream>

#include <iostream>
#include <fstream>
#include <iomanip>
#if defined(WIN32) || defined (IOS) || (defined(UNIX) && !defined(ANDROID))
  #include <cstdio>
#elif  ANDROID
  #include <android/log.h>
#endif 

#define LOG_TAG "NATPunchWrapper"

class Logger {

public:

	static Logger& getInstance() {
		static Logger instance;
		return instance;
	}

	void setup(bool logging_enabled = true,
		bool write_log_2_file = true,
		bool print_log = true,
		bool cache_log = false,
		std::string output_dir = ".") {
		logging_enabled_ = logging_enabled;
		write_log_2_file_ = write_log_2_file;
		print_log_ = print_log;
		cache_log_ = cache_log;
		output_dir_ = output_dir;
		first_msg_ = true;
	}

	void log(std::string tag, std::string msg) {
		if (!logging_enabled_)
			return;
		access_mutex_.lock();
		if (first_msg_) {
			//http://www.network-science.de/ascii/
			doLog("LOGGER", "    :::     :::::::::::::::::::::::    :::    ::::::::: ::::::::::::::::::: ");
			doLog("LOGGER", "   :+:     :+:    :+:         :+:   :+: :+:  :+:    :+:    :+:   :+:    :+: ");
			doLog("LOGGER", "  +:+     +:+    +:+        +:+   +:+   +:+ +:+    +:+    +:+   +:+    +:+  ");
			doLog("LOGGER", " +#+     +:+    +#+       +#+   +#++:++#++:+#++:++#:     +#+   +#+    +:+   ");
			doLog("LOGGER", " +#+   +#+     +#+      +#+    +#+     +#++#+    +#+    +#+   +#+    +#+    ");
			doLog("LOGGER", " #+#+#+#      #+#     #+#     #+#     #+##+#    #+#    #+#   #+#    #+#     ");
			doLog("LOGGER", "  ###    #######################     ######    ######################       ");
			doLog("LOGGER", "       ::::::::: :::   :::            :::    :::::::::     ::: ");
			doLog("LOGGER", "      :+:    :+::+:   :+:          :+: :+:  :+:    :+:   :+:   ");
			doLog("LOGGER", "     +:+    +:+ +:+ +:+          +:+   +:+ +:+    +:+  +:+ +:+ ");
			doLog("LOGGER", "    +#++:++#+   +#++:          +#++:++#++:+#++:++#:  +#+  +:+  ");
			doLog("LOGGER", "   +#+    +#+   +#+           +#+     +#++#+    +#++#+#+#+#+#+ ");
			doLog("LOGGER", "  #+#    #+#   #+#           #+#     #+##+#    #+#      #+#    ");
			doLog("LOGGER", " #########    ###           ###     ######    ###      ###     ");
			doLog("LOGGER", "");
			first_msg_ = false;
		}
		doLog(tag, msg);
		access_mutex_.unlock();
	}

private:
	std::atomic<bool> logging_enabled_;
	std::atomic<bool> write_log_2_file_;
	std::atomic<bool> print_log_;
	std::atomic<bool> cache_log_;
	std::atomic<bool> first_msg_;
	std::string log_file_;
	std::string output_dir_;

	void doLog(std::string tag, std::string msg) {
		if (write_log_2_file_ || print_log_ || cache_log_) {
			std::string m = niceTimeStr() + "|" + tag + "|" + msg;
			if (cache_log_) {
				messages_.push_back(m);
			}

			if (print_log_) {
#if !defined(ANDROID)
				std::cout << m << std::endl;
#elif defined(ANDROID)
				//__android_log_print(ANDROID_LOG_INFO,tag.c_str(),msg.c_str());
				__android_log_print(ANDROID_LOG_INFO, tag.c_str(), "%s", msg.c_str());
#endif
			}

			if (write_log_2_file_) {
				//if(Handler)
				//	Handler(m.c_str());
				log2file(m);
			}
		}
	}

	void outputTime(const time_t timestamp, std::stringstream& ss)
	{
#if defined(__GNUC__) && (__GNUC__ < 5)
		char buf[24];
		if (0 < strftime(buf, sizeof(buf), "%Y-%m-%d %X", localtime(&timestamp)))
			ss << buf;
#else
		ss << std::put_time(localtime(&timestamp), "%Y-%m-%d %X");
#endif
	}

	std::string niceTimeStr() {
		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);

		std::stringstream ss;
		outputTime(in_time_t, ss);
		return ss.str();
	}

	void log2file(std::string msg) {
		std::ofstream of;
		std::string log_file_name = output_dir_ + "\\" + log_file_;
		of.open(log_file_name, std::ios::out | std::ios::app);
		if (of.is_open()) {
			std::stringstream ss;
			ss << msg << std::endl;
			of << ss.str();
			of.close();
		}
		else {
			std::cout << "[LOGGER] Cannot write " << log_file_ << "\n";
		}
	}

	std::vector<std::string> messages_;
	std::mutex access_mutex_;

protected:
	Logger() {
		logging_enabled_ = true;
		write_log_2_file_ = true;
		print_log_ = true;
		cache_log_ = false;

		first_msg_ = false;
		output_dir_ = ".";
		log_file_ = "runlog.log";
		std::cout << niceTimeStr() << "|LOGGER|Constructor called " << std::endl;
	}
	~Logger() {}
};



enum SampleResult
{
	PENDING,
	FAILED,
	SUCCEEDED
};

#define SUPPORT_NAT_TYPE_DETECTION PENDING
#define SUPPORT_NAT_PUNCHTHROUGH PENDING

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
	char logbuf[128]; sprintf(logbuf, "SelectAmongConnectedSystems %s.\n", hostName);
	Logger::getInstance().log(LOG_TAG, logbuf);

	DataStructures::List<SystemAddress> addresses;
	DataStructures::List<RakNetGUID> guids;
	rakPeer->GetSystemList(addresses, guids);
	if (addresses.Size() == 0)
	{
		return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}
	if (addresses.Size()>1)
	{
		char logbuf[128]; sprintf(logbuf, "Select IP address for %s.\n", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
		char buff[64];
		for (unsigned int i = 0; i < addresses.Size(); i++)
		{
			addresses[i].ToString(true, buff);
			char logbuf[128]; sprintf(logbuf, "%i. %s\n", i + 1, buff);
			Logger::getInstance().log(LOG_TAG, logbuf);
		}
		buff[0] = 0; // Gets(buff, sizeof(buff));
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
SystemAddress ConnectBlocking(RakNet::RakPeerInterface *rakPeer, 
	const char *hostName, const char *defaultAddress, const char *defaultPort)
{
	char logbuf[128]; sprintf(logbuf, "ConnectBlocking %s - %s %s.\n", hostName, defaultAddress, defaultPort);
	Logger::getInstance().log(LOG_TAG, logbuf);

	char ipAddr[64];
	if (defaultAddress == 0 || defaultAddress[0] == 0) {
		char logbuf[128]; sprintf(logbuf, "Enter IP of system %s is running on: ", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
	}
	else
	{
		char logbuf[128]; sprintf(logbuf, "Enter IP of system %s, or press enter for default: ", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
	}
	ipAddr[0] = 0; // Gets(ipAddr, sizeof(ipAddr));
	if (ipAddr[0] == 0)
	{
		if (defaultAddress == 0 || defaultAddress[0] == 0)
		{
			char logbuf[128]; sprintf(logbuf, "Failed. No address entered for %s.\n", hostName);
			Logger::getInstance().log(LOG_TAG, logbuf);
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		else
		{
			strcpy(ipAddr, defaultAddress);
		}
	}
	char port[64];
	if (defaultAddress == 0 || defaultAddress[0] == 0) {
		char logbuf[128]; sprintf(logbuf, "Enter port of system %s is running on: ", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
	}
	else {
		char logbuf[128]; sprintf(logbuf, "Enter port of system %s, or press enter for default: ", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
	}
	port[0] = 0; //	Gets(port, sizeof(port));
	if (port[0] == 0)
	{
		if (defaultPort == 0 || defaultPort[0] == 0)
		{
			char logbuf[128]; sprintf(logbuf, "Failed. No port entered for %s.\n", hostName);
			Logger::getInstance().log(LOG_TAG, logbuf);
			return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
		}
		else
		{
			strcpy(port, defaultPort);
		}
	}
	sprintf(logbuf,"Trying to call %s on port %d...\n",ipAddr, atoi(port));
	Logger::getInstance().log(LOG_TAG, logbuf);
	if (rakPeer->Connect(ipAddr, atoi(port), 0, 0) != RakNet::CONNECTION_ATTEMPT_STARTED)
	{
		char logbuf[128]; sprintf(logbuf, "Failed connect call for %s.\n", hostName);
		Logger::getInstance().log(LOG_TAG, logbuf);
		return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}
	Logger::getInstance().log(LOG_TAG, "Connecting...\n");
	RakNet::Packet *packet;
	while (1)
	{
		for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
		{
			if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
			{
				Logger::getInstance().log(LOG_TAG, "System Address Assigned!\n");
				return packet->systemAddress;
			}
			else if (packet->data[0] == ID_NO_FREE_INCOMING_CONNECTIONS)
			{
				Logger::getInstance().log(LOG_TAG, "ID_NO_FREE_INCOMING_CONNECTIONS");
				return RakNet::UNASSIGNED_SYSTEM_ADDRESS;
			}
			else
			{
				Logger::getInstance().log(LOG_TAG, "Unassigned System Address!\n");
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
		Logger::getInstance().log(LOG_TAG, "NatTypeDetectionFramework Init()");
		if (sampleResult == FAILED) return;

		SystemAddress serverAddress = SelectAmongConnectedSystems(rakPeer, "NatTypeDetectionServer");
		if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			serverAddress = ConnectBlocking(rakPeer, "NatTypeDetectionServer", DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT);
			if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			{
				Logger::getInstance().log(LOG_TAG, "Failed to connect to a server.\n");
				sampleResult = FAILED;
				return;
			}
		}
		Logger::getInstance().log(LOG_TAG, "Attach Plugin...");
		ntdc = new NatTypeDetectionClient;
		rakPeer->AttachPlugin(ntdc);
		Logger::getInstance().log(LOG_TAG, "Attached!");
		ntdc->DetectNATType(serverAddress);
		Logger::getInstance().log(LOG_TAG, "Detection done!");
		timeout = RakNet::GetTimeMS() + 5000;
		Logger::getInstance().log(LOG_TAG, "NatTypeDetectionFramework Init done!");
	}

	virtual void ProcessPacket(Packet *packet)
	{
		if (packet->data[0] == ID_NAT_TYPE_DETECTION_RESULT)
		{
			RakNet::NATTypeDetectionResult r = (RakNet::NATTypeDetectionResult) packet->data[1];
			char logbuf[128]; sprintf(logbuf, "NAT Type is %s (%s)\n", NATTypeDetectionResultToString(r), NATTypeDetectionResultToStringFriendly(r));
			Logger::getInstance().log(LOG_TAG, logbuf); 
			Logger::getInstance().log(LOG_TAG, "Using NATPunchthrough can connect to systems using:\n");
			for (int i = 0; i < (int)RakNet::NAT_TYPE_COUNT; i++)
			{
				if (CanConnect(r, (RakNet::NATTypeDetectionResult)i))
				{
					if (i != 0) {
						Logger::getInstance().log(LOG_TAG, ", ");
					}
					char logbuf[128]; sprintf(logbuf, "%s", NATTypeDetectionResultToString((RakNet::NATTypeDetectionResult)i));
					Logger::getInstance().log(LOG_TAG, logbuf);
				}
			}
			Logger::getInstance().log(LOG_TAG, "\n");
			if (r == RakNet::NAT_TYPE_PORT_RESTRICTED || r == RakNet::NAT_TYPE_SYMMETRIC)
			{
				// For UPNP, see Samples\UDPProxy
				Logger::getInstance().log(LOG_TAG, "Note: Your router must support UPNP or have the user manually forward ports.\n");
				Logger::getInstance().log(LOG_TAG, "Otherwise NATPunchthrough may not always succeed.\n");
			}

			sampleResult = SUCCEEDED;
		}
	}
	virtual void Update(RakNet::RakPeerInterface *rakPeer)
	{
		if (sampleResult == FAILED) return;

		if (sampleResult == PENDING && RakNet::GetTimeMS()>timeout)
		{
			Logger::getInstance().log(LOG_TAG, "No response from the server, probably not running NatTypeDetectionServer plugin.\n");
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
		Logger::getInstance().log(LOG_TAG, "NatPunchthoughClientFramework Init!");
		if (sampleResult == FAILED) return;

		serverAddress = SelectAmongConnectedSystems(rakPeer, "NatPunchthroughServer");
		if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
		{
			serverAddress = ConnectBlocking(rakPeer, "NatPunchthroughServer", DEFAULT_SERVER_ADDRESS, DEFAULT_SERVER_PORT);
			if (serverAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
			{
				Logger::getInstance().log(LOG_TAG, "Failed to connect to a server.\n");
				sampleResult = FAILED;
				return;
			}
		}

		npClient = new NatPunchthroughClient;
		npClient->SetDebugInterface(this);
		rakPeer->AttachPlugin(npClient);


		char guid[128];
		Logger::getInstance().log(LOG_TAG, "Enter RakNetGuid of the remote system, which should have already connected\nto the server.\nOr press enter to just listen.\n");
		/*
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
		*/
			Logger::getInstance().log(LOG_TAG, "Listening\n");
			char logbuf[128]; sprintf(logbuf, "My GUID is %s\n", rakPeer->GetMyGUID().ToString());
			Logger::getInstance().log(LOG_TAG, logbuf);
			isListening = true;

			// Find the stride of our router in advance
			npClient->FindRouterPortStride(serverAddress);
		/*
		}
		*/
		Logger::getInstance().log(LOG_TAG, "NatPunchthoughClientFramework Init Done()!");
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
				Logger::getInstance().log(LOG_TAG, "Failed: ID_NAT_TARGET_NOT_CONNECTED\n");
				break;
			case ID_NAT_TARGET_UNRESPONSIVE:
				Logger::getInstance().log(LOG_TAG, "Failed: ID_NAT_TARGET_UNRESPONSIVE\n");
				break;
			case ID_NAT_CONNECTION_TO_TARGET_LOST:
				Logger::getInstance().log(LOG_TAG, "Failed: ID_NAT_CONNECTION_TO_TARGET_LOST\n");
				break;
			case ID_NAT_PUNCHTHROUGH_FAILED:
				Logger::getInstance().log(LOG_TAG, "Failed: ID_NAT_PUNCHTHROUGH_FAILED\n");
				break;
			}

			sampleResult = FAILED;
			return;
		}
		else if (packet->data[0] == ID_NAT_PUNCHTHROUGH_SUCCEEDED)
		{
			unsigned char weAreTheSender = packet->data[1];
			if (weAreTheSender) {
				char logbuf[128]; sprintf(logbuf, "NAT punch success to remote system %s.\n", packet->systemAddress.ToString(true));
				Logger::getInstance().log(LOG_TAG, logbuf);
			}
			else {
				char logbuf[128]; sprintf(logbuf, "NAT punch success from remote system %s.\n", packet->systemAddress.ToString(true));
				Logger::getInstance().log(LOG_TAG, logbuf);
			}
			char guid[128];
			Logger::getInstance().log(LOG_TAG, "Enter RakNetGuid of the remote system, which should have already connected.\nOr press enter to quit.\n");
			guid[0] = 0; // Gets(guid, sizeof(guid));
			if (guid[0])
			{
				RakNetGUID remoteSystemGuid;
				// Gets(guid, sizeof(guid));
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
		//Logger::getInstance().log(LOG_TAG, "Trying to update...\n");
		if (sampleResult == FAILED) return;

		//Logger::getInstance().log(LOG_TAG, "Did not fail... see if pending...\n");
		if (sampleResult == PENDING && RakNet::GetTimeMS()>timeout && isListening == false)
		{
			Logger::getInstance().log(LOG_TAG, "No response from the server, probably not running NatPunchthroughServer plugin.\n");
			sampleResult = FAILED;
		}
		//Logger::getInstance().log(LOG_TAG, "Returning from update call!\n");
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

void PrintPacketMessages(Packet *packet, RakPeerInterface *rakPeer)
{
	char logbuf[128];
	switch (packet->data[0])
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		Logger::getInstance().log(LOG_TAG, "ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		Logger::getInstance().log(LOG_TAG, "ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		Logger::getInstance().log(LOG_TAG, "ID_ALREADY_CONNECTED\n");
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		Logger::getInstance().log(LOG_TAG, "ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		Logger::getInstance().log(LOG_TAG, "ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		Logger::getInstance().log(LOG_TAG, "ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		Logger::getInstance().log(LOG_TAG, "ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		Logger::getInstance().log(LOG_TAG, "We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		Logger::getInstance().log(LOG_TAG, "Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		Logger::getInstance().log(LOG_TAG, "ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		Logger::getInstance().log(LOG_TAG, "ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		sprintf(logbuf, "ID_CONNECTION_LOST from %s\n", packet->systemAddress.ToString(true));
		Logger::getInstance().log(LOG_TAG, logbuf);
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		sprintf(logbuf, "ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		Logger::getInstance().log(LOG_TAG, logbuf); 
		sprintf(logbuf, "My external address is %s\n", rakPeer->GetExternalID(packet->systemAddress).ToString(true));
		Logger::getInstance().log(LOG_TAG, logbuf);
		break;
	}
}

enum FeatureList
{
	_NatTypeDetectionFramework,
	_NatPunchthoughFramework,
	FEATURE_LIST_COUNT
};

void threadFun();

extern "C" __declspec(dllexport)
int __stdcall InitializeRaknetLibrary(const char* foldername, Callback handler) {
	Logger::getInstance().setup(true, true, true, false, std::string(foldername));
	Logger::getInstance().log(LOG_TAG, "Initialization of Raknet done!\n");

	Handler = handler;
	handler((std::string("Current log folder set to ") + std::string(foldername) + std::string("\n")).c_str());

	std::thread runPunch(threadFun);
	runPunch.detach();
	return 0;
}

void threadFun()
{
	RakNet::RakPeerInterface *rakPeer = RakNet::RakPeerInterface::GetInstance();
	unsigned short port = DEFAULT_RAKPEER_PORT;
	RakNet::SocketDescriptor sd(port, 0);
	if (rakPeer->Startup(32, &sd, 1) != RakNet::RAKNET_STARTED)
	{
		Logger::getInstance().log(LOG_TAG, "Failed to start rakPeer! Quitting\n");
		RakNet::RakPeerInterface::DestroyInstance(rakPeer);
		return;
	}

	rakPeer->SetMaximumIncomingConnections(32);

	SampleFramework *samples[FEATURE_LIST_COUNT];
	unsigned int i = 0;
	samples[i++] = new NatTypeDetectionFramework;
	samples[i++] = new NatPunchthoughClientFramework;
	assert(i == FEATURE_LIST_COUNT);

	bool isFirstPrint = true;
	for (i = 0; i < FEATURE_LIST_COUNT; i++)
	{
		if (isFirstPrint)
		{
			Logger::getInstance().log(LOG_TAG, "NAT traversal client\nSupported operations:\n");
			isFirstPrint = false;
		}
		char buf[1024];
		sprintf(buf,"\n%s\nRequires server: %s\nDescription: %s\n", samples[i]->QueryName(), samples[i]->QueryRequiresServer() == 1 ? "Yes" : "No", samples[i]->QueryFunction());
		Logger::getInstance().log(LOG_TAG, buf);
	}
	
	//
	FeatureList currentStage = _NatPunchthoughFramework;// _NatTypeDetectionFramework;

	while (1)
	{
		char logbuf[128];
		sprintf(logbuf,"Executing %s\n", samples[(int)currentStage]->QueryName());
		Logger::getInstance().log(LOG_TAG, logbuf);
		samples[(int)currentStage]->Init(rakPeer);

		bool thisSampleDone = false;
		while (1)
		{
			//Logger::getInstance().log(LOG_TAG, "Update!");
			samples[(int)currentStage]->Update(rakPeer);
			RakNet::Packet *packet;
			//Logger::getInstance().log(LOG_TAG, "Receive...\n");
			for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
			{
				for (i = 0; i < FEATURE_LIST_COUNT; i++)
				{
					samples[i]->ProcessPacket(packet);
				}

				PrintPacketMessages(packet, rakPeer);
			}

			//Logger::getInstance().log(LOG_TAG, "Check Result!");
			if (samples[(int)currentStage]->sampleResult == FAILED ||
				samples[(int)currentStage]->sampleResult == SUCCEEDED)
			{
				Logger::getInstance().log(LOG_TAG, "\n");
				thisSampleDone = true;
				if (samples[(int)currentStage]->sampleResult == FAILED)
				{
					char buf[128];
					sprintf(buf,"Failed %s\n", samples[(int)currentStage]->QueryName());
					Logger::getInstance().log(LOG_TAG, buf);

					int stageInt = (int)currentStage;
					stageInt++;
					currentStage = (FeatureList)stageInt;
					if (currentStage == FEATURE_LIST_COUNT)
					{
						Logger::getInstance().log(LOG_TAG, "Connectivity not possible. Exiting\n");
						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);
						return;
					}
					else
					{
						Logger::getInstance().log(LOG_TAG, "Proceeding to next stage.\n");
						break;
					}
				}
				else
				{
					char buf[128];
					sprintf(buf, "Passed %s\n", samples[(int)currentStage]->QueryName());
					Logger::getInstance().log(LOG_TAG, buf);
					if (samples[(int)currentStage]->QueryQuitOnSuccess())
					{
						std::atomic<bool> stop = false;
						Logger::getInstance().log(LOG_TAG, "Press any key to quit.\n");
						while (!stop)
						{
							for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
							{
								for (i = 0; i < FEATURE_LIST_COUNT; i++)
								{
									samples[i]->ProcessPacket(packet);
								}

								PrintPacketMessages(packet, rakPeer);
							}
							RakSleep(30);
						}

						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);
						Logger::getInstance().log(LOG_TAG, "Press enter to quit.\n");
						return;
					}

					Logger::getInstance().log(LOG_TAG, "Proceeding to next stage.\n");
					int stageInt = (int)currentStage;
					stageInt++;
					if (stageInt<FEATURE_LIST_COUNT)
					{
						currentStage = (FeatureList)stageInt;
					}
					else
					{
						std::atomic<bool> stop = false;
						Logger::getInstance().log(LOG_TAG, "Press any key to quit when done.\n");
						while (!stop)
						{
							for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
							{
								for (i = 0; i < FEATURE_LIST_COUNT; i++)
								{
									samples[i]->ProcessPacket(packet);
								}

								PrintPacketMessages(packet, rakPeer);
							}
							RakSleep(30);
						}
						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);					
						return;
					}
					break;
				}
			}
			//Logger::getInstance().log(LOG_TAG, "Sleep 30ms...\n");
			RakSleep(30);
			//Logger::getInstance().log(LOG_TAG, "done!\n");
		}
	}
	return;
}

/*
int main(void)
{
	RakNet::RakPeerInterface *rakPeer = RakNet::RakPeerInterface::GetInstance();
	printf("Enter local port, or press enter for default: ");
	char buff[64];
	Gets(buff, sizeof(buff));
	unsigned short port = DEFAULT_RAKPEER_PORT;
	if (buff[0] != 0)
		port = atoi(buff);
	RakNet::SocketDescriptor sd(port, 0);
	if (rakPeer->Startup(32, &sd, 1) != RakNet::RAKNET_STARTED)
	{
		printf("Failed to start rakPeer! Quitting\n");
		RakNet::RakPeerInterface::DestroyInstance(rakPeer);
		getch();
		return 1;
	}
	rakPeer->SetMaximumIncomingConnections(32);

	SampleFramework *samples[FEATURE_LIST_COUNT];
	unsigned int i = 0;
	samples[i++] = new NatTypeDetectionFramework;
	samples[i++] = new NatPunchthoughClientFramework;
	assert(i == FEATURE_LIST_COUNT);

	bool isFirstPrint = true;
	for (i = 0; i < FEATURE_LIST_COUNT; i++)
	{
		if (isFirstPrint)
		{
			printf("NAT traversal client\nSupported operations:\n");
			isFirstPrint = false;
		}
		printf("\n%s\nRequires server: %s\nDescription: %s\n", samples[i]->QueryName(), samples[i]->QueryRequiresServer() == 1 ? "Yes" : "No", samples[i]->QueryFunction());
	}

	printf("\nDo you have a server running the NATCompleteServer project? (y/n): ");

	char responseLetter = getche();
	bool hasServer = responseLetter == 'y' || responseLetter == 'Y' || responseLetter == ' ';
	printf("\n");
	if (hasServer == false)
		printf("Note: Only UPNP and Router2 are supported without a server\nYou may want to consider using the Lobby2/Steam project. They host the\nservers for you.\n\n");

	FeatureList currentStage = _NatTypeDetectionFramework;

	if (hasServer == false)
	{
		while (samples[(int)currentStage]->QueryRequiresServer() == true)
		{
			printf("No server: Skipping %s\n", samples[(int)currentStage]->QueryName());
			int stageInt = (int)currentStage;
			stageInt++;
			currentStage = (FeatureList)stageInt;
			if (currentStage == FEATURE_LIST_COUNT)
			{
				printf("Connectivity not possible. Exiting\n");
				getch();
				return 1;
			}
		}
	}

	while (1)
	{
		printf("Executing %s\n", samples[(int)currentStage]->QueryName());
		samples[(int)currentStage]->Init(rakPeer);

		bool thisSampleDone = false;
		while (1)
		{
			samples[(int)currentStage]->Update(rakPeer);
			RakNet::Packet *packet;
			for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
			{
				for (i = 0; i < FEATURE_LIST_COUNT; i++)
				{
					samples[i]->ProcessPacket(packet);
				}

				PrintPacketMessages(packet, rakPeer);
			}

			if (samples[(int)currentStage]->sampleResult == FAILED ||
				samples[(int)currentStage]->sampleResult == SUCCEEDED)
			{
				printf("\n");
				thisSampleDone = true;
				if (samples[(int)currentStage]->sampleResult == FAILED)
				{
					printf("Failed %s\n", samples[(int)currentStage]->QueryName());

					int stageInt = (int)currentStage;
					stageInt++;
					currentStage = (FeatureList)stageInt;
					if (currentStage == FEATURE_LIST_COUNT)
					{
						printf("Connectivity not possible. Exiting\n");
						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);
						getch();
						return 1;
					}
					else
					{
						printf("Proceeding to next stage.\n");
						break;
					}
				}
				else
				{
					printf("Passed %s\n", samples[(int)currentStage]->QueryName());
					if (samples[(int)currentStage]->QueryQuitOnSuccess())
					{

						printf("Press any key to quit.\n");
						while (!kbhit())
						{
							for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
							{
								for (i = 0; i < FEATURE_LIST_COUNT; i++)
								{
									samples[i]->ProcessPacket(packet);
								}

								PrintPacketMessages(packet, rakPeer);
							}
							RakSleep(30);
						}

						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);
						printf("Press enter to quit.\n");
						char temp[32];
						Gets(temp, sizeof(temp));
						getch();
						return 1;
					}

					printf("Proceeding to next stage.\n");
					int stageInt = (int)currentStage;
					stageInt++;
					if (stageInt<FEATURE_LIST_COUNT)
					{
						currentStage = (FeatureList)stageInt;
					}
					else
					{
						printf("Press any key to quit when done.\n");

						while (!kbhit())
						{
							for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
							{
								for (i = 0; i < FEATURE_LIST_COUNT; i++)
								{
									samples[i]->ProcessPacket(packet);
								}

								PrintPacketMessages(packet, rakPeer);
							}
							RakSleep(30);
						}

						rakPeer->Shutdown(100);
						RakNet::RakPeerInterface::DestroyInstance(rakPeer);
						getch();
						return 1;
					}
					break;
				}
			}

			RakSleep(30);
		}
	}

	getch();
	return 0;
}
*/

/*
#include "stdafx.h"

typedef int(__stdcall * Callback)(const char* text);

Callback Handler = 0;

extern "C" __declspec(dllexport)
void __stdcall SetCallback(Callback handler) {
	Handler = handler;
}

extern "C" __declspec(dllexport)
void __stdcall TestCallback() {
	int retval = Handler("hello world");
}
*/