#include "FontImporter.h"
#include "Font.h"

#include <dwrite.h>
#include <dwrite_3.h>
#include <filesystem>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
	bool TryGetFontFamilyNameFromFile(const std::wstring& filePath, std::wstring& outFamily, std::wstring& outError)
	{
		outFamily.clear();
		outError.clear();

		ComPtr<IDWriteFactory> factory;
		HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
		if (FAILED(hr))
		{
			outError = L"DWriteCreateFactory 실패.";
			return false;
		}

		ComPtr<IDWriteFontFile> fontFile;
		hr = factory->CreateFontFileReference(filePath.c_str(), nullptr, &fontFile);
		if (FAILED(hr))
		{
			outError = L"폰트 파일 참조 생성 실패.";
			return false;
		}

		BOOL isSupported = FALSE;
		DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
		DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
		UINT32 numFaces = 0;
		hr = fontFile->Analyze(&isSupported, &fileType, &faceType, &numFaces);
		if (FAILED(hr) || !isSupported || numFaces == 0)
		{
			outError = L"지원하지 않는 폰트 파일입니다.";
			return false;
		}

		ComPtr<IDWriteFontFace> fontFace;
		IDWriteFontFile* files[] = { fontFile.Get() };
		hr = factory->CreateFontFace(faceType, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &fontFace);
		if (FAILED(hr))
		{
			outError = L"폰트 페이스 생성 실패.";
			return false;
		}

		ComPtr<IDWriteFontFace3> fontFace3;
		hr = fontFace.As(&fontFace3);
		if (FAILED(hr) || !fontFace3)
		{
			outError = L"IDWriteFontFace3를 지원하지 않습니다.";
			return false;
		}

		ComPtr<IDWriteLocalizedStrings> names;
		hr = fontFace3->GetFamilyNames(&names);
		if (FAILED(hr) || !names)
		{
			outError = L"폰트 패밀리 이름을 가져오지 못했습니다.";
			return false;
		}

		UINT32 index = 0;
		BOOL found = FALSE;
		hr = names->FindLocaleName(L"ko-kr", &index, &found);
		if (FAILED(hr) || !found)
		{
			hr = names->FindLocaleName(L"en-us", &index, &found);
			if (FAILED(hr) || !found)
				index = 0;
		}

		UINT32 length = 0;
		hr = names->GetStringLength(index, &length);
		if (FAILED(hr) || length == 0)
		{
			outError = L"폰트 패밀리 이름 길이를 가져오지 못했습니다.";
			return false;
		}

		std::wstring family(static_cast<size_t>(length) + 1, L'\0');
		hr = names->GetString(index, family.data(), length + 1);
		if (FAILED(hr))
		{
			outError = L"폰트 패밀리 이름을 가져오지 못했습니다.";
			return false;
		}

		if (!family.empty() && family.back() == L'\0')
			family.pop_back();

		outFamily = family;
		return true;
	}

	bool IsFontFamilyInstalled(const std::wstring& familyName)
	{
		ComPtr<IDWriteFactory> factory;
		HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
		if (FAILED(hr))
			return false;

		ComPtr<IDWriteFontCollection> collection;
		hr = factory->GetSystemFontCollection(&collection);
		if (FAILED(hr) || !collection)
			return false;

		UINT32 index = 0;
		BOOL exists = FALSE;
		collection->FindFamilyName(familyName.c_str(), &index, &exists);
		return exists == TRUE;
	}
}

bool MMMEngine::FontImporter::ConvertTTFToSpriteFont(const std::wstring& inputPath,
	const std::wstring& outputPath,
	const BuildOptions& options,
	std::wstring& outError)
{
	Font::SpriteFontBuildOptions buildOptions;
	buildOptions.fontSize = options.fontSize;
	buildOptions.characterRegion = options.characterRegion;

	std::filesystem::path inputFs(inputPath);
	bool looksLikeFile = inputFs.has_extension() && std::filesystem::exists(inputFs);
	if (looksLikeFile)
	{
		// Prefer file stem first so style suffixes like "R/B" are preserved.
		std::wstring stemName = inputFs.stem().wstring();
		if (!stemName.empty())
		{
			if (Font::BuildSpriteFontFromName(stemName, outputPath, m_makeSpriteFontPath, buildOptions, &outError))
				return true;
		}

		std::wstring familyName;
		std::wstring nameErr;
		if (!TryGetFontFamilyNameFromFile(inputPath, familyName, nameErr))
		{
			if (!outError.empty())
				outError += L"\n";
			outError += nameErr;
			return Font::BuildSpriteFontFromTTF(inputPath, outputPath, m_makeSpriteFontPath, buildOptions, &outError);
		}

		if (!IsFontFamilyInstalled(familyName))
		{
			if (outError.empty())
				outError = L"폰트가 시스템에 설치되어 있지 않습니다. 파일 경로 기반 변환을 시도합니다.\nFont Name: " + familyName;
			return Font::BuildSpriteFontFromTTF(inputPath, outputPath, m_makeSpriteFontPath, buildOptions, &outError);
		}

		if (Font::BuildSpriteFontFromName(familyName, outputPath, m_makeSpriteFontPath, buildOptions, &outError))
			return true;

		return Font::BuildSpriteFontFromTTF(inputPath, outputPath, m_makeSpriteFontPath, buildOptions, &outError);
	}

	// 입력이 파일 경로가 아니라면 폰트 이름으로 간주
	return Font::BuildSpriteFontFromName(inputPath, outputPath, m_makeSpriteFontPath, buildOptions, &outError);
}
