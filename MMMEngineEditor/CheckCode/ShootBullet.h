#pragma once
#include "rttr/type"
#include "ScriptBehaviour.h"
#include "UserScriptsCommon.h"
#include <SimpleMath.h>

namespace MMMEngine
{
    class USERSCRIPTS ShootBullet : public ScriptBehaviour
    {
    private:
        RTTR_ENABLE(ScriptBehaviour)
        RTTR_REGISTRATION_FRIEND
    public:
        ShootBullet()
        {
        REGISTER_BEHAVIOUR_MESSAGE(Start);
        REGISTER_BEHAVIOUR_MESSAGE(Update);

        }

        USCRIPT_MESSAGE()
        void Start();

        USCRIPT_MESSAGE()
        void Update();

        ObjPtr<GameObject> m_snowbull;

        USCRIPT_PROPERTY()
        ResPtr<Prefab> SnowBullet_Prefab;

        USCRIPT_PROPERTY()
        ObjPtr<GameObject> TestEnemy;
    };
}
