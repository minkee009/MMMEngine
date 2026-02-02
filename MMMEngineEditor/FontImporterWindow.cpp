#define NOMINMAX
#include "FontImporterWindow.h"

#include "FontImporter.h"
#include "EditorRegistry.h"
#include "ProjectManager.h"
#include "StringHelper.h"

#include <imgui.h>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

using namespace MMMEngine;
using namespace MMMEngine::Editor;
using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine::Utility;

namespace
{
	std::string GetExecutablePath()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::filesystem::path exePath(buffer);
		return exePath.parent_path().string();
	}

	std::string TryFindMakeSpriteFont()
	{
		const wchar_t* engineDir = _wgetenv(L"MMMENGINE_DIR");
		if (!engineDir || engineDir[0] == L'\0')
			return {};

		std::filesystem::path candidate = std::filesystem::path(engineDir) / "EditorThirdParty" / "MakeSpriteFont.exe";
		if (std::filesystem::exists(candidate))
			return candidate.string();

		return {};
	}

	std::string OpenFontFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Font Files (*.ttf;*.otf)\0*.ttf;*.otf\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return std::string(ofn.lpstrFile);
		}

		return {};
	}

	std::string OpenExeFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Executable (*.exe)\0*.exe\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return std::string(ofn.lpstrFile);
		}

		return {};
	}

	std::string OpenCharsetFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Text Files (*.txt)\0*.txt\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return std::string(ofn.lpstrFile);
		}

		return {};
	}

	bool LoadFileUtf8(const std::filesystem::path& path, std::string& outData, std::wstring& outError)
	{
		outData.clear();
		outError.clear();

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			outError = L"charset 파일을 열 수 없습니다.";
			return false;
		}

		std::ostringstream oss;
		oss << file.rdbuf();
		outData = oss.str();
		if (outData.empty())
		{
			outError = L"charset 파일이 비어있습니다.";
			return false;
		}

		return true;
	}

	bool BuildCharacterRegionFromCharsetFile(const std::filesystem::path& path, std::wstring& outRegion, std::wstring& outError)
	{
		outRegion.clear();
		outError.clear();

		if (!std::filesystem::exists(path))
		{
			outError = L"charset 파일을 찾을 수 없습니다.";
			return false;
		}

		std::string bytes;
		if (!LoadFileUtf8(path, bytes, outError))
			return false;

		std::wstring text = StringHelper::StringToWString(bytes);
		if (text.empty())
		{
			outError = L"charset 파일을 UTF-8로 읽지 못했습니다.";
			return false;
		}

		std::vector<uint32_t> codes;
		codes.reserve(text.size() + 1);

		for (size_t i = 0; i < text.size();)
		{
			wchar_t ch = text[i];
			if (ch == 0xFEFF) // UTF-8 BOM
			{
				++i;
				continue;
			}

			uint32_t cp = 0;
			if (ch >= 0xD800 && ch <= 0xDBFF)
			{
				if (i + 1 < text.size())
				{
					wchar_t lo = text[i + 1];
					if (lo >= 0xDC00 && lo <= 0xDFFF)
					{
						cp = 0x10000 + (((static_cast<uint32_t>(ch) - 0xD800) << 10) | (static_cast<uint32_t>(lo) - 0xDC00));
						i += 2;
					}
					else
					{
						++i;
						continue;
					}
				}
				else
				{
					++i;
					continue;
				}
			}
			else
			{
				cp = static_cast<uint32_t>(ch);
				++i;
			}

			if (cp == '\r' || cp == '\n' || cp == '\t')
				continue;
			if (cp < 0x20 && cp != 0x20)
				continue;

			codes.push_back(cp);
		}

		// Ensure space is available for common text rendering.
		codes.push_back(0x20);

		std::sort(codes.begin(), codes.end());
		codes.erase(std::unique(codes.begin(), codes.end()), codes.end());

		if (codes.empty())
		{
			outError = L"charset 파일에 유효한 문자가 없습니다.";
			return false;
		}

		std::wstringstream ss;
		bool first = true;
		size_t idx = 0;
		while (idx < codes.size())
		{
			uint32_t start = codes[idx];
			uint32_t end = start;
			++idx;
			while (idx < codes.size() && codes[idx] == end + 1)
			{
				end = codes[idx];
				++idx;
			}

			if (!first)
				ss << L" /CharacterRegion:";
			ss << L"0x" << std::uppercase << std::hex << start << L"-0x" << end;
			first = false;
		}

		outRegion = ss.str();
		return true;
	}

	bool ContainsCharacterRegionToken(const std::wstring& text)
	{
		if (text.empty())
			return false;

		std::wstring lower = text;
		for (auto& ch : lower)
		{
			if (ch >= L'A' && ch <= L'Z')
				ch = static_cast<wchar_t>(ch - L'A' + L'a');
		}

		return lower.find(L"/characterregion") != std::wstring::npos;
	}
}

