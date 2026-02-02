#include "RenderShared.h"
#include "rttr/registration.h"

#pragma warning(disable: 4506)

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MMMEngine::Mesh_Vertex>("Mesh_Vertex")
		.constructor<>()
		.property("Pos", &Mesh_Vertex::Pos)
		.property("Normal", &Mesh_Vertex::Normal)
		.property("Tangent", &Mesh_Vertex::Tangent)
		.property("UV", &Mesh_Vertex::UV)
		.property("BoneIndices", &Mesh_Vertex::BoneIndices)
		.property("BoneWeights", &Mesh_Vertex::BoneWeights);

	registration::class_<MMMEngine::Mesh_BoneBuffer>("Mesh_BoneBuffer")
		.constructor<>()
		.property("BoneMat", &Mesh_BoneBuffer::BoneMat);

	registration::class_<MMMEngine::MeshData>("MeshData")
		.property("vertices", &MeshData::vertices);

	registration::class_<MMMEngine::Mesh_AnimTrack>("Mesh_AnimTrack")
		.constructor<>()
		.property("nodeIndex", &Mesh_AnimTrack::nodeIndex)
		.property("posKeys", &Mesh_AnimTrack::posKeys)
		.property("rotKeys", &Mesh_AnimTrack::rotKeys)
		.property("scaleKeys", &Mesh_AnimTrack::scaleKeys);

	registration::class_<MMMEngine::Mesh_VecKey>("Mesh_VecKey")
		.constructor<>()
		.property("timeSec", &Mesh_VecKey::timeSec)
		.property("value", &Mesh_VecKey::value);

	registration::class_<MMMEngine::Mesh_QuatKey>("Mesh_QuatKey")
		.constructor<>()
		.property("timeSec", &Mesh_QuatKey::timeSec)
		.property("value", &Mesh_QuatKey::value);
}
