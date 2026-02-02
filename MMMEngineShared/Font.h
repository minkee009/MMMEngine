#pragma once

#include "Export.h"
#include "Resource.h"
#include "rttr/type"
#include "rttr/registration_friend.h"
#include <string>
#include <vector>

namespace MMMEngine
{
	/// 폰트 리소스. Resource 상속. .spritefont 등 폰트 파일 경로 보관.
	/// 실제 렌더링은 RenderManager DrawUIText 등에서 경로로 로드해 사용.
	class MMMENGINE_API Font : public Resource
	{
	private:
		RTTR_ENABLE(Resource)
		RTTR_REGISTRATION_FRIEND
		friend class ResourceManager;
		friend class SceneManager;
		friend class Scene;

		std::vector<uint8_t> m_spriteFontData;

	public:
		struct SpriteFontBuildOptions
		{
			int fontSize = 32;
			std::wstring characterRegion; // 예: L"0x20-0x7E"
		};

		bool LoadFromFilePath(const std::wstring& filePath) override;

		bool HasSpriteFontData() const { return !m_spriteFontData.empty(); }
		const std::vector<uint8_t>& GetSpriteFontData() const { return m_spriteFontData; }

		static bool BuildSpriteFontFromTTF(const std::wstring& ttfPath,
			const std::wstring& outputSpriteFontPath,
			const std::wstring& makeSpriteFontToolPath,
			const SpriteFontBuildOptions& options,
			std::wstring* outError = nullptr);
		static bool BuildSpriteFontFromName(const std::wstring& fontName,
			const std::wstring& outputSpriteFontPath,
			const std::wstring& makeSpriteFontToolPath,
			const SpriteFontBuildOptions& options,
			std::wstring* outError = nullptr);
	};
}
