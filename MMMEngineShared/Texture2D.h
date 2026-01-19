#pragma once
#include "Export.h"
#include "Resource.h"

#include <wrl/client.h>
#include "d3d11_4.h"

namespace MMMEngine {
	class Material;
	class MMMENGINE_API Texture2D : public Resource
	{
	public:
		//Microsoft::WRL::ComPtr<ID3D11Texture2D1> m_pTexture = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> m_pSRV = nullptr;

		bool LoadFromFilePath(const std::wstring& filePath) override { return true; }
	};
}


