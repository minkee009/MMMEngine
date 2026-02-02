#pragma once
#include "ExportSingleton.hpp"
#include "ResourceManager.h"
#include "Object.h"
#include <filesystem>

namespace MMMEngine
{
    class GameObject;
    class Component;
    class Prefab;

    class MMMENGINE_API ObjectSerializer : public Utility::ExportSingleton<ObjectSerializer>
    {
    public:
        ObjPtr<GameObject> Instantiate(const ObjPtr<GameObject>& original);
        ObjPtr<Component> Instantiate(const ObjPtr<Component>& original);
        ObjPtr<GameObject> Instantiate(const ResPtr<Prefab>& prefab);

        bool CreatePrefabFromGameObject(const ObjPtr<GameObject>& root,
            const std::filesystem::path& directory);
    };
}
