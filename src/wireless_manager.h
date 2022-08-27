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
public:
	WirelessManager(WirelessBackend backend)
		: m_backend(backend)
	{ }

	bool Init();

	const Device& GetCurrentDevice() const { return m_devices[m_current_index]; }
	bool SetCurrentDevice(const Device& device);
	bool ActivateDevice();

	const std::vector<Device>&  GetDevices() const			{ return m_devices; }
	const std::vector<Network>& GetNetworks() const			{ return m_networks; }
	const std::vector<Network>& GetKnownNetworks() const	{ return m_known_networks; }

	bool Scan();
	bool UpdateNetworks();

	bool Connect(const Network& network, const std::string& password = "");
	bool Disconnect();

	bool UpdateKnownNetworks();
	bool ForgetKnownNetwork(const Network& network);

private:
	WirelessBackend			m_backend;

	std::size_t				m_current_index;
	std::vector<Device>		m_devices;
	std::vector<Network>	m_networks;
	std::vector<Network>	m_known_networks;
};