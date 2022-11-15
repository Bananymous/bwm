#include "login_screen.h"

#include <imgui.h>

#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <strings.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern int		g_argc;
extern char**	g_argv;
extern char**	g_env;

static void close_pair(int pair[2])
{
	close(pair[0]);
	close(pair[1]);
}

static bool write_as_root(const std::string file, const std::string& data)
{
	int stdin_pair[2];
	if (pipe(stdin_pair) == -1)
	{
		std::fprintf(stderr, "pipe()\n");
		std::fprintf(stderr, "  %s\n", strerror(errno));
		return false;
	}

	pid_t pid = vfork();
	if (pid == -1)
	{
		std::fprintf(stderr, "fork()\n");
		std::fprintf(stderr, "  %s\n", strerror(errno));
		close_pair(stdin_pair);
		return false;
	}

	if (pid == 0)
	{
		if (dup2(stdin_pair[0], STDIN_FILENO) == -1)
		{
			std::fprintf(stderr, "dup2()\n");
			std::fprintf(stderr, "  %s\n", strerror(errno));
			exit(1);
		}
		close_pair(stdin_pair);

		int dn = open("/dev/null", O_WRONLY);
		if (dn == -1)
		{
			std::fprintf(stderr, "open()\n");
			std::fprintf(stderr, "  %s\n", strerror(errno));
			exit(1);
		}

		if (dup2(dn, STDOUT_FILENO) == -1 || dup2(dn, STDERR_FILENO))
		{
			std::fprintf(stderr, "dup2()\n");
			std::fprintf(stderr, "  %s\n", strerror(errno));
			exit(1);
		}
		close(dn);

		std::vector<char*> new_env;
		char** ptr = g_env;
		while (*ptr)
			new_env.push_back(*ptr++);

		auto path = std::filesystem::canonical("/proc/self/exe");
		std::string pass = std::string("SUDO_ASKPASS=")	+ path.string();
		new_env.push_back(pass.data());
		new_env.push_back(NULL);

		if (execle("/usr/bin/sudo", "sudo", "-A", "tee", file.data(), NULL, new_env.data()) == -1)
		{
			std::fprintf(stderr, "execve()\n");
			std::fprintf(stderr, "  %s\n", strerror(errno));
			exit(1);
		}
	}

	FILE* fp = fdopen(stdin_pair[1], "w");
	close(stdin_pair[0]);

	if (fp == NULL)
	{
		std::fprintf(stderr, "fdopen()\n");
		std::fprintf(stderr, "  %s\n", strerror(errno));
		close(stdin_pair[1]);
		return false;
	}

	std::fprintf(fp, "%s", data.data());
	fclose(fp);

	int ret;
	if (waitpid(pid, &ret, 0) == -1)
	{
		std::fprintf(stderr, "waitpid()\n");
		std::fprintf(stderr, "  %s\n", strerror(errno));
		close_pair(stdin_pair);
		return false;
	}

	return ret == 0;
}

static std::string get_iwd_file_name(const Network& network)
{
	bool basic = true;
	for (char c : network.ssid)
		if (!isalnum(c) && c != '-' && c != '_')
			basic = false;

	std::stringstream ss;

	ss << "/var/lib/iwd/";

	if (basic)
	{
		ss << network.ssid;
	}
	else
	{
		ss << "=";
		ss << std::hex << std::setfill('0') << std::setw(2);
		for (char c : network.ssid)
			ss << (int)c;
	}
	
	ss << "." << network.security;

	return ss.str();
}


LoginScreen* LoginScreen::Create(WirelessManager* wireless_manager, const Network& network)
{
	assert(wireless_manager);

	if (network.security == "psk")
		return new LoginScreenPsk(wireless_manager, network);
	if (network.security == "8021x")
		return new LoginScreen8021x(wireless_manager, network);

	std::fprintf(stderr, "Unsupported security type\n");
	return nullptr;
}



/*
		Psk
*/

LoginScreenPsk::LoginScreenPsk(WirelessManager* wireless_manager, const Network& network)
	: m_wireless_manager(wireless_manager)
	, m_network(network)
{
}

void LoginScreenPsk::Show()
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

	bool connect	= false;
	bool close		= false;

	ImGui::Text("ssid: %s", m_network.ssid.c_str());

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
	if (m_hide_password)
		flags |= ImGuiInputTextFlags_Password;
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



/*
		8021x
*/

LoginScreen8021x::LoginScreen8021x(WirelessManager* wireless_manager, const Network& network)
	: m_wireless_manager(wireless_manager)
	, m_network(network)
{
	
}

void LoginScreen8021x::Show()
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

	bool connect	= false;
	bool close		= false;

	ImGui::Text("ssid: %s", m_network.ssid.c_str());

	ImGui::InputText("anonymous", m_anonymous, sizeof(m_anonymous));
	ImGui::InputText("username", m_username, sizeof(m_username));

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
	if (m_hide_password)
		flags |= ImGuiInputTextFlags_Password;
	if (ImGui::InputText("password", m_password, sizeof(m_password), flags))
		connect = true;

	ImGui::SameLine();
	if (ImGui::Button("Show"))
		m_hide_password = !m_hide_password;

	if (ImGui::Button("Connect"))
		connect = true;

	if (connect && m_username[0] && m_password[0])
	{
		auto config_data = GetConfigData();
		auto file_name = get_iwd_file_name(m_network);

		if (write_as_root(file_name, config_data))
		{
			// iwd does not seem to reload /var/lib/iwd
			// unless it is restarted.
			if (std::system("sudo -n systemctl restart iwd") == 0)
			{
				std::this_thread::sleep_for(std::chrono::seconds(3));
				if (m_wireless_manager->Connect(m_network))
					close = true;
			}
			else
			{
				std::fprintf(stderr, "Could not restart iwd\n");
			}
		}
		else
		{
			std::fprintf(stderr, "Could not write file\n");
		}

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

std::string LoginScreen8021x::GetConfigData() const
{
	/*
	[Security]
	EAP-Method=PEAP
	EAP-Identity=anonymous@realm.edu
	EAP-PEAP-CACert=/path/to/root.crt
	EAP-PEAP-ServerDomainMask=radius.realm.edu
	EAP-PEAP-Phase2-Method=MSCHAPV2
	EAP-PEAP-Phase2-Identity=johndoe@realm.edu
	EAP-PEAP-Phase2-Password=hunter2
	*/

	// FIXME: do not hardcode method

	char anonymous[sizeof(m_anonymous)] {};
	if (m_anonymous[0])
		strncpy(anonymous, m_anonymous, sizeof(anonymous));
	else if (auto ptr = index(m_username, '@'))
		snprintf(anonymous, sizeof(anonymous), "anonymous%s", ptr);

	std::stringstream ss;
	ss << "[Security]"									<< std::endl;
	ss << "EAP-Method=PEAP"								<< std::endl;
	if (anonymous[0])
		ss << "EAP-Identity="			<< anonymous	<< std::endl;
	ss << "EAP-PEAP-Phase2-Method=MSCHAPV2"				<< std::endl;
	ss << "EAP-PEAP-Phase2-Identity="	<< m_username	<< std::endl;
	ss << "EAP-PEAP-Phase2-Password="	<< m_password	<< std::endl;

	return ss.str();
}