#include <imgui.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void windowOnCursorPosition(GLFWwindow* window, double x, double y);
static void windowOnMouseButton(GLFWwindow* window, int button, int action, int mods);


struct Network
{
	std::string		ssid;
	int				strength;
	bool			connected;
};

struct PasswordPrompt
{
	char password[128];
	std::string ssid;
};
static PasswordPrompt s_password_prompt;

template<typename... Args>
void config_error(FILE* fp, int line, const char* fmt, Args&&... args)
{
	fclose(fp);
	fprintf(stderr, "Error on config (line %d)\n  ", line);
	fprintf(stderr, fmt, args...);
	fprintf(stderr, "\n");
	exit(1);
}

std::vector<std::string> split_whitespace(const char* data)
{
	std::vector<std::string> result;
	
	bool had_whitespace = true;
	for (int i = 0; data[i] != '\0'; i++)
	{
		if (isspace(data[i]))
		{
			had_whitespace = true;
		}
		else
		{
			if (had_whitespace)
				result.push_back("");
			result.back().push_back(data[i]);
			had_whitespace = false;
		}
	}

	return result;
}

bool hexstr_to_color(const std::string& str, ImVec4& out)
{
	if (str.empty() || str.front() != '#')
		return false;

	if (str.size() != 7 && str.size() != 9)
		return false;

	for (size_t i = 1; i < str.size(); i++)
		if (!('0' <= str[i] && str[i] <= '9') ||
			!('a' <= str[i] && str[i] <= 'f') ||
			!('A' <= str[i] && str[i] <= 'F')
		)
			return false;

	unsigned long val = strtoul(str.c_str() + 1, NULL, 16);

	int r, g, b, a;

	if (str.size() == 7)
	{
		r = (val >> 16) & 0xFF;
		g = (val >>  8) & 0xFF;
		b = (val >>  0) & 0xFF;
		a = 255;
	}
	else
	{
		r = (val >> 24) & 0xFF;
		g = (val >> 16) & 0xFF;
		b = (val >>  8) & 0xFF;
		a = (val >>  0) & 0xFF;
	}

	out.x = (float)r / 255.0f;
	out.y = (float)g / 255.0f;
	out.z = (float)b / 255.0f;
	out.w = (float)a / 255.0f;

	return true;
}

std::string get_font_path(const std::string& font_name)
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "fc-match --format=%%{file} '%s'", font_name.c_str());

	FILE* fp = popen(buffer, "r");
	if (fp == NULL)
		return font_name;

	char* ptr = fgets(buffer, sizeof(buffer), fp);

	if (pclose(fp) != 0 || ptr == NULL)
		return font_name;

	return buffer;
}

void load_config()
{
	char buffer[1024];

	passwd* pwd = getpwuid(getuid());
	snprintf(buffer, sizeof(buffer), "%s/.config/bwm/config", pwd->pw_dir);

	FILE* fp = fopen(buffer, "r");
	if (fp == NULL)
		return;

	ImGuiIO&	io		= ImGui::GetIO();
	ImGuiStyle&	style	= ImGui::GetStyle();

	std::unordered_map<std::string, ImGuiCol> color_words;
	color_words["background"]			= ImGuiCol_WindowBg;
	color_words["button"]				= ImGuiCol_Button;
	color_words["button_active"]		= ImGuiCol_ButtonActive;
	color_words["button_hover"]			= ImGuiCol_ButtonHovered;
	color_words["wifi_background"]		= ImGuiCol_ChildBg;
	color_words["dropdown"]				= ImGuiCol_FrameBg;
	color_words["dropdown_hover"]		= ImGuiCol_FrameBgHovered;
    color_words["selectable"]			= ImGuiCol_Header;
    color_words["selectable_active"]	= ImGuiCol_HeaderActive;
    color_words["selectable_hover"]		= ImGuiCol_HeaderHovered;
	color_words["popup_background"]		= ImGuiCol_PopupBg;
	color_words["popup_shadow"]			= ImGuiCol_ModalWindowDimBg;

	int line = 0;
	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		line++;

		std::vector<std::string> splitted = split_whitespace(buffer);
		
		if (splitted.empty() || splitted.front().front() == '#')
			continue;

		if (color_words.find(splitted[0]) != color_words.end())
		{
			if (splitted.size() != 2)
				config_error(fp, line, "usage: %s <color>", splitted[0].c_str());
			
			ImVec4 color;
			if (!hexstr_to_color(splitted[1], color))
				config_error(fp, line, "specify color as hex string '#xxxxxx' or '#xxxxxxxx'");
			
			style.Colors[color_words[splitted[0]]] = color;
		}
		else if (splitted.front() == "font")
		{
			if (splitted.size() < 3)
				config_error(fp, line, "usage: font <font name>, <size>");

			if (splitted[splitted.size() - 2].back() != ',')
				config_error(fp, line, "usage: font <font name>, <size>");

			std::string font = splitted[1];
			for (size_t i = 2; i < splitted.size() - 1; i++)
				font += ' ' + splitted[i];
			font.pop_back();

			std::string file = get_font_path(font);
			if (file.empty())
				config_error(fp, line, "could not find font '%s'", font.c_str());

			float size;

			try
			{
				size = std::stof(splitted.back());
			}
			catch(const std::exception& e)
			{
				config_error(fp, line, "font size not a number");
			}
			

			io.Fonts->AddFontFromFileTTF(file.c_str(), size);
		}
		else
		{
			config_error(fp, line, "unknown keyword '%s'", splitted[0].c_str());
		}


	}

	fclose(fp);
}

