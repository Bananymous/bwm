workspace "bwm"
	configurations { "Debug", "Release" }

project "imgui"
	kind "StaticLib"
	language "C++"

	files {
		"vendor/imgui/imconfig.h",
		"vendor/imgui/imgui.cpp",
		"vendor/imgui/imgui.h",
		"vendor/imgui/imgui_demo.cpp",
		"vendor/imgui/imgui_draw.cpp",
		"vendor/imgui/imgui_internal.h",
		"vendor/imgui/imgui_tables.cpp",
		"vendor/imgui/imgui_widgets.cpp",
		"vendor/imgui/imstb_rectpack.h",
		"vendor/imgui/imstb_textedit.h",
		"vendor/imgui/imstb_truetype.h"
	}

	includedirs "vendor/imgui"

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"

project "glfw"
	kind "StaticLib"
	language "C"

	files {
		"vendor/glfw/include/GLFW/glfw3.h",
		"vendor/glfw/include/GLFW/glfw3native.h",
		"vendor/glfw/src/glfw_config.h",
		"vendor/glfw/src/context.c",
		"vendor/glfw/src/init.c",
		"vendor/glfw/src/input.c",
		"vendor/glfw/src/monitor.c",
		"vendor/glfw/src/vulkan.c",
		"vendor/glfw/src/window.c",
		"vendor/glfw/src/x11_init.c",
		"vendor/glfw/src/x11_monitor.c",
		"vendor/glfw/src/x11_window.c",
		"vendor/glfw/src/xkb_unicode.c",
		"vendor/glfw/src/posix_time.c",
		"vendor/glfw/src/posix_thread.c",
		"vendor/glfw/src/glx_context.c",
		"vendor/glfw/src/egl_context.c",
		"vendor/glfw/src/osmesa_context.c",
		"vendor/glfw/src/linux_joystick.c"
	}

	defines "_GLFW_X11"

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"

project "bwm"
	kind "ConsoleApp"
	language "C++"
	targetdir "bin/%{cfg.buildcfg}"
	warnings "Extra"

	files {
		"src/bwm.cpp",
		"src/config.cpp",
		"src/imgui_build.cpp",
		"src/iwd_wireless_manager.cpp",
		"src/iwd_wrapper.cpp",
		"src/wireless_manager.cpp",
	}

	includedirs {
		"vendor/glfw/include",
		"vendor/imgui",
		"src"
	}

	links {
		"imgui",
		"glfw",
		"GL",
		"X11"
	}

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"