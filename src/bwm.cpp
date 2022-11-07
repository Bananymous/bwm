#include "wireless_manager.h"
#include "config.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <chrono>

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void cleanup(GLFWwindow* window)
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
}

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	(void)argv;
	if (argc != 1)
	{
		fprintf(stderr, "bwm does not support commandline arguments\n");
		return EXIT_FAILURE;
	}

	constexpr int c_width = 400, c_height = 400;

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		fprintf(stderr, "Could not initalize glfw\n");
		return EXIT_FAILURE;
	}

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(c_width, c_height, "bwm", NULL, NULL);
	if (window == NULL)
	{
		fprintf(stderr, "Could not create window\n");
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;

	// Init ImGui backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Load config file
	if (!ParseConfig())
	{
		cleanup(window);
		return EXIT_FAILURE;
	}

	WirelessManager* wireless_manager = WirelessManager::Create(WirelessBackend::iwd);
	if (!wireless_manager)
	{
		fprintf(stderr, "Could not initialize wireless backend\n");
		return EXIT_FAILURE;
	}

	wireless_manager->Scan();
	wireless_manager->UpdateNetworks();

	auto next_scan = std::chrono::steady_clock::now() + 10s;
	auto next_update = std::chrono::steady_clock::now() + 2s;

	struct PasswordPrompt
	{
		bool		active			= false;
		Network		network;
		char		password[128];
		bool		show_password;
	};
	PasswordPrompt password_prompt;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Create new frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Set ImGui window to fill whole application window
		ImGui::SetNextWindowSize(ImVec2(c_width, c_height));
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));

		ImGui::Begin("Window", NULL,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		);

		// If password prompt is active we cannot update network list since
		// password_prompt holds a reference to a network in networks object. 
		if (password_prompt.active == false && wireless_manager->GetCurrentDevice().powered == "on")
		{
			// Scan and update networks on specified intervals
			auto current_time = std::chrono::steady_clock::now();
			if (current_time >= next_scan)
			{
				wireless_manager->Scan();
				next_scan = current_time + 10s;
			}
			if (current_time >= next_update)
			{
				wireless_manager->UpdateNetworks();
				next_update = current_time + 2s;
			}
		}

		// Create dropdown for devices
		if (ImGui::BeginCombo("Device", wireless_manager->GetCurrentDevice().name.c_str()))
		{
			const auto& devices			= wireless_manager->GetDevices();
			const auto& current_device	= wireless_manager->GetCurrentDevice();

			for (std::size_t i = 0; i < devices.size(); i++)
			{
				bool selected = (&current_device == &devices[i]);
				if (ImGui::Selectable(devices[i].name.c_str(), selected))
					wireless_manager->SetCurrentDevice(devices[i]);
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImVec2 known_button_size = ImGui::CalcTextSize("Known");
		known_button_size.x += 20.0f;
		known_button_size.y += 10.0f;

		ImGui::SameLine();
		if (ImGui::Button("Known", known_button_size))
		{
			wireless_manager->UpdateKnownNetworks();
			ImGui::OpenPopup("known-networks");
		}

		if (ImGui::BeginPopupModal("known-networks", NULL,
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		))
		{
			const auto& known_networks = wireless_manager->GetKnownNetworks();

			for (size_t i = 0; i < known_networks.size(); i++)
			{
				const Network& known = known_networks[i];
				const size_t id_offset = wireless_manager->GetNetworks().size();

				const float child_padding	= 10.0f;
				const float text_padding	= 10.0f;
				const float button_padding	= 5.0f;

				const float font_size 		= ImGui::GetFontSize();

				const ImVec2 child_size		= ImVec2(c_width - 2.0f * child_padding, font_size + 2.0f * text_padding);
				const ImVec2 button_size	= ImVec2(ImGui::CalcTextSize("Forget").x + 20.0f, child_size.y - 2.0f * button_padding);

				ImGui::BeginChild(id_offset + i + 1, child_size);
				
				ImGui::SetCursorPosX(text_padding);
				ImGui::SetCursorPosY(text_padding);

				ImGui::Text("%s", known.ssid.c_str());

				ImGui::SetCursorPosX(child_size.x - button_size.x - button_padding);
				ImGui::SetCursorPosY(button_padding);

				if (ImGui::Button("Forget", button_size))
				{
					wireless_manager->ForgetKnownNetwork(known);
					i = 100;
				}

				ImGui::EndChild();
			}

			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::Spacing();
		ImGui::Spacing();


		bool open_password_prompt = false;

		if (wireless_manager->GetCurrentDevice().powered != "on")
		{
			if (ImGui::Button("Activate device"))
			{
				if (wireless_manager->ActivateDevice())
				{
					next_scan = std::chrono::steady_clock::now();
					next_update  = std::chrono::steady_clock::now();
				}
			}
		}
		else
		{
			for (size_t i = 0; i < wireless_manager->GetNetworks().size(); i++)
			{
				const Network& network = wireless_manager->GetNetworks()[i];

				const float child_padding	= 10.0f;
				const float text_padding	= 10.0f;
				const float button_padding	= 5.0f;

				const float font_size 		= ImGui::GetFontSize();

				const ImVec2 child_size		= ImVec2(c_width - 2.0f * child_padding, font_size + 2.0f * text_padding);
				const ImVec2 button_size	= ImVec2(ImGui::CalcTextSize("Disconnect").x + 10.0f, child_size.y - 2.0f * button_padding);

				ImGui::BeginChild(i + 1, child_size);

				ImGui::SetCursorPosX(text_padding);
				ImGui::SetCursorPosY(text_padding);

				ImGui::Text("%s", network.ssid.c_str());

				ImGui::SetCursorPosX(child_size.x - button_size.x - button_padding);
				ImGui::SetCursorPosY(button_padding);

				if (network.connected)
				{
					if (ImGui::Button("Disconnect", button_size))
						wireless_manager->Disconnect();
				}
				else
				{
					if (ImGui::Button("Connect", button_size))
					{
						if (!wireless_manager->Connect(network))
						{
							password_prompt.network = network;
							password_prompt.password[0] = '\0';
							password_prompt.show_password = false;
							password_prompt.active = true;
							open_password_prompt = true;
						}
					}
				}

				ImGui::EndChild();
			}
		}

		if (open_password_prompt)
			ImGui::OpenPopup("password-prompt");

		if (ImGui::BeginPopupModal("password-prompt", NULL,
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove
		))
		{
			ImGuiInputTextFlags flags = 0;
			flags |= ImGuiInputTextFlags_EnterReturnsTrue;
			if (!password_prompt.show_password)
				flags |= ImGuiInputTextFlags_Password;

			bool connect = false;

			bool close = false;

			ImGui::Text("ssid: %s", password_prompt.network.ssid.c_str());

			if (ImGui::InputText("##", password_prompt.password, sizeof(password_prompt.password), flags))
				connect = true;

			ImGui::SameLine();
			if (ImGui::Button("Show"))
				password_prompt.show_password = !password_prompt.show_password;

			if (ImGui::Button("Connect"))
				connect = true;

			if (connect && password_prompt.password[0] != '\0')
			{
				if (wireless_manager->Connect(password_prompt.network, password_prompt.password))
					close = true;
				password_prompt.password[0] = '\0';
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				close = true;

			if (close)
			{
				password_prompt.active = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::End();

		ImGui::Render();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	delete wireless_manager;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