std::vector<std::string> get_iwd_devices()
{
	char buffer[1024];

	FILE* fp = popen("iwctl device list", "r");
	if (fp == NULL)
		return { "NULL" };

	std::vector<std::string> devices;

	// Skip first 4 lines since they are headers
	for (int i = 0; i < 4; i++)
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
			return { "NULL" };

	// Parse out device names
	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			continue;

		int start	= 2;
		int end		= 22;

		while (buffer[end - 1] == ' ' && end > start)
			end--;

		if (end != start)
			devices.push_back(std::string(buffer + start, end - start));
	}

	if (pclose(fp) != 0)
		return { "NULL" };

	return devices;
}

void scan_iwd_networks(const std::string& device)
{
	if (device == "NULL")
		return;

	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s scan > /dev/null 2>&1", device.c_str());
	system(command);
}

std::vector<Network> get_iwd_networks(const std::string& device)
{
	if (device == "NULL")
		return {};

	char buffer[1024];

	std::string command = "iwctl station " + device + " get-networks";
	FILE* fp = popen(command.c_str(), "r");
	if (fp == NULL)
		return {};
	
	std::vector<Network> networks;

	// Skip first 4 lines since they are headers
	for (int i = 0; i < 4; i++)
		if (fgets(buffer, sizeof(buffer), fp) == NULL)
			return {};

	while (fgets(buffer, sizeof(buffer), fp) != NULL)
	{
		if (buffer[0] == '\n')
			break;

		std::string stripped;
		for (uint32_t i = 0; i < sizeof(buffer) && buffer[i] != '\n'; i++)
		{
			if (buffer[i] == '\033')
			{
				while (buffer[i] != 'm')
					i++;
				continue;
			}
			stripped.push_back(buffer[i]);
		}

		Network network;

		network.ssid = stripped.substr(4, 32);
		while (network.ssid.back() == ' ')
			network.ssid.pop_back();
		
		network.strength = 0;

		network.connected = (stripped[2] == '>');

		networks.push_back(network);
	}

	if (pclose(fp) != 0)
		return {};

	return networks;
}

void iwd_disconnect(const std::string& device)
{
	if (device == "NULL")
		return;

	char command[1024];
	snprintf(command, sizeof(command), "iwctl station %s disconnect", device.c_str());
	system(command);
}

bool iwd_connect(const std::string& device, const std::string& ssid)
{
	if (device == "NULL")
		return false;
	
	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "iwctl --dont-ask station %s connect \"%s\"", device.c_str(), ssid.c_str());

	FILE* fp = popen(buffer, "r");
	if (fp == NULL)
		return false;

	return pclose(fp) == 0;
}

bool iwd_connect_password(const std::string& device, const std::string& ssid, const std::string& password)
{
	if (device == "NULL")
		return false;

	if (password[0] == '\0')
		return iwd_connect(device, ssid);

	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "iwctl --passphrase %s station %s connect \"%s\"", password.c_str(), device.c_str(), ssid.c_str());

	FILE* fp = popen(buffer, "r");
	if (fp == NULL)
		return false;

	return pclose(fp) == 0;
}



static bool		s_dragging		= false;
static double	s_cursor[2]		= {};
static double	s_cursor_off[2]	= {};

constexpr int width = 400, height = 400;

