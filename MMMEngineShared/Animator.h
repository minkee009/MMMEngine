#pragma once
#include "Export.h"
#include "Component.h"
#include "RenderShared.h"
#include "Object.h"
#include "ResourceManager.h"

namespace MMMEngine {
	struct AnimInfo {
		int clipIdx = -1;			// 클립 인덱스
		float elipsedTime = 0.0f;	// 애니메이션 진행시간
		int nodeIdx = -1;			// 애니메이션 루트본 인덱스 (애니메이션 일부사용용)
		float bufferWeight = 0.0f;	// 애니메이션 버퍼 weight (애니메이션 블랜딩용)

		bool isLoop = false;		// 루프 플래그
	};

	class SkinRenderer;
	class AnimationClip;
	class MMMENGINE_API Animator : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class AnimatorController;
	private:
		UINT clipIndex = (UINT)0;	// 클립 인덱스
		bool mIsReal = false;		// UnInit 플래그
		bool mIsPlaying = false;
		float mPlaySpeed = 1.0f;
		ObjPtr<SkinRenderer> mSkinComp;
		Mesh_BoneBuffer mAnimBuffer;
		std::vector<ResPtr<AnimationClip>> mAnimClips;

		// <AnimName, AnimIdx>
		std::unordered_map<std::string, int> mAnimClipIdx;
		// <AnimName, AnimInfo>
		std::unordered_map<std::string, AnimInfo> mCurrentPlayingMap;

		DirectX::SimpleMath::Vector3 SampleVecKeys(const std::vector<Mesh_VecKey>& keys, float t);
		DirectX::SimpleMath::Quaternion SampleQuatKeys(const std::vector<Mesh_QuatKey>& keys, float t);
		Mesh_VecKey Evaluate(const Mesh_VecKey& _k1, const Mesh_VecKey& _k2, float _currTime);
		Mesh_QuatKey Evaluate(const Mesh_QuatKey& _k1, const Mesh_QuatKey& _k2, float _currTime);
	
		bool IsInSubTree(const NodeTreeAsset& tree, int root, int node);
		void UpdateBoneMatrix();
		void NormalizeWeight();
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update(float _deltaTime);

		void AddAnimClip(ResPtr<AnimationClip> _clip);
		ResPtr<AnimationClip> GetAnimClip(std::string name) { return mAnimClips[mCurrentPlayingMap[name].clipIdx]; }
		void RemoveAnimClip(ResPtr<AnimationClip> _clip);

		std::vector<ResPtr<AnimationClip>>& GetAnimClips() { return mAnimClips; }
		void SetAnimClips(std::vector<ResPtr<AnimationClip>>& _clips) { mAnimClips = _clips; }
	
		void PlayClip(std::string _name, bool _isLoop = false, int _rootIdx = -1);
		void PlayBlendClip(std::string _name, float _blendWeight, bool _isLoop = false, int _rootIdx = -1);
		void StopClip();
		void PauseClip();
		void ResumeClip();
		void SetSpeed(float _speed) { if (_speed > 0) mPlaySpeed = _speed; }

		bool IsPlaying() { return mIsPlaying; }
		bool IsLooping(std::string name) { return mCurrentPlayingMap[name].isLoop; }
	};


}