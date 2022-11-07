#include "iwd_wireless_manager.h"

#include "iwd_wrapper.h"

#include <algorithm>

bool IwdWirelessManager::Init()
{
	if (!iwd_get_devices(m_devices))
		return false;

	if (m_devices.empty())
		return false;

	m_current_index = 0;
	for (std::size_t i = 0; i < m_devices.size(); i++)
	{
		if (m_devices[i].powered == "on")
		{
			m_current_index = i;
			break;
		}
	}
	
	return true;
}

bool IwdWirelessManager::Scan()
{
	return iwd_scan(m_devices[m_current_index]);
}

bool IwdWirelessManager::UpdateNetworks()
{
	return iwd_get_networks(m_devices[m_current_index], m_networks);
}

bool IwdWirelessManager::Connect(const Network& network, const std::string& password)
{
	if (!iwd_connect(m_devices[m_current_index], network, password))
		return false;

	for (Network& n : m_networks)
		n.connected = (n.ssid == network.ssid);

	return true;
}

bool IwdWirelessManager::Disconnect()
{
	if (!iwd_disconnect(m_devices[m_current_index]))
		return false;

	for (Network& network : m_networks)
		network.connected = false;

	return true;
}

bool IwdWirelessManager::UpdateKnownNetworks()
{
	return iwd_get_known_networks(m_known_networks);
}

bool IwdWirelessManager::ForgetKnownNetwork(const Network& network)
{
	if (!iwd_forget_known_network(network))
		return false;

	auto it = std::remove_if(m_known_networks.begin(), m_known_networks.end(), [&](const Network& n) { return n.ssid == network.ssid; });
	m_known_networks.erase(it, m_known_networks.end());

	return true;
}

bool IwdWirelessManager::SetCurrentDevice(const Device& device)
{
	auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const auto& d) { return d.name == device.name; });
	if (it == m_devices.end())
		return false;

	m_current_index = std::distance(m_devices.begin(), it);
	return true;
}

bool IwdWirelessManager::ActivateDevice()
{
	Device& current_device = m_devices[m_current_index];

	if (!iwd_adapter_power_on(current_device))
		return false;

	if (!iwd_device_power_on(current_device))
		return false;

	current_device.powered = "on";

	return true;
}