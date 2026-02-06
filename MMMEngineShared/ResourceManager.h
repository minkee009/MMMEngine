#pragma once
#include "Export.h"
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <typeindex>
#include <filesystem>

#include <vector>

#include "MUID.h"
#include "ExportSingleton.hpp"

#include "Resource.h"
// todo 삭제
#include <iostream>

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
		std::filesystem::path m_rootPath;
	public:
		std::filesystem::path GetCurrentRootPath() { return m_rootPath; }

		void ClearCache() { m_cache.clear(); }

		void StartUp(std::wstring rootPath)
		{
			m_rootPath = rootPath;
		}

		void ShutDown()
		{
			m_cache.clear();
		}

		template<class T>
		ResPtr<T> Load(std::wstring filePath)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T must inherit from Resource");
			rttr::variant loaded = Load(rttr::type::get<T>(), filePath);
			if (!loaded.is_valid())
				return nullptr;

			if (loaded.is_type<ResPtr<Resource>>())
			{
				return std::dynamic_pointer_cast<T>(loaded.get_value<ResPtr<Resource>>());
			}
			if (loaded.is_type<ResPtr<T>>())
				return loaded.get_value<ResPtr<T>>();

			return nullptr;
		}

		rttr::variant Load(rttr::type resourceType, const std::wstring& filePath);

		bool Contains(const std::string& typeString, const std::wstring& filePath)
		{
			std::wstring truePath = m_rootPath.generic_wstring() + filePath;
			ResKey key{ typeString, truePath };

			auto resource_iter = m_cache.find(key);
			if (resource_iter != m_cache.end())
			{
				auto res_shared = resource_iter->second.lock();
				if (res_shared)
					return true;

				std::weak_ptr<Resource> temp = std::move(resource_iter->second);

				m_cache.erase(resource_iter);
			}

			return false;
		}
	};
}