int main(int argc, char** argv)
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return EXIT_FAILURE;

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(width, height, "bwm", NULL, NULL);
	if (window == NULL)
		return EXIT_FAILURE;
    glfwSetCursorPosCallback(window, windowOnCursorPosition);
    glfwSetMouseButtonCallback(window, windowOnMouseButton);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Init ImGui backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Disable imgui.ini file
	ImGui::GetIO().IniFilename = NULL;

	// Load config file
	load_config();

	auto iwd_devices		= get_iwd_devices();
	auto iwd_device_current	= iwd_devices.front();

	scan_iwd_networks(iwd_device_current);
	auto iwd_networks = get_iwd_networks(iwd_device_current);

	auto last_scan 	= std::chrono::system_clock::now();
	auto last_get	= std::chrono::system_clock::now();

	std::unordered_map<std::string, bool> header_open;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (s_dragging && (s_cursor_off[0] != 0 || s_cursor_off[1] != 0))
		{
			int win[2];
			glfwGetWindowPos(window, &win[0], &win[1]);

			win[0] += (int)s_cursor_off[0];
			win[1] += (int)s_cursor_off[1];

            glfwSetWindowPos(window, win[0], win[1]);
			s_cursor_off[0] = 0.0;
			s_cursor_off[1] = 0.0;
		}

		// Create new frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Set ImGui window to fill whole application window
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		ImGui::SetNextWindowSize(ImVec2(width, height));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGui::Begin("Window", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize
		);

		if (std::chrono::system_clock::now() >= last_scan + std::chrono::seconds(10))
		{
			scan_iwd_networks(iwd_device_current);
			last_scan = std::chrono::system_clock::now();
		}

		if (std::chrono::system_clock::now() >= last_get + std::chrono::seconds(2))
		{
			iwd_networks = get_iwd_networks(iwd_device_current);
			last_get = std::chrono::system_clock::now();
		}

		if (ImGui::BeginCombo("Device", iwd_device_current.c_str()))
		{
			for (const std::string& device : iwd_devices)
			{
				bool selected = (iwd_device_current == device);
				if (ImGui::Selectable(device.c_str(), selected))
					iwd_device_current = device;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::Spacing();

		const float		font_h		= ImGui::GetFontSize();
		const float		child_h		= font_h + 20.0f;
		const ImVec2	button_size = ImVec2(ImGui::CalcTextSize("Disconnect").x + 20.0f, child_h - 10.0f);

		const float button_offset	= (child_h - button_size.y) / 2.0f;
		const float text_offset		= (child_h - font_h) / 2.0f;

		bool open_popup = false;

		int index = 1;
		for (Network& network : iwd_networks)
		{
			ImGui::BeginChild(index, ImVec2(0, child_h));
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + text_offset);

			ImGui::Text("%s", network.ssid.c_str());

			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - text_offset + button_offset);

			ImGui::SetCursorPosX(380 - button_size.x);

			if (network.connected)
			{
				if (ImGui::Button("Disconnect", button_size))
				{
					iwd_disconnect(iwd_device_current);
					iwd_networks = get_iwd_networks(iwd_device_current);
				}
			}
			else
			{
				if (ImGui::Button("Connect", button_size))
				{
					if (iwd_connect(iwd_device_current, network.ssid))
						iwd_networks = get_iwd_networks(iwd_device_current);
					else
					{
						s_password_prompt.password[0] = '\0';
						s_password_prompt.ssid = network.ssid;
						open_popup = true;
					}
				}
			}

			ImGui::EndChild();

			index++;
		}

		if (open_popup)
			ImGui::OpenPopup("password");

		if (ImGui::BeginPopupModal("password", NULL,
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar
		))
		{
			static bool show_password = false;
			ImGuiInputTextFlags flags = 0;
			flags |= ImGuiInputTextFlags_EnterReturnsTrue;
			if (!show_password)
				flags |= ImGuiInputTextFlags_Password;

			bool connect = false;

			if (ImGui::InputText("password", s_password_prompt.password, sizeof(s_password_prompt.password), flags))
				connect = true;

			ImGui::SameLine();
			if (ImGui::Button("Show"))
				show_password = !show_password;

			if (ImGui::Button("Connect"))
				connect = true;

			if (connect)
			{
				if (iwd_connect_password(iwd_device_current, s_password_prompt.ssid, s_password_prompt.password))
				{
					iwd_networks = get_iwd_networks(iwd_device_current);
					ImGui::CloseCurrentPopup();
				}
				else
				{
					s_password_prompt.password[0] = '\0';
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::End();

		// Render
		ImGui::Render();

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}

static void windowOnCursorPosition(GLFWwindow* window, double x, double y)
{
	if (s_dragging)
	{
		s_cursor_off[0] = x - s_cursor[0];
		s_cursor_off[1] = y - s_cursor[1];
	}
}

static void windowOnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	if (action == GLFW_PRESS)
	{
		s_dragging = true;

		double x, y;
		glfwGetCursorPos(window, &x, &y);
		s_cursor[0] = x;
		s_cursor[1] = y;
	}
	else if (action == GLFW_RELEASE)
	{
		s_dragging = false;

		s_cursor[0] = 0.0;
		s_cursor[1] = 0.0;
	}
}
