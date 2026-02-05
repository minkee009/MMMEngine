#include "pch.h"
#include "Animator.h"
#include "SkinRenderer.h"
#include "SkeletalMesh.h"
#include "AnimationClip.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Animator>("Animator")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Animator>"));
		//.property("AnimClips", &Animator::GetAnimClips, &Animator::SetAnimClips);

	registration::class_<ObjPtr<Animator>>("ObjPtr<Animator>")
		.constructor<>(
			[]() {
				return Object::NewObject<Animator>();
			})
		.method("Inject", &ObjPtr<Animator>::Inject);
}

static void DecomposeTRS(
	DirectX::SimpleMath::Matrix& m,
	DirectX::SimpleMath::Vector3& outS,
	DirectX::SimpleMath::Quaternion& outR,
	DirectX::SimpleMath::Vector3& outT)
{
	m.Decompose(outS, outR, outT);
}

static DirectX::SimpleMath::Quaternion WeightedAddQuat(
	const DirectX::SimpleMath::Quaternion& acc,
	const DirectX::SimpleMath::Quaternion& q,
	float w)
{
	DirectX::SimpleMath::Quaternion qq = q;

	// hemisphere fix (뒤집힘 방지)
	float dot = acc.x * qq.x + acc.y * qq.y + acc.z * qq.z + acc.w * qq.w;
	if (dot < 0.0f)
	{
		qq.x = -qq.x; qq.y = -qq.y; qq.z = -qq.z; qq.w = -qq.w;
	}

	auto result = DirectX::SimpleMath::Quaternion(
		acc.x + qq.x * w,
		acc.y + qq.y * w,
		acc.z + qq.z * w,
		acc.w + qq.w * w);

	return result;
}

DirectX::SimpleMath::Vector3 MMMEngine::Animator::SampleVecKeys(const std::vector<MMMEngine::Mesh_VecKey>& keys, float t)
{
	if (keys.empty()) return { 0,0,0 };
	if (keys.size() == 1) return keys[0].value;

	// 끝/처음 클램프
	if (t <= keys.front().timeSec) return keys.front().value;
	if (t >= keys.back().timeSec)  return keys.back().value;

	// 선형 탐색 (키가 많으면 lower_bound 추천)
	for (size_t i = 0; i + 1 < keys.size(); ++i)
	{
		const auto& k1 = keys[i];
		const auto& k2 = keys[i + 1];

		if (t >= k1.timeSec && t <= k2.timeSec)
		{
			return Evaluate(k1, k2, t).value;
		}
	}
	return keys.back().value;
}

DirectX::SimpleMath::Quaternion MMMEngine::Animator::SampleQuatKeys(const std::vector<Mesh_QuatKey>& keys, float t)
{
	if (keys.empty()) return DirectX::SimpleMath::Quaternion::Identity;
	if (keys.size() == 1) return keys[0].value;

	if (t <= keys.front().timeSec) return keys.front().value;
	if (t >= keys.back().timeSec)  return keys.back().value;

	for (size_t i = 0; i + 1 < keys.size(); ++i)
	{
		const auto& k1 = keys[i];
		const auto& k2 = keys[i + 1];

		if (t >= k1.timeSec && t <= k2.timeSec)
		{
			return Evaluate(k1, k2, t).value;
		}
	}
	return keys.back().value;
}

MMMEngine::Mesh_VecKey MMMEngine::Animator::Evaluate(const Mesh_VecKey& _k1, const Mesh_VecKey& _k2, float _currTime)
{
	if (_k1.value == _k2.value)
		return _k1;

	float lerpTime = (_currTime - _k1.timeSec) / (_k2.timeSec - _k1.timeSec);
	
	return {
		_currTime,
		_k1.value + (_k2.value - _k1.value) * lerpTime
	};
}

bool MMMEngine::Animator::IsInSubTree(const NodeTreeAsset& tree, int root, int node)
{
	if (root < 0) return true;      // -1이면 전체 적용
	if (root == node) return true;

	int p = tree.nodes[node].parentIndex;
	while (p >= 0)
	{
		if (p == root) return true;
		p = tree.nodes[p].parentIndex;
	}
	return false;
}

MMMEngine::Mesh_QuatKey MMMEngine::Animator::Evaluate(const Mesh_QuatKey& _k1, const Mesh_QuatKey& _k2, float _currTime)
{
	if (_k1.value == _k2.value)
		return _k1;

	float lerpTime = (_currTime - _k1.timeSec) / (_k2.timeSec - _k1.timeSec);

	DirectX::SimpleMath::Quaternion temp;
	temp = DirectX::SimpleMath::Quaternion::Slerp(_k1.value, _k2.value, lerpTime);
	return {
		_currTime,
		temp };
}

