//#include "JsonSerializer.h"
//#include <string>
//#include <rttr/registration>
//
//
//std::optional<json> MMMEngine::JsonSerializer::SerializeVariant(const variant& var)
//{
//	rttr::type t = var.get_type();
//
//	// 1. 기본형 자동 처리
//	if (t.is_arithmetic())
//	{
//		json j;
//		j["__type"] = t.get_name().to_string();
//		j["__value"] = var.to_string(); // 문자열로 저장
//		return j;
//	}
//
//	if (t == rttr::type::get<std::string>())
//		return var.get_value<std::string>();
//
//	// 2. enum
//	if (t.is_enumeration())
//	{
//		auto e = t.get_enumeration();
//		return e.value_to_name(var).to_string();
//	}
//
//	// 3. 사용자 정의 타입 → 재귀
//	if (t.is_class())
//	{
//		nlohmann::json obj = nlohmann::json::object();
//
//		for (auto& prop : t.get_properties())
//		{
//			auto child = prop.get_value(var);
//			if (auto j = SerializeVariant(child))
//				obj[prop.get_name().to_string()] = *j;
//		}
//
//		return obj;
//	}
//
//	return std::nullopt;
//}
//
//bool MMMEngine::JsonSerializer::DeserializeVariant(const json& j, variant& var)
//{
//	rttr::type t = var.get_type();
//
//	// 1. 기본형
//	if (j.is_object() && j.contains("__type") && j.contains("__value"))
//	{
//		std::string typeName = j["__type"];
//		std::string valueStr = j["__value"];
//
//		rttr::type storedType = rttr::type::get_by_name(typeName);
//		if (!storedType.is_valid() || storedType != t)
//			return false;
//
//		rttr::variant tmp = valueStr;
//		if (!tmp.convert(static_cast<type>(t)))
//			return false;
//
//		var = tmp;
//		return true;
//	}
//
//	// 2. string
//	if (t == rttr::type::get<std::string>())
//	{
//		if (!j.is_string())
//			return false;
//
//		var = j.get<std::string>();
//		return true;
//	}
//
//	// 3. enum
//	if (t.is_enumeration())
//	{
//		if (!j.is_string())
//			return false;
//
//		auto e = t.get_enumeration();
//		auto v = e.name_to_value(j.get<std::string>());
//		if (!v.is_valid())
//			return false;
//
//		var = v;
//		return true;
//	}
//
//	// 4. class / struct → 재귀
//	if (t.is_class() && j.is_object())
//	{
//		for (auto& prop : t.get_properties())
//		{
//			const auto name = prop.get_name().to_string();
//			if (!j.contains(name))
//				continue;
//
//			rttr::variant child = prop.get_value(var);
//			if (DeserializeVariant(j[name], child))
//			{
//				prop.set_value(var, child);
//			}
//		}
//		return true;
//	}
//
//	return false;
//}
//
//
//json MMMEngine::JsonSerializer::Serialize(const Object& obj)
//{
//
//	nlohmann::json objJson;
//
//	type t = type::get(obj);
//	objJson["type"] = t.get_name().to_string();
//	objJson["properties"] = nlohmann::json::object();
//
//	for (auto& prop : t.get_properties())
//	{
//		std::string propName = prop.get_name().to_string();
//		variant value = prop.get_value(obj);
//
//		if (auto jsonValue = SerializeVariant(value))
//		{
//			objJson["properties"][propName] = *jsonValue;
//		}
//	}
//
//	return objJson;
//}
//
//bool MMMEngine::JsonSerializer::Deserialize(const json& j, Object& obj)
//{
//	if (!j.contains("properties") || !j["properties"].is_object())
//		return false;
//
//	type t = type::get(obj);
//
//	for (auto& prop : t.get_properties())
//	{
//		std::string propName = prop.get_name().to_string();
//
//		if (!j["properties"].contains(propName))
//			continue;
//
//		variant propValue = prop.get_value(obj);
//		if (DeserializeVariant(j["properties"][propName], propValue))
//		{
//			prop.set_value(obj, propValue);
//		}
//	}
//
//	return true;
//}