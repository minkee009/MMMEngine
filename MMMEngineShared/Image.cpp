#include "Image.h"
#include "Texture2D.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Image>("Image")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Image>"))
		.property("Color", &Image::GetColor, &Image::SetColor)
		.property("Alpha", &Image::GetAlpha, &Image::SetAlpha)
		.property("Texture", &Image::GetTexture, &Image::SetTexture);

	registration::class_<ObjPtr<Image>>("ObjPtr<Image>")
		.constructor<>([]() { return Object::NewObject<Image>(); })
		.method("Inject", &ObjPtr<Image>::Inject);
}

void MMMEngine::Image::SetAlpha(float a)
{
	auto c = GetColor();
	c.w = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f : a;
	SetColor(c);
}

void MMMEngine::Image::SetTexture(const ResPtr<Texture2D>& texture)
{
	Graphic::SetTexture(texture);
}

void MMMEngine::Image::SetNativeSize()
{
	ApplyNativeSizeFromTexture(GetTexture());
}
