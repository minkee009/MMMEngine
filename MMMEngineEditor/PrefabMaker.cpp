#include "PrefabMaker.h"
#include "ObjectManager.h"

bool MMMEngine::Editor::PrefabMaker::CreatePrefabFromGameObject(const ObjPtr<GameObject>& root,
    const std::filesystem::path& directory)
{
    return ObjectManager::Get().CreatePrefabFromGameObject(root, directory);
}
