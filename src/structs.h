#pragma once

#include <string>

struct Device
{
	std::string	name;
	std::string address;
	std::string powered;
	std::string adapter;
	std::string mode;
};

struct Network
{
	std::string ssid;
	std::string security;
	bool		connected;
};