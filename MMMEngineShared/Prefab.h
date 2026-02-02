#pragma once
#include "Export.h"
#include "Resource.h"
#include "Scene.h"
#include "rttr/registration_friend.h"

namespace MMMEngine
{
    class MMMENGINE_API Prefab : public Resource
    {
    private:
        RTTR_ENABLE(Resource);
        RTTR_REGISTRATION_FRIEND

        SnapShot m_snapshot;
    public:
        const SnapShot& GetSnapshot() const;
        bool LoadFromFilePath(const std::wstring& filePath) override;
    };
}
