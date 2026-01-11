#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "App.h"

#include "TestIO.h"

#include "EditorRegistry.h"
#include "MMMInput.h"
#include "MMMResources.h"

#include "TestPakBuilder.h"
#include "TestAssetDataBase.h"
#include "StringHelper.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;

namespace fs = std::filesystem;

static bool ends_with(const std::wstring& s, const std::wstring& suffix) {
	return s.size() >= suffix.size() &&
		s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void Init()
{
	InputManager::Get().StartUp(MMMEngine::g_pApp->GetWindowHandle());

	ResourceManager::Get().SetResolver(&Editor::g_resolver);
	ResourceManager::Get().SetBytesProvider(&Editor::g_filesProvider);

	PakAssetDatabase pakDB;
	if (!pakDB.Mount(L"TestAssets.pak"))
	{
		// 에러 처리
	}

	PakEntry entry;
	if (pakDB.TryGetEntry("C:/Users/minke/Documents/556.txt", entry))
	{
		std::vector<uint8_t> bytes;
		pakDB.ReadBytes(entry, bytes);

		// 여기 bytes가 .mtext 파일 내용과 완전히 같아야 함
	}
}

void Update_A()
{
	InputManager::Get().Update();

	if (Input::GetKeyDown(KeyCode::G))
	{
		auto fileName = OpenTextFileDialog();
		if (!fileName)
			return;

		std::vector<uint8_t> datas;
		if (!ReadFileBytes(fileName.value(), datas))
			return;

		const std::filesystem::path src = fileName.value();
		const std::wstring mtextPath = (src.parent_path() / (src.stem().wstring() + L".mtext")).wstring();
		const std::wstring metaPath = (src.wstring() + L".meta"); // hello.txt.meta

		if (!WriteMTextFile(mtextPath, datas))
			return;

		auto muid = MMMEngine::Utility::MUID::NewMUID();

		std::string canonicalPathUtf8;
#if defined(__cpp_lib_char8_t)
		// C++20 이상
		auto u8str = src.u8string();
		canonicalPathUtf8 = std::string(u8str.begin(), u8str.end());
#else
		// C++17
		canonicalPathUtf8 = src.generic_u8string();
#endif

		if (!WriteMetaJson(metaPath, muid, canonicalPathUtf8, 6))
			return;

		// 디버그 확인
		// (로그 시스템 있으면 그걸로)
		auto debug_mTextPath = L"Saved: " + mtextPath + L"\n";
		auto debug_metaPath = L"Saved: " + metaPath + L"\n";

		std::wcout << debug_mTextPath << debug_metaPath << std::endl;
	}
}

void Update_B()
{
	InputManager::Get().Update();

	if (!Input::GetKeyDown(KeyCode::G))
		return;

	// meta에서 읽은 값이라고 가정
	const fs::path dir = LR"(C:/Users/minke/Documents)"; // TODO: 경로 수정
	const std::wstring suffix = L".txt.meta";

	struct MetaData
	{
		MUID muid;
		std::string artifact;
		uint32_t type;
	};

	std::vector<MMMEngine::PakItem> pakItems;

	for (const auto& entry : fs::directory_iterator(dir)) {
		if (!entry.is_regular_file()) continue;

		std::wstring filename = entry.path().filename().generic_wstring();

		if (ends_with(filename, suffix)) {
			// ".txt.meta" 앞부분 추출
			std::wstring baseName = filename.substr(0, filename.size() - suffix.size());
			fs::path finalPath = dir / baseName;

			std::vector<uint8_t> mtextBytes;
			ReadFileBytes(finalPath.wstring() + L".mtext", mtextBytes);

			auto j = ReadMetaJson(finalPath.generic_wstring() + suffix);

			MetaData outData;

			try
			{
				if (j.contains("muid") && j["muid"].is_string() &&
					j.contains("artifact") && j["artifact"].is_string() &&
					j.contains("type") && j["type"].is_number_unsigned())
				{
					outData.muid = MMMEngine::Utility::MUID::ParseOrThrow(j["muid"].get<std::string>());
					outData.artifact = j["artifact"].get<std::string>();
					outData.type = j["type"].get<uint32_t>();
				}
				else {
					// 필수 필드가 없거나 타입이 일치하지 않는 경우
					throw std::runtime_error("JSON 파일에 필수 필드가 없거나 타입이 잘못되었습니다.");
				}
			}
			catch (const std::exception& e)
			{
				// 에러 로깅 (실제 코드에서는 로깅 시스템을 사용해야 합니다)
				// std::cerr << "ReadMetaJsonToStruct 에러: " << e.what() << std::endl;
				return;
			}

			PakItem item;
			item.pathUtf8 = outData.artifact;
			item.muid = outData.muid;   // meta에서 읽은 MUID
			item.bytes = std::move(mtextBytes);

			pakItems.emplace_back(item);
		}

	}

	PakBuilder::Build(L"TestAssets.pak", pakItems);
}

void Update_C()
{
	InputManager::Get().Update();
}

int main()
{
	App app;
	MMMEngine::g_pApp = &app;

	app.SetProcessHandle(GetModuleHandle(NULL)); //winmain으로 진입하는 경우 hIntance물려주기
	app.OnInitialize.AddListener<&Init>();
	app.OnUpdate.AddListener<&Update_B>();
	app.Run();
}