#include "pch.h"
#include "AnimationClip.h"
#include "ResourceSerializer.h"

RTTR_REGISTRATION{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<AnimationClip>("AnimationClip")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property("mName", &AnimationClip::mName)
		.property("durationSec", &AnimationClip::durationSec)
		.property("ticksPerSecond", &AnimationClip::ticksPerSecond)
		.property("mTracks", &AnimationClip::mTracks);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<AnimationClip>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}
			auto result = std::dynamic_pointer_cast<AnimationClip>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

//void MMMEngine::AnimationClip::FixQuatHemisphere(std::vector<Mesh_QuatKey>& keys)
//{
//	if (keys.empty()) return;
//	for (size_t i = 1; i < keys.size(); ++i)
//	{
//		auto& prev = keys[i - 1].value;
//		auto& cur = keys[i].value;
//
//		float dot = prev.x * cur.x + prev.y * cur.y + prev.z * cur.z + prev.w * cur.w;
//		if (dot < 0.0f)
//			cur = DirectX::SimpleMath::Quaternion(-cur.x, -cur.y, -cur.z, -cur.w);
//
//		cur.Normalize();
//	}
//}

bool MMMEngine::AnimationClip::LoadFromFilePath(const std::wstring& filePath)
{
	std::filesystem::path fPath{ filePath };

	if (std::filesystem::exists(fPath)) {
		ResourceSerializer::Get().DeSerialize_Animation(this, filePath);

		//// rotKey 고치기
		//for (auto& track : mTracks) {
		//	FixQuatHemisphere(track.rotKeys);
		//}

		return true;
	}

	std::cout << "AnimationClip::Animation file does not exist !!" << std::endl;
	return false;
}