void MMMEngine::Animator::UpdateBoneMatrix()
{
	if (!mSkinComp.IsValid())
		return;

	auto& mesh = mSkinComp->GetMesh();
	if (!mesh)
		return;

	// 재생 중인 clip이 없으면 그냥 identity/bind 유지
	bool anyPlaying = false;
	for (auto& [name, info] : mCurrentPlayingMap)
	{
		if (info.bufferWeight > 0.0f && info.clipIdx >= 0 && info.clipIdx < (int)mAnimClips.size())
		{
			if (mSkinComp->mAnimBuffer == nullptr)
				mSkinComp->mAnimBuffer = &mAnimBuffer;
			anyPlaying = true;
			break;
		}
	}
	if (!anyPlaying) {
		mSkinComp->mAnimBuffer = nullptr;
		return;
	}
	


	// 노드트리 불러오기
	NodeTreeAsset& tree = mSkinComp->mesh->mNodeTree;
	const int nodeCount = (int)tree.nodes.size();
	if (nodeCount <= 0)
		return;

	//bindLocal -> bind TRS 준비
	std::vector<DirectX::SimpleMath::Vector3> bindPos(nodeCount);
	std::vector<DirectX::SimpleMath::Vector3> bindScale(nodeCount);
	std::vector<DirectX::SimpleMath::Quaternion> bindRot(nodeCount);

	for (int n = 0; n < nodeCount; ++n)
	{
		DirectX::SimpleMath::Vector3 s, t;
		DirectX::SimpleMath::Quaternion r;
		DecomposeTRS(tree.nodes[n].bindLocal, s, r, t);

		bindPos[n] = t;
		bindRot[n] = r;
		bindScale[n] = s;
	}


	// 블렌딩 누적 버퍼
	std::vector<DirectX::SimpleMath::Vector3> outPos = bindPos;
	std::vector<DirectX::SimpleMath::Vector3> outScale = bindScale;
	std::vector<DirectX::SimpleMath::Quaternion> outRot = bindRot;

	// "bind 유지 + 애니 추가" 방식이면 누적을 위해 별도 accumulator가 필요
	std::vector<DirectX::SimpleMath::Vector3> accPos(nodeCount, { 0,0,0 });
	std::vector<DirectX::SimpleMath::Vector3> accScale(nodeCount, { 0,0,0 });
	std::vector<DirectX::SimpleMath::Quaternion> accRot(nodeCount, DirectX::SimpleMath::Quaternion(0, 0, 0, 0));
	std::vector<float> accW(nodeCount, 0.0f);


	// 각 clip 샘플링해서 누적
	for (auto& [animName, info] : mCurrentPlayingMap)
	{
		if (info.bufferWeight <= 0.0f)
			continue;

		if (info.clipIdx < 0 || info.clipIdx >= (int)mAnimClips.size())
			continue;

		ResPtr<AnimationClip> clip = mAnimClips[info.clipIdx];
		if (!clip)
			continue;

		float w = info.bufferWeight;
		int rootIdx = info.nodeIdx; // -1이면 전체

		// 시간 계산
		float t = info.elipsedTime;

		if (clip->durationSec > 0.0f)
		{
			if (info.isLoop)
			{
				t = std::fmod(t, clip->durationSec);
				if (t < 0.0f) t += clip->durationSec;
			}
			else
			{
				if (t > clip->durationSec) t = clip->durationSec;
				if (t < 0.0f) t = 0.0f;
			}
		}

		// 트랙 적용
		for (const auto& tr : clip->mTracks)
		{
			int n = tr.nodeIndex;
			if (n < 0 || n >= nodeCount) continue;

			// subtree 밖이면 skip (부분 적용)
			if (!IsInSubTree(tree, rootIdx, n))
				continue;

			// 키 없는 축은 bind 유지
			DirectX::SimpleMath::Vector3 pos = bindPos[n];
			DirectX::SimpleMath::Quaternion rot = bindRot[n];
			DirectX::SimpleMath::Vector3 scl = bindScale[n];

			if (!tr.posKeys.empty())   pos = SampleVecKeys(tr.posKeys, t);
			if (!tr.rotKeys.empty())   rot = SampleQuatKeys(tr.rotKeys, t);
			if (!tr.scaleKeys.empty()) scl = SampleVecKeys(tr.scaleKeys, t);

			// 누적
			accPos[n] += pos * w;
			accScale[n] += scl * w;
			accRot[n] = WeightedAddQuat(accRot[n], rot, w);
			accW[n] += w;
		}
	}

	// 누적값을 최종 TRS로 변환 (키가 없던 노드는 bind 유지)
	for (int n = 0; n < nodeCount; ++n)
	{
		if (accW[n] <= 0.0f)
		{
			outPos[n] = bindPos[n];
			outRot[n] = bindRot[n];
			outScale[n] = bindScale[n];
			continue;
		}

		float inv = 1.0f / accW[n];
		outPos[n] = accPos[n] * inv;
		outScale[n] = accScale[n] * inv;

		// normalize quaternion
		DirectX::SimpleMath::Quaternion q = accRot[n];
		float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
		if (len > 0.00001f)
		{
			q.x /= len; q.y /= len; q.z /= len; q.w /= len;
			outRot[n] = q;
		}
		else
		{
			outRot[n] = bindRot[n];
		}
	}

	// local/global matrix 생성
	std::vector<DirectX::SimpleMath::Matrix> local(nodeCount, DirectX::SimpleMath::Matrix::Identity);
	std::vector<DirectX::SimpleMath::Matrix> global(nodeCount, DirectX::SimpleMath::Matrix::Identity);

	for (int n = 0; n < nodeCount; ++n)
	{
		if (accW[n] <= 0.0f)
		{
			local[n] = tree.nodes[n].bindLocal;
			continue;
		}

		local[n] =
			DirectX::SimpleMath::Matrix::CreateScale(outScale[n]) *
			DirectX::SimpleMath::Matrix::CreateFromQuaternion(outRot[n]) *
			DirectX::SimpleMath::Matrix::CreateTranslation(outPos[n]);

		local[n] = XMMatrixTranspose(local[n]);
	}

	// global: DFS 방식으로
	std::vector<char> visited(nodeCount, 0);
	std::function<void(int)> EvalGlobal = [&](int n)
		{
			if (visited[n]) return;
			visited[n] = 1;

			int p = tree.nodes[n].parentIndex;
			if (p < 0) global[n] = local[n];
			else
			{
				EvalGlobal(p);
				global[n] = global[p] * local[n];
			}
		};

	for (int n = 0; n < nodeCount; ++n)
		EvalGlobal(n);

	// Bone Palette 256 채우기
	mAnimBuffer.BoneMat.fill(DirectX::SimpleMath::Matrix::Identity);

	// nodeIdxData: <BoneIdx, NodeIdx>
	for (const auto& [boneIdxU, nodeIdxU] : mesh->nodeIdxData)
	{
		if (boneIdxU >= BONE_MAXSIZE) continue;

		int nodeIdx = (int)nodeIdxU;
		if (nodeIdx < 0 || nodeIdx >= nodeCount) continue;

		// global
		mAnimBuffer.BoneMat[boneIdxU] = global[nodeIdx];
	}
}

