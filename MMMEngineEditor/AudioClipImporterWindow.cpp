#define NOMINMAX
#include "AudioClipImporterWindow.h"
#include <windows.h>
#include <Commdlg.h>

#include "AudioClipFileFormat.h"
#include "EditorRegistry.h"
#include "ProjectManager.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "json/json.hpp"

using namespace MMMEngine;
using namespace MMMEngine::Editor;
using namespace MMMEngine::EditorRegistry;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	struct AudioClipImporterState
	{
		bool initialized = false;
		char inputPath[MAX_PATH] = "";
		char sourcePath[MAX_PATH] = "";
		char outputPath[MAX_PATH] = "Assets/";
		int loadMode = 0; // 0: Sample, 1: Stream, 2: StreamNonBlocking
		char statusMsg[256] = "";
		std::vector<uint8_t> embeddedData;
	};

	AudioClipImporterState& State()
	{
		static AudioClipImporterState state;
		return state;
	}

	std::string ToLowerCopy(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	std::string GetExecutablePath()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		fs::path exePath(buffer);
		return exePath.parent_path().string();
	}

	bool IsAudioFileExtension(const std::string& extLower)
	{
		return extLower == ".wav" || extLower == ".mp3" || extLower == ".ogg" ||
			extLower == ".flac" || extLower == ".aiff" || extLower == ".aif" ||
			extLower == ".aac" || extLower == ".m4a";
	}

	bool IsAudioClipExtension(const std::string& extLower)
	{
		return extLower == ".audioclip";
	}

	const char* LoadModeLabel(int mode)
	{
		switch (mode)
		{
		case 1: return "Stream";
		case 2: return "StreamNonBlocking";
		default: return "Sample";
		}
	}

	int LoadModeFromString(const std::string& raw)
	{
		std::string lower = ToLowerCopy(raw);
		if (lower == "streamnonblocking" || lower == "stream_nonblocking" || lower == "stream+nonblocking")
			return 2;
		if (lower == "stream")
			return 1;
		return 0;
	}

	int ClampLoadMode(int mode)
	{
		if (mode < 0)
			return 0;
		if (mode > 2)
			return 2;
		return mode;
	}

	const char* EmbeddedLabel()
	{
		return "<embedded>";
	}

	bool IsEmbeddedLabel(const char* text)
	{
		return text != nullptr && std::strcmp(text, EmbeddedLabel()) == 0;
	}

	enum class EmbeddedClipStatus
	{
		NotEmbedded,
		Loaded,
		Error
	};

	EmbeddedClipStatus LoadEmbeddedAudioClip(const fs::path& clipPath, std::vector<uint8_t>& outData, int& outMode, std::string& outError)
	{
		outData.clear();
		outMode = 0;
		outError.clear();

		std::ifstream file(clipPath, std::ios::binary);
		if (!file.is_open())
		{
			outError = "AudioClip 파일을 열 수 없습니다.";
			return EmbeddedClipStatus::Error;
		}

		MMMEngine::AudioClipFileFormat::Header header{};
		file.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (!file.good())
		{
			outError = "AudioClip 헤더 읽기 실패.";
			return EmbeddedClipStatus::Error;
		}

		if (!MMMEngine::AudioClipFileFormat::HasMagic(header))
			return EmbeddedClipStatus::NotEmbedded;

		if (header.version != MMMEngine::AudioClipFileFormat::kVersion)
		{
			outError = "AudioClip 버전이 맞지 않습니다.";
			return EmbeddedClipStatus::Error;
		}

		if (header.dataSize == 0)
		{
			outError = "AudioClip 오디오 데이터가 비어있습니다.";
			return EmbeddedClipStatus::Error;
		}

		outData.resize(header.dataSize);
		file.read(reinterpret_cast<char*>(outData.data()), header.dataSize);
		if (!file.good())
		{
			outData.clear();
			outError = "AudioClip 오디오 데이터 읽기 실패.";
			return EmbeddedClipStatus::Error;
		}

		outMode = ClampLoadMode(static_cast<int>(header.loadMode));
		return EmbeddedClipStatus::Loaded;
	}

	bool ReadBinaryFile(const fs::path& path, std::vector<uint8_t>& outData, std::string& outError)
	{
		outData.clear();
		outError.clear();

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			outError = "파일을 열 수 없습니다.";
			return false;
		}

		std::streamsize size = file.tellg();
		if (size <= 0)
		{
			outError = "파일이 비어있습니다.";
			return false;
		}

		file.seekg(0, std::ios::beg);
		outData.resize(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(outData.data()), size))
		{
			outError = "파일 읽기 실패.";
			outData.clear();
			return false;
		}

		return true;
	}

	std::string ToProjectRelativeOrAbsolute(const fs::path& path)
	{
		if (!ProjectManager::Get().HasActiveProject())
			return path.string();

		return ProjectManager::Get().ToProjectRelativePath(path.string());
	}

	std::string NormalizePathString(const std::string& pathStr)
	{
		fs::path p = fs::path(pathStr).lexically_normal();
		return p.string();
	}

	void SetStatus(AudioClipImporterState& state, const std::string& msg)
	{
		strncpy_s(state.statusMsg, msg.c_str(), sizeof(state.statusMsg) - 1);
		state.statusMsg[sizeof(state.statusMsg) - 1] = '\0';
	}

	std::string OpenAudioOrClipFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Audio/AudioClip (*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a;*.audioclip)\0"
			"*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a;*.audioclip\0"
			"Audio Files (*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a)\0"
			"*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a\0"
			"AudioClip (*.audioclip)\0*.audioclip\0"
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

	std::string OpenAudioFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Audio Files (*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a)\0"
			"*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif;*.aac;*.m4a\0"
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

	bool LoadLegacyAudioClipFile(const fs::path& clipPath, std::string& outSource, int& outMode, std::string& outError)
	{
		outSource.clear();
		outError.clear();
		outMode = 0;

		std::ifstream file(clipPath);
		if (!file.is_open())
		{
			outError = u8"AudioClip 파일을 열 수 없습니다.";
			return false;
		}

		json data = json::parse(file, nullptr, false);
		if (data.is_discarded() || !data.is_object())
		{
			outError = u8"AudioClip JSON 파싱 실패.";
			return false;
		}

		if (!data.contains("source") || !data["source"].is_string())
		{
			outError = u8"AudioClip에 source 정보가 없습니다.";
			return false;
		}

		outSource = data["source"].get<std::string>();
		std::string modeStr = data.value("loadMode", "Sample");
		outMode = ClampLoadMode(LoadModeFromString(modeStr));
		return true;
	}

	void ApplyInputPath(AudioClipImporterState& state, const std::string& pathStr)
	{
		state.statusMsg[0] = '\0';
		if (pathStr.empty())
			return;

		std::string normalized = NormalizePathString(pathStr);
		fs::path path(normalized);
		std::string extLower = ToLowerCopy(path.extension().string());

		std::string displayPath = ToProjectRelativeOrAbsolute(path);
		strncpy_s(state.inputPath, displayPath.c_str(), sizeof(state.inputPath) - 1);
		state.inputPath[sizeof(state.inputPath) - 1] = '\0';

		if (IsAudioClipExtension(extLower))
		{
			state.embeddedData.clear();
			std::vector<uint8_t> embedded;
			int mode = 0;
			std::string error;
			EmbeddedClipStatus status = LoadEmbeddedAudioClip(path, embedded, mode, error);
			if (status == EmbeddedClipStatus::Loaded)
			{
				state.embeddedData = std::move(embedded);
				state.loadMode = mode;
				strncpy_s(state.sourcePath, EmbeddedLabel(), sizeof(state.sourcePath) - 1);
				state.sourcePath[sizeof(state.sourcePath) - 1] = '\0';

				std::string outputPath = ToProjectRelativeOrAbsolute(path);
				strncpy_s(state.outputPath, outputPath.c_str(), sizeof(state.outputPath) - 1);
				state.outputPath[sizeof(state.outputPath) - 1] = '\0';
				return;
			}
			if (status == EmbeddedClipStatus::Error)
			{
				SetStatus(state, error);
				return;
			}

			std::string source;
			if (!LoadLegacyAudioClipFile(path, source, mode, error))
			{
				SetStatus(state, error.empty() ? u8"AudioClip 파일을 읽을 수 없습니다." : error);
				return;
			}

			state.embeddedData.clear();
			strncpy_s(state.sourcePath, source.c_str(), sizeof(state.sourcePath) - 1);
			state.sourcePath[sizeof(state.sourcePath) - 1] = '\0';
			state.loadMode = mode;

			std::string outputPath = ToProjectRelativeOrAbsolute(path);
			strncpy_s(state.outputPath, outputPath.c_str(), sizeof(state.outputPath) - 1);
			state.outputPath[sizeof(state.outputPath) - 1] = '\0';
		}
		else if (IsAudioFileExtension(extLower))
		{
			state.embeddedData.clear();
			std::string sourcePath = ToProjectRelativeOrAbsolute(path);
			strncpy_s(state.sourcePath, sourcePath.c_str(), sizeof(state.sourcePath) - 1);
			state.sourcePath[sizeof(state.sourcePath) - 1] = '\0';

			fs::path outPath = path;
			outPath.replace_extension(".audioclip");
			std::string outputPath = ToProjectRelativeOrAbsolute(outPath);
			strncpy_s(state.outputPath, outputPath.c_str(), sizeof(state.outputPath) - 1);
			state.outputPath[sizeof(state.outputPath) - 1] = '\0';
		}
		else
		{
			state.embeddedData.clear();
			SetStatus(state, u8"지원하지 않는 파일 형식입니다.");
		}
	}

	bool ResolveAbsolutePath(const std::string& pathStr, fs::path& outAbs, std::string& outError)
	{
		outError.clear();
		fs::path path = fs::path(pathStr).lexically_normal();
		if (path.empty())
		{
			outError = u8"경로가 비어있습니다.";
			return false;
		}

		if (path.is_relative())
		{
			if (!ProjectManager::Get().HasActiveProject())
			{
				outError = u8"프로젝트가 열려있지 않아 상대경로를 처리할 수 없습니다.";
				return false;
			}
			outAbs = ProjectManager::Get().GetActiveProject().ProjectRootFS() / path;
		}
		else
		{
			outAbs = path;
		}
		return true;
	}

	fs::path EnsureAudioClipExtension(const fs::path& path, const fs::path& nameSource)
	{
		fs::path out = path;
		if (out.has_extension())
		{
			out.replace_extension(".audioclip");
			return out;
		}

		if (fs::exists(out) && fs::is_directory(out))
		{
			fs::path stemSource = nameSource;
			if (stemSource.empty())
				stemSource = fs::path("AudioClip");
			out /= stemSource.stem();
		}
		else if (out.filename().empty())
		{
			fs::path stemSource = nameSource;
			if (stemSource.empty())
				stemSource = fs::path("AudioClip");
			out /= stemSource.stem();
		}

		out += ".audioclip";
		return out;
	}

}

