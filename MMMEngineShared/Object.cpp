#include "GameObject.h"
#include "Object.h"
#include "Component.h"
#include "Transform.h"
#include "rttr/registration"
#include "rttr/type"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"
#include <stdexcept>
#include <iostream>

using namespace MMMEngine::Utility;

uint64_t MMMEngine::Object::s_nextInstanceID = 1;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Object>("Object")
		.property("Name", &Object::GetName, &Object::SetName)(rttr::metadata("INSPECTOR", "HIDDEN"))
		.property("MUID", &Object::GetMUID, &Object::SetMUID)
		.property_readonly("InstanceID", &Object::GetInstanceID)
		.property_readonly("isDestroyed", &Object::IsDestroyed)(rttr::metadata("INSPECTOR", "HIDDEN"));

	registration::class_<ObjPtrBase>("ObjPtr")
		.method("IsValid", &ObjPtrBase::IsValid)
		.method("GetRaw", &ObjPtrBase::GetRaw, registration::private_access)
		.method("GetPtrID", &ObjPtrBase::GetPtrID)
		.method("GetPtrGeneration", &ObjPtrBase::GetPtrGeneration);

	registration::class_<ObjPtr<Object>>("ObjPtr<Object>")
		.constructor<>(
			[]() { 
				return Object::NewObject<Object>(); 
			})
		.method("Inject", &ObjPtr<Object>::Inject);
}


MMMEngine::Object::Object() : m_instanceID(s_nextInstanceID++)
{
	if (!ObjectManager::Get().IsCreatingObject())
	{
		throw std::runtime_error(
			"Object는 CreatePtr로만 생성할 수 있습니다.\n"
			"스택 생성이나 직접 new 사용이 감지되었습니다."
		);
	}

	m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
	m_muid = MUID::NewMUID();
	m_ptrID = UINT32_MAX;
	m_ptrGen = 0;
}

void MMMEngine::Object::SetMUID(const Utility::MUID& muid)
{
    if (m_muid == muid)
        return;

    Utility::MUID oldMuid = m_muid;
    m_muid = muid;
    ObjectManager::Get().UpdateObjectMUID(this, oldMuid, m_muid);
}

MMMEngine::Object::~Object()
{
	if (!ObjectManager::Get().IsDestroyingObject())
	{
#ifdef _DEBUG
		// Debug: 디버거 중단 + 스택 트레이스
		std::cerr << "\n=== 오브젝트 파괴 오류 ===" << std::endl;
		std::cerr << "Object는 Destroy로만 파괴할 수 있습니다." << std::endl;
		std::cerr << "직접 delete 사용이 감지되었습니다." << std::endl;
		std::cerr << "\n>>> 호출 스택 확인 <<<\n" << std::endl;
		__debugbreak();
#endif
		// Release와 Debug 모두: 즉시 종료
		std::cerr << "\nFATAL ERROR: 허용되지 않는 방법으로 오브젝트 파괴" << std::endl;
		std::abort(); 
	}
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::Object::Instantiate(const ObjPtr<GameObject>& original)
{
	return ObjectManager::Get().Instantiate(original);
}

MMMEngine::ObjPtr<MMMEngine::Component> MMMEngine::Object::Instantiate(const ObjPtr<Component>& original)
{
	return ObjectManager::Get().Instantiate(original);
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::Object::Instantiate(const ResPtr<Prefab>& prefab)
{
	return ObjectManager::Get().Instantiate(prefab);
}

void MMMEngine::Object::DontDestroyOnLoad(const ObjPtrBase& objPtr)
{
	ObjectManager::Get().DontDestroyOnLoad(objPtr);
}

void MMMEngine::Object::Destroy(const ObjPtrBase& objPtr, float delay)
{
	if (ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<Transform>())
	{
#ifdef _DEBUG
		assert(false && "Transform은 파괴할 수 없습니다.");
#endif
		return;
	}

	ObjectManager::Get().Destroy(objPtr, delay);
}
