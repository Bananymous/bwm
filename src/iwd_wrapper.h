#pragma once

#include <vector>
#include <string>

bool	iwd_get_devices(std::vector<std::string>& out);

size_t	iwd_get_networks(const std::string& device, std::vector<std::string>& out);
bool	iwd_scan(const std::string& device);

bool	iwd_connect(const std::string& device, const std::string& ssid, const std::string& password = std::string());
bool	iwd_disconnect(const std::string& device);

bool	iwd_get_known_networks(std::vector<std::string>& out);
bool	iwd_forget_known_network(const std::string& ssid);