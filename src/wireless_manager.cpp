#include "wireless_manager.h"

#include "iwd_wireless_manager.h"


WirelessManager* WirelessManager::Create(WirelessBackend backend)
{
	WirelessManager* result = nullptr;

	switch (backend)
	{
		case WirelessBackend::iwd:
			result = new IwdWirelessManager();
			break;
	}

	if (!result)
		return nullptr;

	if (result->Init())
		return result;

	delete result;
	return nullptr;
}
