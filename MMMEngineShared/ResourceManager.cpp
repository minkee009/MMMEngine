#include "ResourceManager.h"
#include "StringHelper.h"

DEFINE_SINGLETON(MMMEngine::ResourceManager)

rttr::variant MMMEngine::ResourceManager::Load(rttr::type resourceType, const std::wstring& filePath)
{
	if (!resourceType.is_valid())
		return rttr::variant();

	std::wstring root = m_rootPath.generic_wstring();
	if (!root.empty() && root.back() != L'/' && root.back() != L'\\')
		root += L'/';

	std::wstring truePath = m_rootPath.generic_wstring() + filePath;

	std::string typeName = resourceType.get_name().to_string();
	ResKey key{ typeName, truePath };

	auto it = m_cache.find(key);
	if (it != m_cache.end())
	{
		if (auto sp = it->second.lock())
			return rttr::variant(sp);

		std::weak_ptr<Resource> temp = std::move(it->second);
		m_cache.erase(it);
	}

	rttr::variant resource = resourceType.create();
	if (!resource.is_valid())
		return rttr::variant();

	std::shared_ptr<Resource> resPtr = resource.get_value<std::shared_ptr<Resource>>();
	if (!resPtr)
		return rttr::variant();

	resPtr->SetFilePath(filePath);
	if (!resPtr->LoadFromFilePath(truePath))
	{
		std::cout << Utility::StringHelper::WStringToString(truePath) << u8" : 유효하지 않은 파일패스" << std::endl;
		return rttr::variant();
	}

	m_cache[key] = resPtr;
	return rttr::variant(resPtr);
}
