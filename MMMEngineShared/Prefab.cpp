#include "Prefab.h"

#include "json/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <rttr/registration>

namespace fs = std::filesystem;
using json = nlohmann::json;

RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<Prefab>("Prefab")
        .constructor<>()(policy::ctor::as_std_shared_ptr)
        .property_readonly("GetFilePath", &Prefab::GetFilePath);

    type::register_converter_func(
        [](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<Prefab>
        {
            if (!from) {
                ok = true;
                return nullptr;
            }
            auto result = std::dynamic_pointer_cast<Prefab>(from);
            ok = (result != nullptr);
            return result;
        }
    );
}

const MMMEngine::SnapShot& MMMEngine::Prefab::GetSnapshot() const
{
    return m_snapshot;
}

bool MMMEngine::Prefab::LoadFromFilePath(const std::wstring& filePath)
{
    fs::path fPath(filePath);
    if (!fs::exists(fPath))
        return false;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();

    try
    {
        m_snapshot = json::from_msgpack(buffer);
    }
    catch (...)
    {
        return false;
    }
    return true;
}
