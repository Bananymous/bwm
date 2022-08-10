#include "iwd_wrapper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PARSE_COLOR " | sed -e 's/\\x1b\\[[0-9;]*m//g'"

bool iwd_get_devices(std::vector<std::string>& out)
{
	char buffer[1024];

	FILE* fp = popen("iwctl device list" PARSE_COLOR, "r");
	if (fp == NULL)
		return false;


    size_t offset = 0;
	for (int i = 0; i < 4; i++)
	{
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
		{
			pclose(fp);	
			return false;
		}
        if (i == 2)
        {
            const char* pos = strstr(buffer, "Name");
            if (pos == NULL)
            {
                pclose(fp);
                return false;
            }
            offset = pos - buffer;
        }
	}

	std::vector<std::string> devices;

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		size_t start	= offset;
		size_t len		= 20;

		while (buffer[start + len - 1] == ' ')
			len--;

		devices.emplace_back(buffer + start, len);
	}

	if (pclose(fp) != 0)
		return false;

	out = std::move(devices);
	return true;
}

size_t iwd_get_networks(const std::string& device, std::vector<std::string>& out)
{
	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "iwctl station %s get-networks" PARSE_COLOR, device.c_str());

	FILE* fp = popen(buffer, "r");
	if (fp == NULL)
		return -2;
	
    size_t offset = 0;
	for (int i = 0; i < 4; i++)
	{
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
		{
			pclose(fp);
			return -2;
		}
        if (i == 2)
        {
            const char* pos = strstr(buffer, "Network name");
            if (pos == NULL)
            {
                pclose(fp);
                return false;
            }
            offset = pos - buffer;
        }
	}

	size_t connected_index = -1;
	std::vector<std::string> networks;

	size_t index = 0;
	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		size_t start = offset;
		size_t len = 32;

        for (int i = 0; i < start; i++)
            if (buffer[i] == '>')
            connected_index = index;

		while (buffer[start + len - 1] == ' ')
			len--;

		networks.emplace_back(buffer + start, len);

		index++;
	}

	if (pclose(fp) != 0)
		return -2;

	out = std::move(networks);
	return connected_index;
}

bool iwd_scan(const std::string& device)
{
	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s scan &> /dev/null", device.c_str());
	return system(command) == 0;
}

bool iwd_connect(const std::string& device, const std::string& ssid, const std::string& password)
{
	char command[1024];
	if (password.empty())
		snprintf(command, sizeof(command), "iwctl --dont-ask station %s connect \"%s\" &> /dev/null", device.c_str(), ssid.c_str());
	else
		snprintf(command, sizeof(command), "iwctl --passphrase %s station %s connect \"%s\" &> /dev/null", password.c_str(), device.c_str(), ssid.c_str());
	return system(command) == 0;
}

bool iwd_disconnect(const std::string& device)
{
	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s disconnect &> /dev/null", device.c_str());
	return system(command) == 0;
}

bool iwd_get_known_networks(std::vector<std::string>& out)
{
	char buffer[1024];

	FILE* fp = popen("iwctl known-networks list" PARSE_COLOR, "r");
	if (fp == NULL)
		return false;

    size_t offset = 0;
	for (int i = 0; i < 4; i++)
	{
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
		{
			pclose(fp);
			return false;
		}
        if (i == 2)
        {
            const char* pos = strstr(buffer, "Name");
            if (pos == NULL)
            {
                pclose(fp);
                return false;
            }
            offset = pos - buffer;
        }
	}

	std::vector<std::string> networks;

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		size_t start	= offset;
		size_t len		= 32;

		while (buffer[start + len - 1] == ' ')
			len--;

		networks.emplace_back(buffer + start, len);
	}

	if (pclose(fp) != 0)
		return false;

	out = std::move(networks);
	return true;
}

bool iwd_forget_known_network(const std::string& ssid)
{
	char command[1024];
	snprintf(command, sizeof(command), "iwctl known-networks %s forget &> /dev/null", ssid.c_str());
	return system(command) == 0;
}
