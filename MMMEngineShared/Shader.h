#pragma once

#include "Export.h"
#include "Resource.h"
#include <wrl/client.h>

#include <d3d11_4.h>

namespace MMMEngine {
	using ShaderProperty = std::variant <
		Microsoft::WRL::ComPtr<ID3D11VertexShader>, Microsoft::WRL::ComPtr<ID3D11PixelShader>
		>;

	class MMMENGINE_API Shader : public Resource {
	public:
		ShaderProperty m_ShaderProperty;
		Microsoft::WRL::ComPtr<ID3D10Blob> m_pBlob;
	};
}