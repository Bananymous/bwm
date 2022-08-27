#include "iwd_wrapper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PARSE_COLOR " | sed -e 's/\\x1b\\[[0-9;]*m//g'"

struct PropertyInfo
{
	std::size_t offset;
	std::size_t max_len;
};

static std::string strip_property(const char* buffer, const PropertyInfo& info)
{
	std::size_t pos = info.offset;
	std::size_t len = info.max_len;
	while (isspace(buffer[pos + len - 1]))
		len--;
	return std::string(buffer + pos, len);
}

static bool parse_buffer_property_info(const char* buffer, const char* key, PropertyInfo& out_info)
{
	const char* pkey = strstr(buffer, key);
	if (pkey == NULL)
		return false;
	out_info.offset = pkey - buffer;

	const char* pnext = pkey + strlen(key);
	while (*pnext && isspace(*pnext))
		pnext++;
	out_info.max_len = pnext - pkey;

	return true;
}

bool iwd_get_devices(std::vector<Device>& out)
{
	char buffer[1024];

	FILE* fp = popen("iwctl device list" PARSE_COLOR, "r");
	if (fp == NULL)
		return false;

	PropertyInfo prop_name;
	PropertyInfo prop_address;
	PropertyInfo prop_powered;
	PropertyInfo prop_adapter;
	PropertyInfo prop_mode;

	for (int i = 0; i < 4; i++)
	{
		bool fail = false;
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
			fail = true;
		else if (i == 2)
		{
			if (!fail && !parse_buffer_property_info(buffer, "Name", prop_name))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Address", prop_address))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Powered", prop_powered))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Adapter", prop_adapter))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Mode", prop_mode))
				fail = true;
		}

		if (fail)
		{
			pclose(fp);
			return false;
		}
	}

	std::vector<Device> devices;

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		Device device;
		device.name		= strip_property(buffer, prop_name);
		device.address	= strip_property(buffer, prop_address);
		device.powered	= strip_property(buffer, prop_powered);
		device.adapter	= strip_property(buffer, prop_adapter);
		device.mode		= strip_property(buffer, prop_mode);
		devices.push_back(std::move(device));
	}

	if (pclose(fp) != 0)
		return false;

	out = std::move(devices);
	return true;
}

bool iwd_set_adapter_property(const std::string& adapter, const std::string& property, const std::string& value)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "iwctl adapter %s set-property %s %s &> /dev/null", adapter.c_str(), property.c_str(), value.c_str());
	return system(buffer) == 0;
}

bool iwd_set_device_property(const Device& device, const std::string& property, const std::string& value)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "iwctl device %s set-property %s %s &> /dev/null", device.name.c_str(), property.c_str(), value.c_str());
	return system(buffer) == 0;
}

bool iwd_get_networks(const Device& device, std::vector<Network>& out)
{
	if (device.powered != "on" || device.mode != "station")
		return false;

	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "iwctl station %s get-networks" PARSE_COLOR, device.name.c_str());
	FILE* fp = popen(buffer, "r");
	if (fp == NULL)
		return false;

	PropertyInfo prop_network_name;
	PropertyInfo prop_security;

	for (int i = 0; i < 4; i++)
	{
		bool fail = false;
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
			fail = true;
		else if (i == 2)
		{
			if (!fail && !parse_buffer_property_info(buffer, "Network name", prop_network_name))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Security", prop_security))
				fail = true;
		}

		if (fail)
		{
			pclose(fp);
			return false;
		}
	}

	std::vector<Network> networks;

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		Network network;
		network.ssid		= strip_property(buffer, prop_network_name);
		network.security	= strip_property(buffer, prop_security);
		network.connected	= (buffer[2] == '>');
		networks.push_back(std::move(network));
	}

	if (pclose(fp) != 0)
		return false;

	out = std::move(networks);
	return true;
}

bool iwd_scan(const Device& device)
{
	if (device.powered != "on" || device.mode != "station")
		return false;
	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s scan &> /dev/null", device.name.c_str());
	return system(command) == 0;
}

bool iwd_connect(const Device& device, const Network& network, const std::string& password)
{
	if (device.powered != "on" || device.mode != "station")
		return false;
	char command[1024];
	if (password.empty())
		snprintf(command, sizeof(command), "iwctl --dont-ask station %s connect \"%s\" &> /dev/null", device.name.c_str(), network.ssid.c_str());
	else
		snprintf(command, sizeof(command), "iwctl --passphrase %s station %s connect \"%s\" &> /dev/null", password.c_str(), device.name.c_str(), network.ssid.c_str());
	return system(command) == 0;
}

bool iwd_disconnect(const Device& device)
{
	if (device.powered != "on" || device.mode != "station")
		return false;
	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s disconnect &> /dev/null", device.name.c_str());
	return system(command) == 0;
}

bool iwd_get_known_networks(std::vector<Network>& out)
{
	char buffer[1024];

	FILE* fp = popen("iwctl known-networks list" PARSE_COLOR, "r");
	if (fp == NULL)
		return false;

	PropertyInfo prop_name;
	PropertyInfo prop_security;

	for (int i = 0; i < 4; i++)
	{
		bool fail = false;
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
			fail = true;
		else if (i == 2)
		{
			if (!fail && !parse_buffer_property_info(buffer, "Name", prop_name))
				fail = true;
			if (!fail && !parse_buffer_property_info(buffer, "Security", prop_security))
				fail = true;
		}

		if (fail)
		{
			pclose(fp);
			return false;
		}
	}

	std::vector<Network> networks;

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		Network network;
		network.ssid		= strip_property(buffer, prop_name);
		network.security	= strip_property(buffer, prop_security);
		network.connected	= false;
		networks.push_back(std::move(network));
	}

	if (pclose(fp) != 0)
		return false;

	out = std::move(networks);
	return true;
}

bool iwd_forget_known_network(const Network& network)
{
	char command[1024];
	snprintf(command, sizeof(command), "iwctl known-networks %s forget &> /dev/null", network.ssid.c_str());
	return system(command) == 0;
}



bool iwd_adapter_power_on(const Device& device)		{ return iwd_set_adapter_property(device.adapter, "Powered", "on"); }
bool iwd_adapter_power_off(const Device& device)	{ return iwd_set_adapter_property(device.adapter, "Powered", "off"); }
bool iwd_device_power_on(const Device& device)		{ return iwd_set_device_property(device, "Powered", "on"); }
bool iwd_device_power_off(const Device& device)		{ return iwd_set_device_property(device, "Powered", "off"); }