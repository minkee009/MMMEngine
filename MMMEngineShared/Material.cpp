#include "Material.h"
#include "VShader.h"
#include "PShader.h"
#include "Texture2D.h"
#include "ResourceManager.h"
#include "RenderManager.h"
#include <DirectXTex.h>
#include <WICTextureLoader.h>
#include <RendererTools.h>
#include "json/json.hpp"

#pragma comment (lib, "DirectXTex.lib")
#pragma comment (lib, "DirectXTK.lib")

namespace fs = std::filesystem;
namespace mw = Microsoft::WRL;

// 프로퍼티 설정
void MMMEngine::Material::SetProperty(const std::wstring& name, const MMMEngine::PropertyValue& value)
{
	m_properties[name] = value;
}


// 프로퍼티 가져오기
MMMEngine::PropertyValue MMMEngine::Material::GetProperty(const std::wstring& name) const
{
	auto it = m_properties.find(name);
	if (it != m_properties.end())
		return it->second;

	// TODO :: 뒤에 name 추가하기
	throw std::runtime_error("Property not found");

}

void MMMEngine::Material::SetVShader(const std::wstring& _filePath)
{
	m_pVShader = ResourceManager::Get().Load<VShader>(_filePath);
}

void MMMEngine::Material::SetPShader(const std::wstring& _filePath)
{
	m_pPShader = ResourceManager::Get().Load<PShader>(_filePath);
}

MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::Material::GetVShader() const
{
	return m_pVShader;
}

MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::Material::GetPShader() const
{
	return m_pPShader;
}

void MMMEngine::Material::LoadTexture(const std::wstring& _propertyName, const std::wstring& _filePath)
{
	/*auto texture = ResourceManager::Get().Load<Texture2D>(_filePath);

	fs::path fPath(_filePath);

	mw::ComPtr<ID3D11ShaderResourceView> srv;
	CreateResourceView(fPath, srv.GetAddressOf());

	if (srv) {
		srv.As(&texture->m_pSRV);
	}

	SetProperty(_propertyName, texture);*/
}

bool MMMEngine::Material::LoadFromFilePath(const std::wstring& _filePath)
{
	nlohmann::json m_snapShot;


}
