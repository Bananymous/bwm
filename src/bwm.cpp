#include "iwd_wrapper.h"

#include <imgui.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

static bool		s_dragging		= false;
static double	s_cursor[2]		= {};
static double	s_cursor_off[2]	= {};

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
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


template<typename... Args>
void config_error(FILE* fp, int line, const char* fmt, Args&&... args)
{
	fclose(fp);
	fprintf(stderr, "Error on config (line %d)\n  ", line);
	fprintf(stderr, fmt, args...);
	fprintf(stderr, "\n");
	exit(1);
}

std::vector<std::string> split_whitespace(const std::string& data)
{
	std::vector<std::string> result;

	std::string::const_iterator begin	= data.begin();
	std::string::const_iterator end		= data.begin();

	while (end != data.end())
	{
		begin = std::find_if_not(end, data.end(), isspace);
		if (begin == data.end())
			break;
		end = std::find_if(begin, data.end(), isspace);

		result.emplace_back(begin, end);
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
		if (!('0' <= str[i] && str[i] <= '9') &&
			!('a' <= str[i] && str[i] <= 'f') &&
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


int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	constexpr int c_width = 400, c_height = 400;

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return EXIT_FAILURE;

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(c_width, c_height, "bwm", NULL, NULL);
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

	std::vector<std::string> devices;
	if (!iwd_get_devices(devices))
	{
		fprintf(stderr, "Could not get wireless devices\n");
		return 1;
	}
	if (devices.empty())
	{
		fprintf(stderr, "No internet devices available\n");
		return 1;
	}
	
	std::string current_device = devices.front();

	iwd_scan(current_device);
	auto last_scan = std::chrono::steady_clock::now();

	std::vector<std::string> networks;
	size_t connected = iwd_get_networks(current_device, networks);
	auto last_get = std::chrono::steady_clock::now();

	std::vector<std::string> known_networks;

	struct PasswordPrompt
	{
		std::string ssid;
		char password[128];
		bool show;
	};
	PasswordPrompt password_prompt;

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
		ImGui::SetNextWindowSize(ImVec2(c_width, c_height));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGui::Begin("Window", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize
		);

		// Scan and update networks on specified intervals
		if (std::chrono::steady_clock::now() >= last_scan + 10s)
		{
			iwd_scan(current_device);
			last_scan += 10s;
		}
		if (std::chrono::steady_clock::now() >= last_get + 2s)
		{
			connected = iwd_get_networks(current_device, networks);
			last_get += 2s;;
		}

		// Create dropdown for devices
		if (ImGui::BeginCombo("Device", current_device.c_str()))
		{
			for (const std::string& device : devices)
			{
				bool selected = (current_device == device);
				if (ImGui::Selectable(device.c_str(), selected))
					current_device = device;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Known"))
		{
			iwd_get_known_networks(known_networks);
			ImGui::OpenPopup("known-networks");
		}

		if (ImGui::BeginPopupModal("known-networks", NULL,
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		))
		{
			for (size_t i = 0; i < known_networks.size(); i++)
			{
				const std::string& known = known_networks[i];
				const size_t id_offset = networks.size();

				const float child_padding	= 10.0f;
				const float text_padding	= 10.0f;
				const float button_padding	= 5.0f;

				const float font_size 		= ImGui::GetFontSize();

				const ImVec2 child_size		= ImVec2(c_width - 2.0f * child_padding, font_size + 2.0f * text_padding);
				const ImVec2 button_size	= ImVec2(ImGui::CalcTextSize("Forget").x + 10.0f, child_size.y - 2.0f * button_padding);

				ImGui::BeginChild(id_offset + i + 1, child_size);
				
				ImGui::SetCursorPosX(text_padding);
				ImGui::SetCursorPosY(text_padding);

				ImGui::Text("%s", known.c_str());

				ImGui::SetCursorPosX(child_size.x - button_size.x - button_padding);
				ImGui::SetCursorPosY(button_padding);

				if (ImGui::Button("Forget", button_size))
				{
					if (iwd_forget_known_network(known))
					{
						known_networks.erase(known_networks.begin() + i);
						i--;
					}
				}

				ImGui::EndChild();
			}

			//ImGui::SetCursorPosX(c_width - ImGui::CalcTextSize("Close").x - 25.0f);

			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::Spacing();
		ImGui::Spacing();

		bool open_popup = false;
		for (size_t i = 0; i < networks.size(); i++)
		{
			const std::string& ssid = networks[i];

			const float child_padding	= 10.0f;
			const float text_padding	= 10.0f;
			const float button_padding	= 5.0f;

			const float font_size 		= ImGui::GetFontSize();

			const ImVec2 child_size		= ImVec2(c_width - 2.0f * child_padding, font_size + 2.0f * text_padding);
			const ImVec2 button_size	= ImVec2(ImGui::CalcTextSize("Disconnect").x + 10.0f, child_size.y - 2.0f * button_padding);

			ImGui::BeginChild(i + 1, child_size);

			ImGui::SetCursorPosX(text_padding);
			ImGui::SetCursorPosY(text_padding);

			ImGui::Text("%s", ssid.c_str());

			ImGui::SetCursorPosX(child_size.x - button_size.x - button_padding);
			ImGui::SetCursorPosY(button_padding);

			if (i == connected)
			{
				if (ImGui::Button("Disconnect", button_size))
					if (iwd_disconnect(current_device))
						connected = -1;
			}
			else
			{
				if (ImGui::Button("Connect", button_size))
				{
					if (iwd_connect(current_device, ssid))
					{
						connected = i;
					}
					else
					{
						password_prompt.ssid = ssid;
						password_prompt.password[0] = '\0';
						password_prompt.show = false;
						open_popup = true;
					}
				}
			}

			ImGui::EndChild();
		}

		if (open_popup)
			ImGui::OpenPopup("password");

		if (ImGui::BeginPopupModal("password", NULL,
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		))
		{
			ImGuiInputTextFlags flags = 0;
			flags |= ImGuiInputTextFlags_EnterReturnsTrue;
			if (!password_prompt.show)
				flags |= ImGuiInputTextFlags_Password;

			bool connect = false;

			if (ImGui::InputText("##", password_prompt.password, sizeof(password_prompt.password), flags))
				connect = true;

			ImGui::SameLine();
			if (ImGui::Button("Show"))
				password_prompt.show = !password_prompt.show;

			if (ImGui::Button("Connect"))
				connect = true;

			if (connect && password_prompt.password[0] != '\0')
			{
				if (iwd_connect(current_device, password_prompt.ssid, password_prompt.password))
				{
					for (size_t i = 0; i < networks.size(); i++)
						if (networks[i] == password_prompt.ssid)
							connected = i;
					ImGui::CloseCurrentPopup();
				}
				password_prompt.password[0] = '\0';
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
