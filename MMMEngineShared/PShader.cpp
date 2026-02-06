#include "PShader.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include <d3dcompiler.h>

#include <rttr/registration>

namespace fs = std::filesystem;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<PShader>("PShader")
		.constructor<>()(policy::ctor::as_std_shared_ptr);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<PShader>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}

			auto result = std::dynamic_pointer_cast<PShader>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

bool MMMEngine::PShader::LoadFromFilePath(const std::wstring& filePath)
{
	if (!fs::exists(fs::path(filePath))) {
		std::cout << "PShader::File not exist !!" << std::endl;
		return false;
	}

	auto m_pDevice = RenderManager::Get().GetDevice();

	// PS쉐이더 컴파일
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HR_T(D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		0, 0,
		m_pBlob.GetAddressOf(),
		errorBlob.GetAddressOf()), "PShader::");

	if (errorBlob)
	{
		const char* errMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
		size_t errSize = errorBlob->GetBufferSize();

		// 콘솔 출력
		std::cerr << std::string(errMsg, errSize) << std::endl;
	}

	HR_T(m_pDevice->CreatePixelShader(
		m_pBlob->GetBufferPointer(),
		m_pBlob->GetBufferSize(),
		nullptr,
		m_pPShader.GetAddressOf()), "PShader::");

	return true;
}
