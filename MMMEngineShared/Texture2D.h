#pragma once
#include "Export.h"
#include "Resource.h"

#include <wrl/client.h>
#include <filesystem>
#include "d3d11_4.h"

namespace MMMEngine {
	class Material;
	class MMMENGINE_API Texture2D : public Resource
	{
	public:
		//Microsoft::WRL::ComPtr<ID3D11Texture2D1> m_pTexture = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> m_pSRV = nullptr;

		void CreateResourceView(std::filesystem::path& _path, ID3D11ShaderResourceView** _out);
		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}


