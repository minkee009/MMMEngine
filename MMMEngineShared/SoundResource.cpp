#include "SoundResource.h"
#include "AudioManager.h"
#include <codecvt>
#include <locale>

static std::string ToUtf8(const std::wstring& w)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(w);
}

MMMEngine::SoundResource::~SoundResource()
{
	if (m_sound)
	{
		m_sound->release();
		m_sound = nullptr;
	}
}

bool MMMEngine::SoundResource::LoadFromFilePath(const std::wstring& filePath)
{
	FMOD::System* sys = AudioManager::Get().GetSystem();
	if (!sys) return false;

	FMOD_MODE mode = FMOD_DEFAULT;

	if (m_kind == Kind::SFX3D) mode |= FMOD_3D;
	else mode |= FMOD_2D;

	mode |= (m_loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	const std::string audioUtf8 = ToUtf8(filePath);

	FMOD_RESULT r;
	if (m_kind == Kind::BGM)
		r = sys->createStream(audioUtf8.c_str(), mode, nullptr, &m_sound);
	else
		r = sys->createSound(audioUtf8.c_str(), mode, nullptr, &m_sound);

	if (r != FMOD_OK || !m_sound)
		return false;

	if (m_kind == Kind::SFX3D)
		m_sound->set3DMinMaxDistance(m_minDist, m_maxDist);

	return true;
}