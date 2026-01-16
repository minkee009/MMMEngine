#define NOMINMAX
#include "SimpleMath.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
	using namespace DirectX::SimpleMath;
	using namespace rttr;

	registration::class_<DirectX::SimpleMath::Vector3>("Vector3")
		.constructor<>()
		.constructor<float, float, float>()
		.property("x", &Vector3::x)
		.property("y", &Vector3::y)
		.property("z", &Vector3::z);

	registration::class_<DirectX::SimpleMath::Vector4>("Vector4")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Vector4::x)
		.property("y", &Vector4::y)
		.property("z", &Vector4::z)
		.property("w", &Vector4::w);

	registration::class_<DirectX::SimpleMath::Quaternion>("Quaternion")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Quaternion::x)
		.property("y", &Quaternion::y)
		.property("z", &Quaternion::z)
		.property("w", &Quaternion::w);

	registration::class_<DirectX::SimpleMath::Color>("Color")
		.constructor<>()
		.constructor<float, float, float, float>()
		.property("x", &Color::x)
		.property("y", &Color::y)
		.property("z", &Color::z)
		.property("w", &Color::w);
}