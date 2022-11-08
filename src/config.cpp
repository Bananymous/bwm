#include "config.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include <cstdio>
#include <pwd.h>
#include <unistd.h>

template<typename... Args>
void print_config_error(FILE* fp, int line, const char* fmt, Args&&... args)
{
	fclose(fp);
	fprintf(stderr, "Error on config (line %d)\n  ", line);
	fprintf(stderr, fmt, args...);
	fprintf(stderr, "\n");
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

static std::string get_font_path(const std::string& font_name)
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

bool ParseConfig()
{
	char buffer[1024];

	passwd* pwd = getpwuid(getuid());
	snprintf(buffer, sizeof(buffer), "%s/.config/bwm/config", pwd->pw_dir);

	FILE* fp = fopen(buffer, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Could not open config file\n");
		return true;
	}

	ImGuiIO&	io		= ImGui::GetIO();
	ImGuiStyle&	style	= ImGui::GetStyle();

	std::unordered_map<std::string, ImGuiCol> color_words;
	color_words["background"]			= ImGuiCol_WindowBg;
	color_words["border"]				= ImGuiCol_Border;
	color_words["button"]				= ImGuiCol_Button;
	color_words["button_active"]		= ImGuiCol_ButtonActive;
	color_words["button_hover"]			= ImGuiCol_ButtonHovered;
	color_words["dropdown"]				= ImGuiCol_FrameBg;
	color_words["dropdown_hover"]		= ImGuiCol_FrameBgHovered;
	color_words["selectable"]			= ImGuiCol_Header;
	color_words["selectable_active"]	= ImGuiCol_HeaderActive;
	color_words["selectable_hover"]		= ImGuiCol_HeaderHovered;
	color_words["popup_background"]		= ImGuiCol_PopupBg;
	color_words["popup_shadow"]			= ImGuiCol_ModalWindowDimBg;
	color_words["table_header"]			= ImGuiCol_TableHeaderBg;
	color_words["table_row"]			= ImGuiCol_TableRowBg;
	color_words["table_row_alt"]		= ImGuiCol_TableRowBgAlt;
	color_words["table_border_inner"]	= ImGuiCol_TableBorderLight;
	color_words["table_border_outer"]	= ImGuiCol_TableBorderStrong;

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
			{
				print_config_error(fp, line, "usage: %s <color>", splitted[0].c_str());
				return false;
			}
			
			ImVec4 color;
			if (!hexstr_to_color(splitted[1], color))
			{
				print_config_error(fp, line, "specify color as hex string '#xxxxxx' or '#xxxxxxxx'");
				return false;
			}
			
			style.Colors[color_words[splitted[0]]] = color;
		}
		else if (splitted.front() == "font")
		{
			if (splitted.size() < 3)
			{
				print_config_error(fp, line, "usage: font <font name>, <size>");
				return false;
			}

			if (splitted[splitted.size() - 2].back() != ',')
			{
				print_config_error(fp, line, "usage: font <font name>, <size>");
				return false;
			}

			std::string font = splitted[1];
			for (size_t i = 2; i < splitted.size() - 1; i++)
				font += ' ' + splitted[i];
			font.pop_back();

			std::string file = get_font_path(font);
			if (file.empty())
			{
				print_config_error(fp, line, "could not find font '%s'", font.c_str());
				return false;
			}

			float size;

			try
			{
				size = std::stof(splitted.back());
			}
			catch(const std::exception& e)
			{
				print_config_error(fp, line, "font size not a number");
				return false;
			}
			

			io.Fonts->AddFontFromFileTTF(file.c_str(), size);
		}
		else
		{
			print_config_error(fp, line, "unknown keyword '%s'", splitted[0].c_str());
			return false;
		}


	}

	fclose(fp);

	return true;
}