void MMMEngine::Animator::NormalizeWeight()
{
	// 전체 weight 정규화
	float sum = 0.0f;
	for (auto& [name, anim] : mCurrentPlayingMap)
		sum += anim.bufferWeight;

	if (sum > 1.0f && sum > 0.0f)
	{
		for (auto& [name, anim] : mCurrentPlayingMap)
			anim.bufferWeight /= sum;
	}
}

void MMMEngine::Animator::NormalizeWeightExcept(const std::string& fixedName)
{
	// fixed clip이 없으면 기존 NormalizeWeight와 동일 처리
	auto itFixed = mCurrentPlayingMap.find(fixedName);
	if (itFixed == mCurrentPlayingMap.end())
	{
		NormalizeWeight();
		return;
	}

	float fixedW = itFixed->second.bufferWeight;
	if (fixedW < 0.0f) fixedW = 0.0f;
	if (fixedW > 1.0f) fixedW = 1.0f;

	// 나머지 합
	float othersSum = 0.0f;
	for (auto& [name, anim] : mCurrentPlayingMap)
	{
		if (name == fixedName) continue;
		if (anim.bufferWeight <= 0.0f) continue;
		othersSum += anim.bufferWeight;
	}

	// 남은 예산
	float remain = 1.0f - fixedW;
	if (remain < 0.0f) remain = 0.0f;

	// fixed만 남기고 나머지 0 처리
	if (remain == 0.0f)
	{
		for (auto& [name, anim] : mCurrentPlayingMap)
		{
			if (name == fixedName) continue;
			anim.bufferWeight = 0.0f;
		}
		itFixed->second.bufferWeight = fixedW;
		return;
	}

	// 나머지가 없으면 fixed만 유지
	if (othersSum <= 0.00001f)
	{
		itFixed->second.bufferWeight = fixedW;
		return;
	}

	// 나머지들을 remain에 맞게 비율 스케일
	float scale = remain / othersSum;
	for (auto& [name, anim] : mCurrentPlayingMap)
	{
		if (name == fixedName) continue;
		anim.bufferWeight *= scale;
	}

	// 고정값 다시 보장
	itFixed->second.bufferWeight = fixedW;
}

