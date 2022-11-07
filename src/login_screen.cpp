#include "login_screen.h"

#include <imgui.h>

#include <cassert>

LoginScreen* LoginScreen::Create(WirelessManager* wireless_manager, const Network& network)
{
	assert(wireless_manager);

	if (network.security == "psk")
		return new PskLoginScreen(wireless_manager, network);

	std::fprintf(stderr, "Unsupported security type\n");
	return nullptr;
}

PskLoginScreen::PskLoginScreen(WirelessManager* wireless_manager, const Network& network)
	: m_wireless_manager(wireless_manager)
	, m_network(network)
{
}

void PskLoginScreen::Show()
{
	if (!m_is_opened)
		ImGui::OpenPopup("login-screen");
	
	if (!ImGui::BeginPopupModal("login-screen", NULL,
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove
	))
	{
		return;
	}

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
	flags |= ImGuiInputTextFlags_EnterReturnsTrue;
	if (m_hide_password)
		flags |= ImGuiInputTextFlags_Password;

	bool connect	= false;
	bool close		= false;

	ImGui::Text("ssid: %s", m_network.ssid.c_str());

	if (ImGui::InputText("##", m_password, sizeof(m_password), flags))
		connect = true;

	ImGui::SameLine();
	if (ImGui::Button("Show"))
		m_hide_password = !m_hide_password;

	if (ImGui::Button("Connect"))
		connect = true;

	if (connect)
	{
		if (m_wireless_manager->Connect(m_network, m_password))
			close = true;
		m_password[0] = '\0';
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		close = true;

	if (close)
	{
		m_done = true;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
	
}