#include "VShader.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include <d3dcompiler.h>
#include "rttr/registration.h"

namespace fs = std::filesystem;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<VShader>("VShader")
		.constructor<>()(policy::ctor::as_std_shared_ptr);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<VShader>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}

			auto result = std::dynamic_pointer_cast<VShader>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

bool MMMEngine::VShader::LoadFromFilePath(const std::wstring& filePath)
{
	if (!std::filesystem::exists(std::filesystem::path(filePath))) {
		std::cout << "VSShader::File not exist !!" << std::endl;
		return false;
	}

	auto m_pDevice = RenderManager::Get().GetDevice();

	// VS쉐이더 컴파일
	Microsoft::WRL::ComPtr<ID3D10Blob> errorBlob;

	HR_T(D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		0, 0,
		m_pBlob.GetAddressOf(),
		errorBlob.GetAddressOf()));

	HR_T(m_pDevice->CreateVertexShader(
		m_pBlob->GetBufferPointer(),
		m_pBlob->GetBufferSize(),
		nullptr,
		m_pVShader.GetAddressOf()));

	// InputLayout 생성
	m_pInputLayout = ShaderInfo::Get().CreateVShaderLayout(m_pBlob.Get());

	return true;
}