void MMMEngine::Animator::Initialize()
{
	mSkinComp = GetComponent<SkinRenderer>();
	if (!mSkinComp)
		Destroy(SelfPtr(this));
		
	mIsReal = mSkinComp->SetAnimatior(this);
	if (!mIsReal)
		Destroy(SelfPtr(this));

	auto clip = ResourceManager::Get().Load<AnimationClip>(L"Assets/Test/Player_Idle_0.animclip");
	AddAnimClip(clip);
	PlayClip(clip->mName, true);
}

void MMMEngine::Animator::UnInitialize()
{
	if (mIsReal)
		mSkinComp->RemoveAnimator();
	mSkinComp->mAnimBuffer = nullptr;

	mSkinComp.Reset();
	mCurrentPlayingMap.clear();
}

void MMMEngine::Animator::Update(float _deltaTime)
{
	if (!mIsPlaying)
		return;

	for (auto& [name, info] : mCurrentPlayingMap)
	{
		if (info.bufferWeight <= 0.0f)
			continue;

		if (mIsPlaying)
			info.elipsedTime += (_deltaTime * mPlaySpeed);
	}

	UpdateBoneMatrix();
}

void MMMEngine::Animator::AddAnimClip(ResPtr<AnimationClip> _clip)
{
	int idx = mAnimClips.size();
	mAnimClips.push_back(_clip);
	mAnimClipIdx[_clip->mName] = idx;
}

void MMMEngine::Animator::RemoveAnimClip(ResPtr<AnimationClip> _clip)
{
	// 애니메이션 목록에 있는지 확인
	auto it = mAnimClipIdx.find(_clip->mName);
	if (it == mAnimClipIdx.end())
		return;
	
	int removeIdx = it->second;

	// 사이즈 확인
	if (mAnimClipIdx.size() == 1) {
		mAnimClipIdx.clear();
		mAnimClips.clear();
		mCurrentPlayingMap.erase(_clip->mName);
		return;
	}
	 // 마지막 클립을 제거할 위치로 이동
	mAnimClips[removeIdx].swap(mAnimClips.back());

	// 맵 업데이트: 마지막 클립 이름 → removeIdx
	mAnimClipIdx[mAnimClips[removeIdx]->mName] = removeIdx;

	// 제거 대상 이름 삭제
	mAnimClipIdx.erase(it);
	mCurrentPlayingMap.erase(_clip->mName);

	// 벡터에서 마지막 요소 제거
	mAnimClips.pop_back();
}

void MMMEngine::Animator::ResumeClip()
{
	if(!mIsPlaying)
	mIsPlaying = true;
}

void MMMEngine::Animator::PauseClip()
{
	mIsPlaying = false;
}

void MMMEngine::Animator::StopClip()
{
	mIsPlaying = false;
	mCurrentPlayingMap.clear();
}

void MMMEngine::Animator::PlayClip(std::string _name, bool _isLoop /*= false*/, int _rootIdx /*= -1*/)
{
	// 이름으로 AnimInfo가 존재하는지 확인
	auto it = mAnimClipIdx.find(_name);
	if (it == mAnimClipIdx.end())
		return;

	AnimInfo info;
	int clipIdx = it->second;

	// clipIdx 유효성 체크
	if (clipIdx < 0 || clipIdx >= (int)mAnimClips.size())
		return;

	// 기존 재생들 전부 끄고 이거만 재생(단일 재생 정책)
	for (auto& [name, anim] : mCurrentPlayingMap)
	{
		anim.elipsedTime = 0.0f;
		anim.bufferWeight = 0.0f;
	}

	// 현재 클립 설정
	info.clipIdx = clipIdx;
	info.elipsedTime = 0.0f;
	info.nodeIdx = _rootIdx;
	info.bufferWeight = 1.0f;
	info.isLoop = _isLoop;

	mCurrentPlayingMap[_name] = info;

	mIsPlaying = true;
}

void MMMEngine::Animator::PlayBlendClip(std::string _name, float _blendWeight, bool _isLoop /*= false*/, int _rootIdx /*= -1*/)
{
	// weight clamp
	if (_blendWeight < 0.0f) _blendWeight = 0.0f;
	if (_blendWeight > 1.0f) _blendWeight = 1.0f;

	auto it = mCurrentPlayingMap.find(_name);
	if (it == mCurrentPlayingMap.end())
		return;

	AnimInfo& info = it->second;

	if (info.clipIdx < 0 || info.clipIdx >= (int)mAnimClips.size())
		return;

	// 블렌드로 재생: 시간은 유지하거나 새로 시작 정책을 선택 가능
	// 여기서는 "처음 블렌드 시작이면 0초부터 시작" 정책
	if (info.bufferWeight <= 0.0f && _blendWeight > 0.0f)
		info.elipsedTime = 0.0f;

	info.nodeIdx = _rootIdx;
	info.bufferWeight = _blendWeight;

	// 추가된 클립제외 Weight 정규화
	NormalizeWeightExcept(_name);

	mIsPlaying = true;
}
