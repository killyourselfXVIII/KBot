#pragma once

#include <fstream>
#include <thread>
#include <filesystem>
#include <tlhelp32.h>
#include "HTTP.h"
#include "LCU.h"
#include "Config.h"

class Misc
{
public:

	static inline std::string programVersion = "1.5.2";

	static void LaunchLegacyClient()
	{
		if (!std::filesystem::exists(std::format("{}LoL Companion", S.leaguePath)))
		{
			std::filesystem::create_directory(std::format("{}LoL Companion", S.leaguePath));
		}
		if (!std::filesystem::exists(std::format("{}LoL Companion/system.yaml", S.leaguePath)))
		{
			std::ifstream infile(std::format("{}system.yaml", S.leaguePath));
			std::ofstream outfile(std::format("{}LoL Companion/system.yaml", S.leaguePath));
			std::string content = "";

			std::string temp;
			while (std::getline(infile, temp))
				content += temp + "\n";

			infile.close();
			size_t pos = content.find("riotclient:");
			content = content.substr(0, pos + 11);

			outfile << content;
			outfile.close();
		}

		if (::FindWindowA("RCLIENT", "League of Legends"))
		{
			LCU::Request("POST", "https://127.0.0.1/process-control/v1/process/quit");

			// wait for client to close (maybe theres a better method of doing that)
			std::this_thread::sleep_for(std::chrono::milliseconds(4500));
		}

		ShellExecuteA(NULL, "open", std::format("{}LeagueClient.exe", S.leaguePath).c_str(),
			std::format("--system-yaml-override=\"{}LoL Companion/system.yaml\"", S.leaguePath).c_str(), NULL, SW_SHOWNORMAL);
	}

	static void CheckVersion()
	{
		std::string getLatest = HTTP::Request("GET", "https://api.github.com/repos/KebsCS/KBotExt/releases/latest");

		Json::CharReaderBuilder builder;
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		JSONCPP_STRING err;
		Json::Value root;
		if (reader->parse(getLatest.c_str(), getLatest.c_str() + static_cast<int>(getLatest.length()), &root, &err))
		{
			std::string latestTag = root["tag_name"].asString();

			std::vector<std::string>latestNameSplit = Utils::StringSplit(latestTag, ".");
			std::vector<std::string>programVersionSplit = Utils::StringSplit(Misc::programVersion, ".");

			for (size_t i = 0; i < 2; i++)
			{
				if (latestNameSplit[i] != programVersionSplit[i])
				{
					if (MessageBoxA(0, "Open download website?", "New major version available", MB_YESNO | MB_SETFOREGROUND) == IDYES)
					{
						ShellExecuteW(0, 0, L"https://github.com/KebsCS/KBotExt/releases/latest", 0, 0, SW_SHOW);
					}
				}
			}
			if (latestTag != Misc::programVersion
				&& std::find(S.ignoredVersions.begin(), S.ignoredVersions.end(), latestTag) == S.ignoredVersions.end())
			{
				const auto status = MessageBoxA(0, "Open download website?\nCancel to ignore this version forever", "New minor update available", MB_YESNOCANCEL | MB_SETFOREGROUND);
				if (status == IDYES)
				{
					ShellExecuteW(0, 0, L"https://github.com/KebsCS/KBotExt/releases/latest", 0, 0, SW_SHOW);
				}
				else if (status == IDCANCEL)
				{
					S.ignoredVersions.emplace_back(latestTag);
					Config::Save();
				}
			}
		}
	}

	static std::string GetCurrentPatch()
	{
		std::string result = HTTP::Request("GET", "https://ddragon.leagueoflegends.com/api/versions.json");
		Json::CharReaderBuilder builder;
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		JSONCPP_STRING err;
		Json::Value root;
		if (reader->parse(result.c_str(), result.c_str() + static_cast<int>(result.length()), &root, &err))
		{
			return root[0].asString();
		}
		return "0.0.0";
	}

	static void GetAllChampionSkins()
	{
		std::string getSkins = HTTP::Request("GET", "https://raw.communitydragon.org/latest/plugins/rcp-be-lol-game-data/global/default/v1/skins.json");
		Json::CharReaderBuilder builder;
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		JSONCPP_STRING err;
		Json::Value root;
		if (!reader->parse(getSkins.c_str(), getSkins.c_str() + static_cast<int>(getSkins.length()), &root, &err))
			return;

		std::map<std::string, Champ>champs;
		for (const std::string& id : root.getMemberNames())
		{
			const Json::Value currentSkin = root[id];

			std::string loadScreenPath = currentSkin["loadScreenPath"].asString();
			size_t nameStart = loadScreenPath.find("ASSETS/Characters/") + strlen("ASSETS/Characters/");
			std::string champName = loadScreenPath.substr(nameStart, loadScreenPath.find("/", nameStart) - nameStart);

			std::string name = currentSkin["name"].asString();

			std::pair<std::string, std::string> skin;
			if (currentSkin["isBase"].asBool() == true)
			{
				champs[champName].name = champName;

				std::string splashPath = currentSkin["splashPath"].asString();
				size_t keyStart = splashPath.find("champion-splashes/") + strlen("champion-splashes/");
				std::string champKey = splashPath.substr(keyStart, splashPath.find("/", keyStart) - keyStart);

				champs[champName].key = std::stoi(champKey);
				skin.first = id;
				skin.second = "default";
				champs[champName].skins.insert(champs[champName].skins.begin(), skin);
			}
			else
			{
				if (currentSkin["questSkinInfo"]) // K/DA ALL OUT Seraphine
				{
					const Json::Value skinTiers = currentSkin["questSkinInfo"]["tiers"];
					for (Json::Value::ArrayIndex i = 0; i < skinTiers.size(); i++)
					{
						skin.first = skinTiers[i]["id"].asString();
						skin.second = skinTiers[i]["name"].asString();
						champs[champName].skins.emplace_back(skin);
					}
				}
				else
				{
					skin.first = id;
					skin.second = name;
					champs[champName].skins.emplace_back(skin);
				}
			}
		}

		std::vector<Champ> temp;
		for (const auto& c : champs)
		{
			temp.emplace_back(c.second);
		}
		champSkins = temp;
	}

