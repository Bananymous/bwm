#pragma once

#include "wireless_manager.h"


class IwdWirelessManager : public WirelessManager
{
public:
	virtual bool Init() override;

	virtual const Device& GetCurrentDevice() const override { return m_devices[m_current_index]; }
	virtual bool SetCurrentDevice(const Device& device) override;
	virtual bool ActivateDevice() override;

	virtual const std::vector<Device>&  GetDevices() const override			{ return m_devices; }
	virtual const std::vector<Network>& GetNetworks() const override		{ return m_networks; }
	virtual const std::vector<Network>& GetKnownNetworks() const override	{ return m_known_networks; }

	virtual bool Scan() override;
	virtual bool UpdateNetworks() override;

	virtual bool Connect(const Network& network, const std::string& password) override;
	virtual bool Disconnect() override;

	virtual bool UpdateKnownNetworks() override;
	virtual bool ForgetKnownNetwork(const Network& network) override;

private:
	std::size_t				m_current_index;
	std::vector<Device>		m_devices;
	std::vector<Network>	m_networks;
	std::vector<Network>	m_known_networks;
};