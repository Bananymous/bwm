#pragma once

#include "structs.h"

#include <vector>
#include <string>

bool iwd_get_devices(std::vector<Device>& out);

bool iwd_set_adapter_property(const std::string& adapter, const std::string& property, const std::string& value);
bool iwd_set_device_property(const Device& device, const std::string& property, const std::string& value);

bool iwd_get_networks(const Device& device, std::vector<Network>& out);
bool iwd_scan(const Device& device);

bool iwd_connect(const Device& device, const Network& network, const std::string& password = std::string());
bool iwd_disconnect(const Device& device);

bool iwd_get_known_networks(std::vector<Network>& out);
bool iwd_forget_known_network(const Network& network);


// Wrappers around powering devices/adapters

bool iwd_adapter_power_on(const Device& device);
bool iwd_adapter_power_off(const Device& device);
bool iwd_device_power_on(const Device& device);
bool iwd_device_power_off(const Device& device);