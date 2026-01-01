#pragma once
#include <rttr/registration.h>
#include <rttr/detail/policies/ctor_policies.h>

#include "json/json.hpp"

using namespace nlohmann::json_abi_v3_12_0;

#include <type_traits>
#include "Object.h"

namespace MMMEngine
{
	class JsonSerializer
	{
	private:
		std::optional<json> SerializeVariant(const variant& var);
		bool DeserializeVariant(const json& j, variant& var);
	public:
		json Serialize(const Object& obj);
		bool Deserialize(const json& j, Object& obj);
	};
}
