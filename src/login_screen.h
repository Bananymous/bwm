#pragma once

#include "wireless_manager.h"

class LoginScreen
{
protected:
	LoginScreen() {}
public:
	static LoginScreen* Create(WirelessManager* wireless_manager, const Network& network);

	virtual ~LoginScreen() {}

	virtual bool Done() const = 0;
	virtual void Show() = 0;
};

class PskLoginScreen : public LoginScreen
{
public:
	PskLoginScreen(WirelessManager* wireless_manager, const Network& network);

	virtual bool Done() const override { return m_done; } 
	virtual void Show() override;

private:
	bool					m_is_opened = false;
	bool					m_done;

	char					m_password[128];
	bool					m_hide_password = true;

	WirelessManager*		m_wireless_manager;
	Network					m_network;
};