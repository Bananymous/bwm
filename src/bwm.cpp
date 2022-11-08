#include "wireless_manager.h"
#include "login_screen.h"
#include "config.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <chrono>

int		g_argc;
char**	g_argv;
char**	g_env;

int WINDOW_WIDTH = 400;
int WINDOW_HEIGHT = 400;

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

static void frame_start(GLFWwindow* window)
{
	glfwPollEvents();

	// Create new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	glfwGetWindowSize(window, &WINDOW_WIDTH, &WINDOW_HEIGHT);

	// Set ImGui window to fill whole application window
	ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));

	ImGui::Begin("Window", NULL,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove
	);
}

static void frame_end(GLFWwindow* window)
{
	ImGui::End();

	ImGui::Render();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(window);
}



static void get_and_echo_password(GLFWwindow* window)
{
	char password[128] {};
	bool hide_password = true;

	while (!glfwWindowShouldClose(window))
	{
		frame_start(window);

		ImGui::Text("%s", g_argv[1]);

		bool done = false;
		
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
		if (hide_password)
			flags |= ImGuiInputTextFlags_Password;

		done |= ImGui::InputText("##", password, sizeof(password), flags);

		ImGui::SameLine();
		if (ImGui::Button("Show"))
			hide_password = !hide_password;

		done |= ImGui::Button("Enter");

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			break;

		if (done)
		{
			std::printf("%s", password);
			break;
		}

		frame_end(window);
	}

	cleanup(window);
}



static void known_networks_popup(WirelessManager* wireless_manager)
{
	if (!ImGui::BeginPopupModal("known-networks", NULL,
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove
	))
	{
		return;
	}

	const auto& known_networks = wireless_manager->GetKnownNetworks();

	if (ImGui::BeginTable("known-networks", 2))
	{
		ImVec2 known_button_size = ImGui::CalcTextSize("Forget");
		known_button_size.x += 10.0f;
		known_button_size.y += 10.0f;

		ImGui::TableSetupColumn("##", ImGuiTableColumnFlags_WidthFixed, WINDOW_WIDTH - 2 * known_button_size.x);
		ImGui::TableSetupColumn("##", ImGuiTableColumnFlags_WidthFixed, known_button_size.x);

		std::size_t count = known_networks.size();
		for (std::size_t i = 0; i < count; i++)
		{
			const Network& network = known_networks[i];

			ImGui::TableNextColumn();
			ImGui::Text("%s", network.ssid.c_str());

			ImGui::TableNextColumn();
			ImGui::PushID(i + 1000);
			if (ImGui::Button("Forget", known_button_size))
			{
				wireless_manager->ForgetKnownNetwork(network);
				i = count;
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (ImGui::Button("Close"))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

int main(int argc, char** argv, char** env)
{
	using namespace std::chrono_literals;
	using clock = std::chrono::steady_clock;

	bool password_mode = (argc == 2 && strncmp(argv[1], "[sudo]", 6) == 0);

	g_argc = argc;
	g_argv = argv;
	g_env = env;

	if (password_mode)
	{
		WINDOW_HEIGHT = 100;
		WINDOW_WIDTH = 250;
	}
	else if (argc != 1)
	{
		for (int i = 0; i < argc; i++)
			fprintf(stderr, "%s\n", argv[i]);
		fprintf(stderr, "bwm does not support commandline arguments\n");
		return EXIT_FAILURE;
	}

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

	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "bwm", NULL, NULL);
	if (window == NULL)
	{
		fprintf(stderr, "Could not create window\n");
		glfwTerminate();
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

	if (password_mode)
	{
		get_and_echo_password(window);
		return 0;
	}

	WirelessManager* wireless_manager = WirelessManager::Create(WirelessBackend::iwd);
	if (!wireless_manager)
	{
		fprintf(stderr, "Could not initialize wireless backend\n");
		return EXIT_FAILURE;
	}

	wireless_manager->Scan();
	wireless_manager->UpdateNetworks();

	LoginScreen* login_screen = nullptr;

	auto next_scan		= clock::now() + 10s;
	auto next_update	= clock::now() + 2s;

	while (!glfwWindowShouldClose(window))
	{
		frame_start(window);

		if (wireless_manager->GetCurrentDevice().powered == "on")
		{
			// Scan and update networks on specified intervals
			auto current_time = clock::now();
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

		known_networks_popup(wireless_manager);

		ImGui::Spacing();
		ImGui::Spacing();

		if (wireless_manager->GetCurrentDevice().powered != "on")
		{
			if (ImGui::Button("Activate device"))
			{
				if (wireless_manager->ActivateDevice())
				{
					next_scan	= clock::now();
					next_update = clock::now();
				}
			}
		}
		else if (ImGui::BeginTable("networks", 3))
		{
			const ImVec2 button_size = ImVec2(ImGui::CalcTextSize("Disconnect").x + 10.0f, 0.0f);

			ImGui::TableSetupColumn("ssid");
			ImGui::TableSetupColumn("security", ImGuiTableColumnFlags_WidthFixed, -1);
			ImGui::TableSetupColumn("",			ImGuiTableColumnFlags_WidthFixed, -1);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < wireless_manager->GetNetworks().size(); i++)
			{
				const Network& network = wireless_manager->GetNetworks()[i];

				ImGui::TableNextColumn();
				ImGui::Text("%s", network.ssid.c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%s", network.security.c_str());

				ImGui::TableNextColumn();
				if (network.connected)
				{
					ImGui::PushID(i + 1);
					if (ImGui::Button("Disconnect", button_size))
						wireless_manager->Disconnect();
					ImGui::PopID();
				}
				else
				{
					ImGui::PushID(i + 1);
					if (ImGui::Button("Connect", button_size))
					{						
						if (!wireless_manager->Connect(network))
							login_screen = LoginScreen::Create(wireless_manager, network);
					}
					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}

		if (login_screen)
		{
			login_screen->Show();
			if (login_screen->Done())
			{
				delete login_screen;
				login_screen = nullptr;
			}
		}
		
		frame_end(window);
	}

	if (login_screen)
		delete login_screen;

	delete wireless_manager;

	cleanup(window);

	return EXIT_SUCCESS;
}
