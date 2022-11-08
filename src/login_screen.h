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

class LoginScreenPsk : public LoginScreen
{
public:
	LoginScreenPsk(WirelessManager* wireless_manager, const Network& network);

	virtual bool Done() const override { return m_done; } 
	virtual void Show() override;

private:
	bool					m_is_opened		= false;
	bool					m_done			= false;

	char					m_password[128] {};
	bool					m_hide_password	= true;

	WirelessManager*		m_wireless_manager;
	Network					m_network;
};

class LoginScreen8021x : public LoginScreen
{
public:
	LoginScreen8021x(WirelessManager* wireless_manager, const Network& network);

	virtual bool Done() const override { return m_done; }
	virtual void Show() override;

private:
	std::string GetConfigData() const;

private:
	bool					m_is_opened		= false;
	bool					m_done			= false;

	char					m_anonymous[128] {};
	char 					m_username[128] {};
	char					m_password[128] {};
	bool					m_hide_password	= true;

	WirelessManager*		m_wireless_manager;
	Network					m_network;
};