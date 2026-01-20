#pragma once
#include "ExportSingleton.hpp"
#include <string>
#include "ResourceManager.h"

#include "Material.h"
#include "json/json.hpp"
#include <rttr/type>

namespace MMMEngine {
	class MMMENGINE_API MaterialSerealizer : public Utility::ExportSingleton<MaterialSerealizer>
	{
	private:
		PropertyValue property_from_json(const nlohmann::json& j);
		void to_json(nlohmann::json& j, const MMMEngine::PropertyValue& value);
	public:
		void Serealize(Material* _material, std::wstring _path);		// _path는 출력path
		void UnSerealize(Material* _material, std::wstring _path);	// _path는 입력path
	};
}


