#pragma once

#include "Export.h"
#include "Resource.h"
#include <variant>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <d3d11_4.h>
#include "Texture2D.h"
#include <filesystem>
#include "Shader.h"


namespace MMMEngine {
	using PropertyValue = std::variant<
		int, float, DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Matrix,
		std::shared_ptr<MMMEngine::Texture2D>
	>;

	class MMMENGINE_API Material
	{
	private:
		std::unordered_map<std::string, PropertyValue> m_properties;
		std::shared_ptr<Shader> m_pVShader;
		std::shared_ptr<Shader> m_pPShader;

		void CreateResourceView(std::filesystem::path& _path, ID3D11ShaderResourceView** _out);
	public:
		void SetProperty(const std::string& name, const PropertyValue& value);
		PropertyValue GetProperty(const std::string& name) const;

		void SetVShader(const std::wstring& _filePath);
		void SetPShader(const std::wstring& _filePath);
		std::shared_ptr<Shader> GetShader() const;
		
		void LoadTexture(const std::string& _name, const std::wstring& _filePath);
	};
}


