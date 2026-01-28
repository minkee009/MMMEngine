#pragma once
#include "Export.h"
#include "Component.h"
#include "ResourceManager.h"

#include "rttr/type"

namespace MMMEngine {
	class Texture2D;
	class MMMENGINE_API SkySphere : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
	public:
		ResPtr<Texture2D> GetSkyTexture() { return m_pSkyTexture; }
		ResPtr<Texture2D> GetSkyIrr() { return m_pSkyTexture; }
		ResPtr<Texture2D> GetSkySpecular() { return m_pSkyTexture; }
		ResPtr<Texture2D> GetSkyBrdf() { return m_pSkyTexture; }

	private:
		ResPtr<Texture2D> m_pSkyTexture;
		ResPtr<Texture2D> m_pSkyIrr;
		ResPtr<Texture2D> m_pSkySpecular;
		ResPtr<Texture2D> m_pSkyBrdf;
	};
}


