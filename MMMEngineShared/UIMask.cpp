#include "UIMask.h"
#include "Graphic.h"
#include "GameObject.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<UIMask>("UIMask")
		(rttr::metadata("wrapper_type_name", "ObjPtr<UIMask>"))
		.property("AlphaThreshold", &UIMask::GetAlphaThreshold, &UIMask::SetAlphaThreshold)
		.property("ShowGraphic", &UIMask::GetShowGraphic, &UIMask::SetShowGraphic);

	registration::class_<ObjPtr<UIMask>>("ObjPtr<UIMask>")
		.constructor<>([]() { return Object::NewObject<UIMask>(); })
		.method("Inject", &ObjPtr<UIMask>::Inject);
}

void MMMEngine::UIMask::SetAlphaThreshold(float v)
{
	if (v < 0.0f)
		v = 0.0f;
	if (v > 1.0f)
		v = 1.0f;
	m_alphaThreshold = v;
}

MMMEngine::ObjPtr<MMMEngine::Graphic> MMMEngine::UIMask::GetTargetGraphic() const
{
	auto go = GetGameObject();
	if (!go.IsValid())
		return nullptr;

	return go->GetComponent<Graphic>();
}
