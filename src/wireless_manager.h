#pragma once

#include "structs.h"

#include <string>
#include <vector>

enum class WirelessBackend
{
	iwd
};

class WirelessManager
{
protected:
	WirelessManager() {}

public:
	static WirelessManager* Create(WirelessBackend backend);

	virtual ~WirelessManager() {};

	virtual bool Init() = 0;

	virtual const Device& GetCurrentDevice() const = 0;
	virtual bool SetCurrentDevice(const Device& device) = 0;
	virtual bool ActivateDevice() = 0;

	virtual const std::vector<Device>&  GetDevices() const = 0;
	virtual const std::vector<Network>& GetNetworks() const = 0;
	virtual const std::vector<Network>& GetKnownNetworks() const = 0;

	virtual bool Scan() = 0;
	virtual bool UpdateNetworks() = 0;

	virtual bool Connect(const Network& network, const std::string& password = "") = 0;
	virtual bool Disconnect() = 0;

	virtual bool UpdateKnownNetworks() = 0;
	virtual bool ForgetKnownNetwork(const Network& network) = 0;
};