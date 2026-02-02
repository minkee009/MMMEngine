#include "Font.h"
#include "rttr/registration"
#include <filesystem>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Font>("Font")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property_readonly("GetFilePath", &Font::GetFilePath);

	rttr::type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<Font>
		{
			if (!from) { ok = true; return nullptr; }
			auto result = std::dynamic_pointer_cast<Font>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

bool MMMEngine::Font::LoadFromFilePath(const std::wstring& filePath)
{
	std::filesystem::path p(filePath);
	if (!std::filesystem::exists(p))
		return false;

	m_spriteFontData.clear();
	std::wstring ext = p.has_extension() ? p.extension().wstring() : std::wstring();
	for (auto& c : ext)
		c = static_cast<wchar_t>(towlower(c));

	if (ext == L".spritefont")
	{
		std::ifstream file(p, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return false;

		std::streamsize size = file.tellg();
		if (size > 0)
		{
			m_spriteFontData.resize(static_cast<size_t>(size));
			file.seekg(0, std::ios::beg);
			if (!file.read(reinterpret_cast<char*>(m_spriteFontData.data()), size))
				return false;
		}
	}
	// 경로는 ResourceManager::Load에서 이미 SetFilePath(filePath)로 설정됨
	return true;
}

namespace
{
	bool RunMakeSpriteFont(const std::wstring& fontArg,
		const std::wstring& outputSpriteFontPath,
		const std::wstring& makeSpriteFontToolPath,
		const MMMEngine::Font::SpriteFontBuildOptions& options,
		bool requireFileExists,
		std::wstring* outError)
	{
		if (outError)
			outError->clear();

		if (makeSpriteFontToolPath.empty())
		{
			if (outError) *outError = L"MakeSpriteFont.exe 경로가 비어있습니다.";
			return false;
		}

		if (!std::filesystem::exists(makeSpriteFontToolPath))
		{
			if (outError) *outError = L"MakeSpriteFont.exe를 찾을 수 없습니다.";
			return false;
		}

		if (requireFileExists && !std::filesystem::exists(fontArg))
		{
			if (outError) *outError = L"입력 폰트 파일을 찾을 수 없습니다.";
			return false;
		}

		std::filesystem::path outPath(outputSpriteFontPath);
		if (outPath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(outPath.parent_path(), ec);
		}

		std::wstringstream cmd;
		cmd << L"\"" << makeSpriteFontToolPath << L"\" "
			<< L"\"" << fontArg << L"\" "
			<< L"\"" << outputSpriteFontPath << L"\" "
			<< L"/FontSize:" << options.fontSize;

		if (!options.characterRegion.empty())
			cmd << L" /CharacterRegion:" << options.characterRegion;

		std::wstring cmdLine = cmd.str();

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE readPipe = nullptr;
	HANDLE writePipe = nullptr;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
	{
		if (outError) *outError = L"파이프 생성 실패.";
		return false;
	}
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = writePipe;
	si.hStdError = writePipe;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	PROCESS_INFORMATION pi = {};

	std::wstring cmdMutable = cmdLine;
	cmdMutable.push_back(L'\0');
	DWORD flags = CREATE_NO_WINDOW;
	if (!CreateProcessW(nullptr, cmdMutable.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(writePipe);
		CloseHandle(readPipe);
		if (outError) *outError = L"MakeSpriteFont 실행에 실패했습니다.";
		return false;
	}

	CloseHandle(writePipe);

	std::string output;
	output.reserve(1024);

	auto readAvailable = [&output, readPipe]() -> bool
	{
		DWORD avail = 0;
		if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr))
			return false;
		if (avail == 0)
			return true;

		std::vector<char> buffer(avail + 1);
		DWORD readBytes = 0;
		if (ReadFile(readPipe, buffer.data(), avail, &readBytes, nullptr) && readBytes > 0)
		{
			buffer[readBytes] = '\0';
			output.append(buffer.data(), readBytes);
		}
		return true;
	};

	while (true)
	{
		if (!readAvailable())
			break;

		DWORD wait = WaitForSingleObject(pi.hProcess, 50);
		if (wait == WAIT_OBJECT_0)
			break;
	}

	// read remaining output
	readAvailable();
	CloseHandle(readPipe);

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

		if (exitCode != 0)
		{
			if (outError)
			{
				std::wstring message = L"MakeSpriteFont 변환 실패.";
				if (!output.empty())
				{
					int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, output.c_str(), -1, nullptr, 0);
					if (needed <= 0)
						needed = MultiByteToWideChar(CP_ACP, 0, output.c_str(), -1, nullptr, 0);

					if (needed > 0)
					{
						std::wstring wout(static_cast<size_t>(needed), L'\0');
						if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, output.c_str(), -1, wout.data(), needed) <= 0)
							MultiByteToWideChar(CP_ACP, 0, output.c_str(), -1, wout.data(), needed);
						if (!wout.empty() && wout.back() == L'\0')
							wout.pop_back();
						message += L"\n";
						message += wout;
					}
				}
				*outError = message;
			}
			return false;
		}

		return true;
	}
}

bool MMMEngine::Font::BuildSpriteFontFromTTF(const std::wstring& ttfPath,
	const std::wstring& outputSpriteFontPath,
	const std::wstring& makeSpriteFontToolPath,
	const SpriteFontBuildOptions& options,
	std::wstring* outError)
{
	return RunMakeSpriteFont(ttfPath, outputSpriteFontPath, makeSpriteFontToolPath, options, true, outError);
}

bool MMMEngine::Font::BuildSpriteFontFromName(const std::wstring& fontName,
	const std::wstring& outputSpriteFontPath,
	const std::wstring& makeSpriteFontToolPath,
	const SpriteFontBuildOptions& options,
	std::wstring* outError)
{
	return RunMakeSpriteFont(fontName, outputSpriteFontPath, makeSpriteFontToolPath, options, false, outError);
}