void MMMEngine::Editor::FontImporterWindow::Render()
{
	if (!g_editor_window_fontImporter)
		return;

	ImGuiWindowClass wc;
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowClass(&wc);

	if (ImGui::Begin(u8"Font 임포터", &g_editor_window_fontImporter, ImGuiWindowFlags_NoDocking))
	{
		static bool initialized = false;
		static char importPath[MAX_PATH] = "";
		static char exportPath[256] = "Assets/";
		static char toolPath[MAX_PATH] = "";
		static char charsetPath[MAX_PATH] = "";
		static int fontSize = 32;
		static char characterRegion[128] = "";
		static char statusMsg[2048] = "";

		auto& importer = FontImporter::Get();

		if (!initialized)
		{
			std::string exportPathStr = StringHelper::WStringToString(importer.m_exportPath);
			if (!exportPathStr.empty())
			{
				strncpy_s(exportPath, exportPathStr.c_str(), sizeof(exportPath) - 1);
				exportPath[sizeof(exportPath) - 1] = '\0';
			}

			std::string toolPathStr = StringHelper::WStringToString(importer.m_makeSpriteFontPath);
			if (!toolPathStr.empty())
			{
				strncpy_s(toolPath, toolPathStr.c_str(), sizeof(toolPath) - 1);
				toolPath[sizeof(toolPath) - 1] = '\0';
			}
			else
			{
				std::string found = TryFindMakeSpriteFont();
				if (!found.empty())
				{
					strncpy_s(toolPath, found.c_str(), sizeof(toolPath) - 1);
					toolPath[sizeof(toolPath) - 1] = '\0';
					importer.m_makeSpriteFontPath = StringHelper::StringToWString(found);
				}
			}

			std::string charsetPathStr = StringHelper::WStringToString(importer.m_charsetFilePath);
			if (!charsetPathStr.empty())
			{
				strncpy_s(charsetPath, charsetPathStr.c_str(), sizeof(charsetPath) - 1);
				charsetPath[sizeof(charsetPath) - 1] = '\0';
			}

			initialized = true;
		}

		ImGui::Text(u8"MakeSpriteFont.exe");
		ImGui::InputText("##font_tool_path", toolPath, sizeof(toolPath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기##font_tool"))
		{
			std::string initialDir = GetExecutablePath();
			std::string selected = OpenExeFileDialog(initialDir);
			if (!selected.empty())
			{
				strncpy_s(toolPath, selected.c_str(), sizeof(toolPath) - 1);
				toolPath[sizeof(toolPath) - 1] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::Text(u8"폰트 이름 또는 TTF/OTF 파일");
		ImGui::InputText("##font_import_path", importPath, sizeof(importPath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기##font_import"))
		{
			std::string initialDir;
			if (ProjectManager::Get().HasActiveProject())
			{
				auto root = ProjectManager::Get().GetActiveProject().ProjectRootFS();
				initialDir = root.string();
			}
			else
			{
				initialDir = GetExecutablePath();
			}

			std::string selected = OpenFontFileDialog(initialDir);
			if (!selected.empty())
			{
				strncpy_s(importPath, selected.c_str(), sizeof(importPath) - 1);
				importPath[sizeof(importPath) - 1] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::Text(u8"내보내기 경로 (프로젝트 기준)");
		ImGui::InputText("##font_export_path", exportPath, sizeof(exportPath));

		ImGui::Spacing();
		ImGui::Text(u8"Font Size");
		ImGui::InputInt("##font_size", &fontSize);
		if (fontSize < 1)
			fontSize = 1;

		ImGui::Spacing();
		ImGui::Text(u8"CharacterRegion (옵션)");
		ImGui::InputText("##font_region", characterRegion, sizeof(characterRegion));

		ImGui::Spacing();
		ImGui::Text(u8"Charset 파일 (옵션)");
		ImGui::InputText("##font_charset_path", charsetPath, sizeof(charsetPath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기##font_charset"))
		{
			std::string initialDir;
			if (ProjectManager::Get().HasActiveProject())
			{
				auto root = ProjectManager::Get().GetActiveProject().ProjectRootFS();
				initialDir = root.string();
			}
			else
			{
				initialDir = GetExecutablePath();
			}

			std::string selected = OpenCharsetFileDialog(initialDir);
			if (!selected.empty())
			{
				strncpy_s(charsetPath, selected.c_str(), sizeof(charsetPath) - 1);
				charsetPath[sizeof(charsetPath) - 1] = '\0';
			}
		}

		ImGui::Spacing();
		if (ImGui::Button(u8"변환"))
		{
			statusMsg[0] = '\0';

			if (importPath[0] == '\0')
			{
				strncpy_s(statusMsg, u8"입력 폰트 파일을 선택해주세요.", sizeof(statusMsg) - 1);
				statusMsg[sizeof(statusMsg) - 1] = '\0';
				return;
			}
			if (toolPath[0] == '\0')
			{
				strncpy_s(statusMsg, u8"MakeSpriteFont.exe 경로를 지정해주세요.", sizeof(statusMsg) - 1);
				statusMsg[sizeof(statusMsg) - 1] = '\0';
				return;
			}

			std::filesystem::path importPathFs = std::filesystem::path(importPath).lexically_normal();
			std::filesystem::path inputAbs = importPathFs;
			if (importPathFs.is_relative())
			{
				if (!ProjectManager::Get().HasActiveProject())
				{
					strncpy_s(statusMsg, u8"프로젝트가 열려있지 않아 상대경로를 처리할 수 없습니다.", sizeof(statusMsg) - 1);
					statusMsg[sizeof(statusMsg) - 1] = '\0';
					return;
				}
				inputAbs = ProjectManager::Get().GetActiveProject().ProjectRootFS() / importPathFs;
			}

			std::filesystem::path exportPathFs = std::filesystem::path(exportPath).lexically_normal();
			std::filesystem::path outputAbs = exportPathFs;

			if (exportPathFs.is_relative())
			{
				if (!ProjectManager::Get().HasActiveProject())
				{
					strncpy_s(statusMsg, u8"프로젝트가 열려있지 않아 내보내기 경로를 처리할 수 없습니다.", sizeof(statusMsg) - 1);
					statusMsg[sizeof(statusMsg) - 1] = '\0';
					return;
				}
				outputAbs = ProjectManager::Get().GetActiveProject().ProjectRootFS() / exportPathFs;
			}

			if (!outputAbs.has_extension())
			{
				std::wstring stem = inputAbs.stem().wstring();
				outputAbs /= stem + L".spritefont";
			}

			importer.m_exportPath = StringHelper::StringToWString(exportPath);
			importer.m_makeSpriteFontPath = StringHelper::StringToWString(toolPath);
			importer.m_charsetFilePath = StringHelper::StringToWString(charsetPath);

			FontImporter::BuildOptions options;
			options.fontSize = fontSize;

			std::wstring regionFromFile;
			if (charsetPath[0] != '\0')
			{
				std::filesystem::path charsetPathFs = std::filesystem::path(charsetPath).lexically_normal();
				std::filesystem::path charsetAbs = charsetPathFs;
				if (charsetPathFs.is_relative())
				{
					if (!ProjectManager::Get().HasActiveProject())
					{
						strncpy_s(statusMsg, u8"프로젝트가 열려있지 않아 charset 경로를 처리할 수 없습니다.", sizeof(statusMsg) - 1);
						statusMsg[sizeof(statusMsg) - 1] = '\0';
						return;
					}
					charsetAbs = ProjectManager::Get().GetActiveProject().ProjectRootFS() / charsetPathFs;
				}

				std::wstring regionError;
				if (!BuildCharacterRegionFromCharsetFile(charsetAbs, regionFromFile, regionError))
				{
					std::string errUtf8 = StringHelper::WStringToString(regionError);
					strncpy_s(statusMsg, errUtf8.c_str(), sizeof(statusMsg) - 1);
					statusMsg[sizeof(statusMsg) - 1] = '\0';
					return;
				}
			}

			std::wstring manualRegion = StringHelper::StringToWString(characterRegion);
			std::wstring finalRegion = regionFromFile;
			if (!manualRegion.empty())
			{
				if (!finalRegion.empty())
				{
					if (ContainsCharacterRegionToken(manualRegion))
						finalRegion += L" " + manualRegion;
					else
						finalRegion += L" /CharacterRegion:" + manualRegion;
				}
				else
				{
					finalRegion = manualRegion;
				}
			}

			options.characterRegion = finalRegion;

			std::wstring error;
			if (!importer.ConvertTTFToSpriteFont(inputAbs.wstring(), outputAbs.wstring(), options, error))
			{
				std::string errorUtf8 = StringHelper::WStringToString(error);
				strncpy_s(statusMsg, errorUtf8.c_str(), sizeof(statusMsg) - 1);
				statusMsg[sizeof(statusMsg) - 1] = '\0';
			}
			else
			{
				strncpy_s(statusMsg, u8"변환 완료", sizeof(statusMsg) - 1);
				statusMsg[sizeof(statusMsg) - 1] = '\0';
			}
		}

		if (statusMsg[0] != '\0')
		{
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.45f, 1.0f));
			ImGui::TextWrapped("%s", statusMsg);
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
}