	static void TaskKillLeague()
	{
		Misc::TerminateProcessByName(L"RiotClientServices.exe");
		Misc::TerminateProcessByName(L"RiotClientCrashHandler.exe");
		Misc::TerminateProcessByName(L"RiotClientUx.exe");
		Misc::TerminateProcessByName(L"RiotClientUxRender.exe");

		Misc::TerminateProcessByName(L"LeagueClient.exe");
		Misc::TerminateProcessByName(L"LeagueCrashHandler.exe");
		Misc::TerminateProcessByName(L"LeagueClientUx.exe");
		Misc::TerminateProcessByName(L"LeagueClientUxRender.exe");
	}

	static std::string ChampIdToName(int id)
	{
		if (!id)
		{
			return "None";
		}
		else if (champSkins.empty())
		{
			return "No data";// "Champion data is still being fetched";
		}
		{
			for (const auto& c : champSkins)
			{
				if (c.key == id)
					return c.name;
			}
		}
		return "";
	}

	static std::string ClearLogs()
	{
		std::string result = "";

		TaskKillLeague();

		std::this_thread::sleep_for(std::chrono::seconds(2));

		std::error_code errorCode;

		std::string logsFolder = S.leaguePath + "Logs";
		if (std::filesystem::exists(logsFolder))
		{
			SetFileAttributesA(logsFolder.c_str(), GetFileAttributesA(logsFolder.c_str()) & ~FILE_ATTRIBUTE_READONLY & ~FILE_ATTRIBUTE_HIDDEN);
			std::filesystem::remove_all(logsFolder, errorCode);
			result += logsFolder + " - " + errorCode.message() + "\n";
		}

		std::string configFolder = S.leaguePath + "Config";
		if (std::filesystem::exists(configFolder))
		{
			SetFileAttributesA(configFolder.c_str(), GetFileAttributesA(configFolder.c_str()) & ~FILE_ATTRIBUTE_READONLY & ~FILE_ATTRIBUTE_HIDDEN);
			std::filesystem::remove_all(configFolder, errorCode);
			result += configFolder + " - " + errorCode.message() + "\n";
		}

		std::string programData = "C:/ProgramData/Riot Games";
		if (std::filesystem::exists(programData))
		{
			SetFileAttributesA(programData.c_str(), GetFileAttributesA(programData.c_str()) & ~FILE_ATTRIBUTE_READONLY & ~FILE_ATTRIBUTE_HIDDEN);
			std::filesystem::remove_all(programData, errorCode);
			result += programData + " - " + errorCode.message() + "\n";
		}

		char* pLocal;
		size_t localLen;
		_dupenv_s(&pLocal, &localLen, "LOCALAPPDATA");
		std::string local = pLocal;
		local += "\\Riot Games";
		if (std::filesystem::exists(local))
		{
			SetFileAttributesA(local.c_str(), GetFileAttributesA(local.c_str()) & ~FILE_ATTRIBUTE_READONLY & ~FILE_ATTRIBUTE_HIDDEN);
			std::filesystem::remove_all(local, errorCode);
			result += local + " - " + errorCode.message() + "\n";
		}

		int counter = 0;
		for (const auto& file : std::filesystem::directory_iterator(std::filesystem::temp_directory_path()))
		{
			std::filesystem::remove_all(file, errorCode);
			counter++;
		}
		result += "Deleted " + std::to_string(counter) + " files in temp directory\n";
		return result;
	}

	// returns true on success
	static bool TerminateProcessByName(std::wstring processName)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		bool result = false;
		if (snapshot != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32W entry;
			entry.dwSize = sizeof(PROCESSENTRY32W);
			if (Process32FirstW(snapshot, &entry))
			{
				do
				{
					if (std::wstring(entry.szExeFile) == processName)
					{
						HANDLE process = OpenProcess(PROCESS_TERMINATE, false, entry.th32ProcessID);
						bool terminate = TerminateProcess(process, 0);
						CloseHandle(process);
						result = terminate;
					}
				} while (Process32NextW(snapshot, &entry));
			}
		}
		CloseHandle(snapshot);
		return result;
	}
};

namespace ImGui
{
	// Helper to display a little (?) mark which shows a tooltip when hovered.
	// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
	static void HelpMarker(const char* desc)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	static void ArrowButtonDisabled(const char* id, ImGuiDir dir)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		ImGui::ArrowButton(id, dir);
		ImGui::PopStyleVar();
	}

	static void AddUnderLine(ImColor col)
	{
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		min.y = max.y;
		ImGui::GetWindowDrawList()->AddLine(min, max, col, 1.0f);
	}

	static void TextURL(const char* name, const char* url, uint8_t sameLineBefore = 1, uint8_t sameLineAfter = 1)
	{
		if (1 == sameLineBefore) { ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x); }
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
		ImGui::Text(name);
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
		{
			if (ImGui::IsMouseClicked(0))
			{
				ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
			}
			AddUnderLine(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
			ImGui::SetTooltip("  Open in browser\n%s", url);
		}
		else
		{
			AddUnderLine(ImGui::GetStyle().Colors[ImGuiCol_Button]);
		}
		if (1 == sameLineAfter) { ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x); }
	}
}