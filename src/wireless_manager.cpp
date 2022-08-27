#include "wireless_manager.h"

#include "iwd_wrapper.h"

#include <algorithm>

bool WirelessManager::Init()
{
	switch (m_backend)
	{
		case WirelessBackend::iwd:
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
	}
	return false;
}

bool WirelessManager::Scan()
{
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			return iwd_scan(m_devices[m_current_index]);
	}
	return false;
}

bool WirelessManager::UpdateNetworks()
{
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			return iwd_get_networks(m_devices[m_current_index], m_networks);
	}
	return false;
}

bool WirelessManager::Connect(const Network& network, const std::string& password)
{
	bool success = false;
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			success = iwd_connect(m_devices[m_current_index], network, password);
			break;
	}

	if (success)
	{
		for (Network& m_network : m_networks)
		{
			if (m_network.ssid == network.ssid)
				m_network.connected = true;
			else
				m_network.connected = false;
		}
	}

	return success;
}

bool WirelessManager::Disconnect()
{
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			if (iwd_disconnect(m_devices[m_current_index]))
			{
				for (Network& network : m_networks)
					network.connected = false;
				return true;
			}
			break;
	}
	return false;
}

bool WirelessManager::UpdateKnownNetworks()
{
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			return iwd_get_known_networks(m_known_networks);
	}
	return false;
}

bool WirelessManager::ForgetKnownNetwork(const Network& network)
{
	bool success = false;
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			success = iwd_forget_known_network(network);
			break;
	}

	if (success)
	{
		auto it = std::remove_if(m_known_networks.begin(), m_known_networks.end(), [&network](const Network& n) { return n.ssid == network.ssid; });
		m_known_networks.erase(it, m_known_networks.end());
	}

	return success;
}

bool WirelessManager::SetCurrentDevice(const Device& device)
{
	for (std::size_t i = 0; i < m_devices.size(); i++)
	{
		if (m_devices[i].name == device.name)
		{
			m_current_index = i;
			return true;
		}
	}
	return false;
}

bool WirelessManager::ActivateDevice()
{
	bool success = false;
	switch (m_backend)
	{
		case WirelessBackend::iwd:
			success = iwd_adapter_power_on(m_devices[m_current_index]) && iwd_device_power_on(m_devices[m_current_index]);
			break;
	}

	if (success)
		m_devices[m_current_index].powered = "on";

	return success;
}