#include "Export.h"
#include "ScriptBehaviour.h"
#include "ShootBullet.h"
#include "MMMInput.h"
#include "Transform.h"
#include "SnowBullet.h"

using namespace DirectX::SimpleMath;

void MMMEngine::ShootBullet::Start()
{
	m_snowbull = Instantiate(SnowBullet_Prefab);
	Vector3 Trans = Vector3{ 0.f , 5.0f , 0.f };
	m_snowbull->GetTransform()->SetWorldPosition(Trans);
	m_snowbull->SetActive(false);
}

void MMMEngine::ShootBullet::Update()
{
	if (Input::GetKeyDown(KeyCode::P))
	{
		m_snowbull->SetActive(true);
		auto m_pos = GetTransform()->GetWorldPosition();
		m_snowbull->GetComponent<SnowBullet>()->StartBullet(m_pos, 1.0f, 15.0f, TestEnemy);
	}
}
