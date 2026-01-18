#pragma once
#include "Export.h"
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <typeindex>

#include <vector>

#include "MUID.h"
#include "ExportSingleton.hpp"

#include "Resource.h"

namespace MMMEngine
{
	template <typename T>
	using ResPtr = std::shared_ptr<T>;

	template <typename T>
	using ResWeakPtr = std::weak_ptr<T>;

	struct ResKey
	{
		std::string typeName;
		std::wstring path;

		bool operator==(const ResKey& o) const noexcept {
			return typeName == o.typeName && path == o.path;
		}
	};

	struct ResKeyHash
	{
		size_t operator()(const ResKey& k) const noexcept
		{
			size_t h1 = std::hash<std::wstring>{}(k.path);
			size_t h2 = std::hash<std::string>{}(k.typeName);
			return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
		}
	};

	class MMMENGINE_API ResourceManager : public Utility::ExportSingleton<ResourceManager>
	{
	private:
		std::unordered_map<ResKey, std::weak_ptr<Resource>, ResKeyHash> m_cache;

	public:
		void ClearCache() { m_cache.clear(); }

		template<class T>
		ResPtr<T> Load(std::wstring filePath)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T는 반드시 Resource를 상속받아야 합니다.");

			ResKey key{ rttr::type::get<T>().get_name().to_string(), filePath };

			if (auto it = m_cache.find(key); it != m_cache.end())
				if (auto sp = it->second.lock())
					return std::dynamic_pointer_cast<T>(sp);

			auto res = std::make_shared<T>();
			res->SetFilePath(filePath);
			if (!res->LoadFromFilePath(filePath))
				return nullptr;

			m_cache[key] = res;
			return res;
		}


		bool Contains(const std::string& typeString, const std::wstring& filePath)
		{
			ResKey key{ typeString, filePath };

			auto resource_iter = m_cache.find(key);
			if (resource_iter != m_cache.end())
			{
				auto res_shared = resource_iter->second.lock();
				if (res_shared)
					return true;

				m_cache.erase(resource_iter);
			}

			return false;
		}
	};
}