void MMMEngine::Editor::AudioClipImporterWindow::OpenWithPath(const fs::path& path)
{
	auto& state = State();
	ApplyInputPath(state, path.string());
	g_editor_window_audioClipImporter = true;
}

void MMMEngine::Editor::AudioClipImporterWindow::Render()
{
	if (!g_editor_window_audioClipImporter)
		return;

	ImGuiWindowClass wc;
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowClass(&wc);

	auto& state = State();

	if (ImGui::Begin(u8"AudioClip 임포터", &g_editor_window_audioClipImporter, ImGuiWindowFlags_NoDocking))
	{
		if (!state.initialized)
		{
			state.initialized = true;
		}

		ImGui::Text(u8"입력 파일 (음원 또는 .audioclip)");
		ImGui::InputText("##audio_import_path", state.inputPath, sizeof(state.inputPath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기##audio_import"))
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

			std::string selected = OpenAudioOrClipFileDialog(initialDir);
			if (!selected.empty())
			{
				ApplyInputPath(state, selected);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"적용##audio_import"))
		{
			ApplyInputPath(state, state.inputPath);
		}

		ImGui::Spacing();
		ImGui::Text(u8"원본 음원 파일");
		ImGui::InputText("##audio_source_path", state.sourcePath, sizeof(state.sourcePath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기##audio_source"))
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

			std::string selected = OpenAudioFileDialog(initialDir);
			if (!selected.empty())
			{
				state.embeddedData.clear();
				std::string display = ToProjectRelativeOrAbsolute(fs::path(selected));
				strncpy_s(state.sourcePath, display.c_str(), sizeof(state.sourcePath) - 1);
				state.sourcePath[sizeof(state.sourcePath) - 1] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::Text(u8"내보내기 경로 (프로젝트 기준)");
		ImGui::InputText("##audio_output_path", state.outputPath, sizeof(state.outputPath));

		ImGui::Spacing();
		ImGui::Text(u8"Load Mode");
		const char* modeLabels[] = { "Sample", "Stream", "StreamNonBlocking" };
		if (ImGui::BeginCombo("##audio_loadmode", modeLabels[state.loadMode]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool selected = (state.loadMode == i);
				if (ImGui::Selectable(modeLabels[i], selected))
				{
					state.loadMode = i;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		if (ImGui::Button(u8"변환"))
		{
			state.statusMsg[0] = '\0';

			std::string error;
			std::vector<uint8_t> audioData;

			bool useEmbedded = !state.embeddedData.empty() &&
				(state.sourcePath[0] == '\0' || IsEmbeddedLabel(state.sourcePath));

			fs::path sourceAbs;
			if (!useEmbedded)
			{
				if (!ResolveAbsolutePath(state.sourcePath, sourceAbs, error))
				{
					SetStatus(state, error);
					goto end_render;
				}

				std::string sourceExtLower = ToLowerCopy(sourceAbs.extension().string());
				if (!IsAudioFileExtension(sourceExtLower))
				{
					SetStatus(state, u8"원본 음원 파일 확장자가 유효하지 않습니다.");
					goto end_render;
				}

				if (!fs::exists(sourceAbs))
				{
					SetStatus(state, u8"원본 음원 파일을 찾을 수 없습니다.");
					goto end_render;
				}

				if (!ReadBinaryFile(sourceAbs, audioData, error))
				{
					SetStatus(state, error);
					goto end_render;
				}
			}
			else
			{
				audioData = state.embeddedData;
			}

			if (audioData.empty())
			{
				SetStatus(state, u8"오디오 데이터가 비어있습니다.");
				goto end_render;
			}

			if (audioData.size() > std::numeric_limits<uint32_t>::max())
			{
				SetStatus(state, u8"오디오 데이터가 너무 큽니다.");
				goto end_render;
			}

			fs::path outputAbs;
			if (!ResolveAbsolutePath(state.outputPath, outputAbs, error))
			{
				SetStatus(state, error);
				goto end_render;
			}

			fs::path nameSource = sourceAbs;
			if (nameSource.empty())
			{
				fs::path inputAbs;
				std::string inputError;
				if (ResolveAbsolutePath(state.inputPath, inputAbs, inputError))
					nameSource = inputAbs;
			}

			outputAbs = EnsureAudioClipExtension(outputAbs, nameSource);

			if (outputAbs.has_parent_path())
			{
				std::error_code ec;
				fs::create_directories(outputAbs.parent_path(), ec);
			}

			MMMEngine::AudioClipFileFormat::Header header{};
			std::memcpy(header.magic, MMMEngine::AudioClipFileFormat::kMagic, sizeof(header.magic));
			header.version = MMMEngine::AudioClipFileFormat::kVersion;
			header.loadMode = static_cast<uint32_t>(ClampLoadMode(state.loadMode));
			header.dataSize = static_cast<uint32_t>(audioData.size());

			std::ofstream outFile(outputAbs, std::ios::binary);
			if (!outFile.is_open())
			{
				SetStatus(state, u8"AudioClip 파일 저장에 실패했습니다.");
				goto end_render;
			}

			outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
			outFile.write(reinterpret_cast<const char*>(audioData.data()), static_cast<std::streamsize>(audioData.size()));
			outFile.close();

			state.embeddedData = std::move(audioData);
			strncpy_s(state.sourcePath, EmbeddedLabel(), sizeof(state.sourcePath) - 1);
			state.sourcePath[sizeof(state.sourcePath) - 1] = '\0';

			std::string outputDisplay = ToProjectRelativeOrAbsolute(outputAbs);
			strncpy_s(state.outputPath, outputDisplay.c_str(), sizeof(state.outputPath) - 1);
			state.outputPath[sizeof(state.outputPath) - 1] = '\0';

			SetStatus(state, u8"변환 완료");
		}

	end_render:
		if (state.statusMsg[0] != '\0')
		{
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.45f, 1.0f));
			ImGui::TextWrapped("%s", state.statusMsg);
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
}
