#include "Material.h"
#include "Shader.h"
#include "Texture2D.h"
#include "ResourceManager.h"
#include "RenderManager.h"
#include <DirectXTex.h>
#include <WICTextureLoader.h>
#include <RendererTools.h>

#pragma comment (lib, "DirectXTex.lib")
#pragma comment (lib, "DirectXTK.lib")

namespace fs = std::filesystem;
namespace mw = Microsoft::WRL;

// 프로퍼티 설정
void MMMEngine::Material::SetProperty(const std::string& name, const MMMEngine::PropertyValue& value)
{
	m_properties[name] = value;
}

// SRV 만들어주는 함수
void MMMEngine::Material::CreateResourceView(std::filesystem::path& _path, ID3D11ShaderResourceView** _out)
{
	// 디바이스 가져오기
	auto m_pDevice = RenderManager::Get().m_pDevice;

	// TGA파일 확인
	if (_path.extension() == L".tga") {
		DirectX::ScratchImage image;
		DirectX::TexMetadata meta;
		HR_T(DirectX::LoadFromTGAFile(_path.wstring().c_str(), &meta, image));
		HR_T(DirectX::CreateShaderResourceView(m_pDevice.Get(), image.GetImages(), image.GetImageCount(), meta, _out));
	}
	else {
		ID3D11DeviceContext* context = nullptr;
		m_pDevice->GetImmediateContext(&context);

		HR_T(CreateWICTextureFromFileEx(
			m_pDevice.Get(),
			context,
			_path.wstring().c_str(),
			0,
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			DirectX::DX11::WIC_LOADER_FLAGS::WIC_LOADER_FORCE_RGBA32 | DirectX::DX11::WIC_LOADER_FLAGS::WIC_LOADER_IGNORE_SRGB,
			nullptr,
			_out));
	}
}

// 프로퍼티 가져오기
MMMEngine::PropertyValue MMMEngine::Material::GetProperty(const std::string& name) const
{
	auto it = m_properties.find(name);
	if (it != m_properties.end())
		return it->second;

	throw std::runtime_error("Property not found: " + name);

}

void MMMEngine::Material::SetVShader(const std::wstring& _filePath)
{
	fs::path fPath(_filePath);

	if (fPath.empty() || !fs::exists(fPath))
		throw std::runtime_error("Material::VSShader not exist !!");

	auto device = RenderManager::Get().GetDevice();
	auto& blob = m_pVShader->m_pBlob;

	HR_T(CompileShaderFromFile(_filePath.c_str(), "main", "vs_5_0", &blob));

	mw::ComPtr<ID3D11VertexShader> vsShader;
	HR_T(device->CreateVertexShader(
		blob->GetBufferPointer(),
		blob->GetBufferSize(),
		nullptr,
		vsShader.GetAddressOf()
	));

	if (vsShader)
		m_pVShader->m_ShaderProperty = vsShader;
}

void MMMEngine::Material::SetPShader(const std::wstring& _filePath)
{
	fs::path fPath(_filePath);

	if (fPath.empty() || !fs::exists(fPath))
		throw std::runtime_error("Material::PSShader not exist !!");

	auto device = RenderManager::Get().GetDevice();
	auto& blob = m_pPShader->m_pBlob;

	HR_T(CompileShaderFromFile(_filePath.c_str(), "main", "vs_5_0", &blob));

	mw::ComPtr<ID3D11PixelShader> psShader;
	HR_T(device->CreatePixelShader(
		blob->GetBufferPointer(),
		blob->GetBufferSize(),
		nullptr,
		psShader.GetAddressOf()
	));

	if (psShader)
		m_pPShader->m_ShaderProperty = psShader;
}

std::shared_ptr<MMMEngine::Shader> MMMEngine::Material::GetShader() const
{
	return std::shared_ptr<Shader>();
}

void MMMEngine::Material::LoadTexture(const std::string& _propertyName, const std::wstring& _filePath)
{
	auto texture = ResourceManager::Get().Load<Texture2D>(_filePath);

	fs::path fPath(_filePath);

	mw::ComPtr<ID3D11ShaderResourceView> srv;
	CreateResourceView(fPath, srv.GetAddressOf());
	
	if (srv) {
		srv.As(&texture->m_pSRV);
	}

	SetProperty(_propertyName, texture);
}
