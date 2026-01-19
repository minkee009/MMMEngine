#pragma once
#include "Export.h"
#include "Resource.h"

namespace MMMEngine {
	class MMMENGINE_API PShader : public Resource
	{
	public:
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPSShader;
		Microsoft::WRL::ComPtr<ID3D10Blob> m_pBlob;

		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}


