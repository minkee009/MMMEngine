#include "ParticleRenderer.h"

#include "RenderManager.h"
#include "RenderCommand.h"
#include "GameObject.h"
#include "Transform.h"
#include "Material.h"
#include "StaticMesh.h"
#include "ShaderInfo.h"
#include "ResourceManager.h"
#include "Camera.h"
#include "TimeManager.h"
#include "GlobalRegistry.h"

#include "rttr/registration.h"

#include <algorithm>
#include <cmath>
#include <memory>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace Microsoft::WRL;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<ParticleRenderer::EmitShape>("ParticleEmitShape")
		(
			value("Cone", ParticleRenderer::EmitShape::Cone),
			value("Sphere", ParticleRenderer::EmitShape::Sphere),
			value("Ring", ParticleRenderer::EmitShape::Ring),
			value("Box", ParticleRenderer::EmitShape::Box)
		);

	registration::enumeration<ParticleRenderer::EmitDirectionRange>("ParticleDirectionRange")
		(
			value("Fixed", ParticleRenderer::EmitDirectionRange::Fixed),
			value("Cone", ParticleRenderer::EmitDirectionRange::Cone),
			value("Hemisphere", ParticleRenderer::EmitDirectionRange::Hemisphere),
			value("Sphere", ParticleRenderer::EmitDirectionRange::Sphere),
			value("Box", ParticleRenderer::EmitDirectionRange::Box)
		);

	registration::enumeration<ParticleRenderer::ParticleType>("ParticleType")
		(
			value("Quad", ParticleRenderer::ParticleType::Quad),
			value("Mesh", ParticleRenderer::ParticleType::Mesh)
		);

	registration::enumeration<ParticleRenderer::ParticleFade>("ParticleFade")
		(
			value("None", ParticleRenderer::ParticleFade::None),
			value("Shrink", ParticleRenderer::ParticleFade::Shrink),
			value("Fade", ParticleRenderer::ParticleFade::Fade),
			value("ShrinkFade", ParticleRenderer::ParticleFade::ShrinkFade)
		);

	registration::class_<ParticleRenderer>("ParticleRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<ParticleRenderer>"))
		.property("Shape", &ParticleRenderer::GetShape, &ParticleRenderer::SetShape)
		.property("ShapeParams", &ParticleRenderer::GetShapeParams, &ParticleRenderer::SetShapeParams)
		.property("EmitDirection", &ParticleRenderer::GetEmitDirection, &ParticleRenderer::SetEmitDirection)
		.property("DirectionRange", &ParticleRenderer::GetDirectionRange, &ParticleRenderer::SetDirectionRange)
			(rttr::metadata("INSPECTOR_CHAIN", "Cone=DirectionAngleRange;Box=DirectionBoxRange"))
		.property("DirectionAngleRange", &ParticleRenderer::GetDirectionAngleRange, &ParticleRenderer::SetDirectionAngleRange)
		.property("DirectionBoxRange", &ParticleRenderer::GetDirectionBoxRange, &ParticleRenderer::SetDirectionBoxRange)
		.property("LifetimeMin", &ParticleRenderer::GetLifetimeMin, &ParticleRenderer::SetLifetimeMin)
		.property("LifetimeMax", &ParticleRenderer::GetLifetimeMax, &ParticleRenderer::SetLifetimeMax)
		.property("MaxParticles", &ParticleRenderer::GetMaxParticles, &ParticleRenderer::SetMaxParticles)
		.property("SpawnRate", &ParticleRenderer::GetSpawnRate, &ParticleRenderer::SetSpawnRate)
		.property("AngularSpeedMin", &ParticleRenderer::GetAngularSpeedMin, &ParticleRenderer::SetAngularSpeedMin)
		.property("AngularSpeedMax", &ParticleRenderer::GetAngularSpeedMax, &ParticleRenderer::SetAngularSpeedMax)
		.property("StartRotationMin", &ParticleRenderer::GetStartRotationMin, &ParticleRenderer::SetStartRotationMin)
		.property("StartRotationMax", &ParticleRenderer::GetStartRotationMax, &ParticleRenderer::SetStartRotationMax)
		.property("LinearSpeedMin", &ParticleRenderer::GetLinearSpeedMin, &ParticleRenderer::SetLinearSpeedMin)
		.property("LinearSpeedMax", &ParticleRenderer::GetLinearSpeedMax, &ParticleRenderer::SetLinearSpeedMax)
		.property("ScaleMin", &ParticleRenderer::GetScaleMin, &ParticleRenderer::SetScaleMin)
		.property("ScaleMax", &ParticleRenderer::GetScaleMax, &ParticleRenderer::SetScaleMax)
		.property("SpawnFormula", &ParticleRenderer::GetSpawnFormula, &ParticleRenderer::SetSpawnFormula)
		.property("UpdateFormula", &ParticleRenderer::GetUpdateFormula, &ParticleRenderer::SetUpdateFormula)
		.property("Color", &ParticleRenderer::GetParticleColor, &ParticleRenderer::SetParticleColor)
		.property("ParticleType", &ParticleRenderer::GetParticleType, &ParticleRenderer::SetParticleType)
			(rttr::metadata("INSPECTOR_CHAIN", "Quad=Texture;Mesh=Mesh,Material"))
		.property("FadeMode", &ParticleRenderer::GetFadeMode, &ParticleRenderer::SetFadeMode)
		.property("Material", &ParticleRenderer::GetMaterial, &ParticleRenderer::SetMaterial)
		.property("Mesh", &ParticleRenderer::GetMesh, &ParticleRenderer::SetMesh)
		.property("Texture", &ParticleRenderer::GetTexture, &ParticleRenderer::SetTexture);

	registration::class_<ObjPtr<ParticleRenderer>>("ObjPtr<ParticleRenderer>")
		.constructor<>([]() { return Object::NewObject<ParticleRenderer>(); })
		.method("Inject", &ObjPtr<ParticleRenderer>::Inject);
}

namespace
{
	inline float ClampMin(float v, float minV)
	{
		return (v < minV) ? minV : v;
	}

	inline float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	inline float DegToRad(float deg)
	{
		return deg * (XM_PI / 180.0f);
	}
}

namespace MMMEngine
{
	void ParticleRenderer::SetParticleColor(const Vector4& v)
	{
		Vector4 color = v;
		const float maxRgb = std::max(color.x, std::max(color.y, color.z));
		const bool rgbIs255 = (maxRgb > 1.0f + 1e-4f);
		if (rgbIs255)
		{
			color.x /= 255.0f;
			color.y /= 255.0f;
			color.z /= 255.0f;
		}

		if (color.w > 1.0f + 1e-4f)
			color.w /= 255.0f;

		color.x = Clamp01(color.x);
		color.y = Clamp01(color.y);
		color.z = Clamp01(color.z);
		color.w = Clamp01(color.w);

		m_particleColor = color;
	}

	void ParticleRenderer::Initialize()
	{
		renderIndex = RenderManager::Get().AddRenderer(this);

		std::random_device rd;
		m_rng = std::mt19937(rd());

		auto device = RenderManager::Get().GetDevice();
		if (!device.Get())
			return;

		if (!m_quadVB.Get() || !m_quadIB.Get())
		{
			std::vector<Mesh_Vertex> verts(4);
			verts[0].Pos = { -0.5f, -0.5f, 0.0f };
			verts[1].Pos = {  0.5f, -0.5f, 0.0f };
			verts[2].Pos = {  0.5f,  0.5f, 0.0f };
			verts[3].Pos = { -0.5f,  0.5f, 0.0f };

			for (auto& v : verts)
			{
				v.Normal = { 0.0f, 0.0f, 1.0f };
				v.Tangent = { 1.0f, 0.0f, 0.0f };
			}
			verts[0].UV = { 0.0f, 1.0f };
			verts[1].UV = { 1.0f, 1.0f };
			verts[2].UV = { 1.0f, 0.0f };
			verts[3].UV = { 0.0f, 0.0f };

			std::vector<UINT> indices = { 0, 1, 2, 0, 2, 3 };

			D3D11_BUFFER_DESC vbd = {};
			vbd.Usage = D3D11_USAGE_DEFAULT;
			vbd.ByteWidth = static_cast<UINT>(sizeof(Mesh_Vertex) * verts.size());
			vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA vinit = {};
			vinit.pSysMem = verts.data();

			device->CreateBuffer(&vbd, &vinit, m_quadVB.GetAddressOf());

			D3D11_BUFFER_DESC ibd = {};
			ibd.Usage = D3D11_USAGE_DEFAULT;
			ibd.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());
			ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA iinit = {};
			iinit.pSysMem = indices.data();

			device->CreateBuffer(&ibd, &iinit, m_quadIB.GetAddressOf());
		}
	}

	void ParticleRenderer::UnInitialize()
	{
		RenderManager::Get().RemoveRenderer(renderIndex);
		m_particles.clear();
		m_material.reset();
		m_mesh.reset();
		m_autoMaterial.reset();
		m_quadVB.Reset();
		m_quadIB.Reset();
	}

	void ParticleRenderer::StartPreview()
	{
		m_previewEnabled = true;
		m_spawnAccumulator = 0.0f;
	}

	void ParticleRenderer::StopPreview()
	{
		m_previewEnabled = false;
		m_spawnAccumulator = 0.0f;
		m_particles.clear();
	}

	void ParticleRenderer::PreviewBurst()
	{
		auto tr = GetTransform();
		if (!tr)
			return;

		const Matrix emitterWorld = tr->GetWorldMatrix();
		const Quaternion emitterRot = tr->GetWorldRotation();

		uint32_t count = m_maxParticles;
		for (uint32_t i = 0; i < count && m_particles.size() < m_maxParticles; ++i)
			SpawnParticle(emitterWorld, emitterRot);
	}

	void ParticleRenderer::Render()
	{
		auto tr = GetTransform();
		if (!tr)
			return;

		const bool allowSim = m_previewEnabled ||
			(GlobalRegistry::g_runtimeActive && !RenderManager::Get().IsSceneViewPass());
		const float dt = TimeManager::Get().GetDeltaTime();
		if (allowSim && dt > 0.0f)
			UpdateSimulation(dt, true);

		if (m_particles.empty())
			return;

		ResPtr<Material> useMaterial = (m_particleType == ParticleType::Quad) ? nullptr : m_material;
		if (m_particleType == ParticleType::Quad && !useMaterial)
		{
			EnsureAutoMaterial();
			useMaterial = m_autoMaterial;
			if (!useMaterial)
				return;
		}

		if (m_particleType == ParticleType::Mesh && !m_mesh)
			return;

		if (m_particleType == ParticleType::Quad && useMaterial && m_texture)
		{
			const auto& props = useMaterial->GetProperties();
			if (props.find(L"_albedo") != props.end())
				useMaterial->SetProperty(L"_albedo", m_texture);
			else
				useMaterial->AddProperty(L"_albedo", m_texture);
		}

		const bool isQuad = (m_particleType == ParticleType::Quad);
		if (isQuad && useMaterial)
		{
			const auto& props = useMaterial->GetProperties();
			auto it = props.find(L"mBaseColor");
			if (it != props.end())
			{
				if (std::get_if<Vector4>(&it->second))
					useMaterial->SetProperty(L"mBaseColor", m_particleColor);
				else if (std::get_if<Vector3>(&it->second))
					useMaterial->SetProperty(L"mBaseColor", Vector4(m_particleColor.x, m_particleColor.y, m_particleColor.z, m_particleColor.w));
				else
				{
					useMaterial->RemoveProperty(L"mBaseColor");
					useMaterial->AddProperty(L"mBaseColor", m_particleColor);
				}
			}
			else
			{
				useMaterial->AddProperty(L"mBaseColor", m_particleColor);
			}
		}

		Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		if (useMaterial)
		{
			const auto& props = useMaterial->GetProperties();
			auto it = props.find(L"mBaseColor");
			if (it != props.end())
			{
				if (auto col4 = std::get_if<Vector4>(&it->second))
					baseColor = *col4;
				else if (auto col3 = std::get_if<Vector3>(&it->second))
					baseColor = { col3->x, col3->y, col3->z, 1.0f };
			}
		}

		const Vector4 tint = m_particleColor;
		const Vector4 tintedBase = {
			baseColor.x * tint.x,
			baseColor.y * tint.y,
			baseColor.z * tint.z,
			baseColor.w * tint.w
		};
		const bool wantsColorOverride = !isQuad && ((std::fabs(tint.x - 1.0f) > 1e-4f) ||
			(std::fabs(tint.y - 1.0f) > 1e-4f) ||
			(std::fabs(tint.z - 1.0f) > 1e-4f) ||
			(std::fabs(tint.w - 1.0f) > 1e-4f) ||
			(m_fadeMode == ParticleFade::Fade || m_fadeMode == ParticleFade::ShrinkFade));
		const bool wantsAlphaOverride = isQuad &&
			(m_fadeMode == ParticleFade::Fade || m_fadeMode == ParticleFade::ShrinkFade);

		Matrix view = RenderManager::Get().GetViewMatrix();
		Matrix invView = view.Invert();
		Vector3 camPos = invView.Translation();
		Vector3 camUp = Vector3::TransformNormal(Vector3::Up, invView);
		Vector3 camForward = Vector3::TransformNormal(Vector3::Forward, invView);
		if (camUp.LengthSquared() < 0.0001f)
			camUp = Vector3::Up;
		if (camForward.LengthSquared() < 0.0001f)
			camForward = Vector3::Forward;
		camUp.Normalize();
		camForward.Normalize();

		for (auto& p : m_particles)
		{
			float lifeT = (p.lifetime > 0.0f) ? (p.age / p.lifetime) : 1.0f;
			lifeT = Clamp01(lifeT);

			float alpha = 1.0f;
			float scaleMul = 1.0f;
			if (m_fadeMode == ParticleFade::Fade || m_fadeMode == ParticleFade::ShrinkFade)
				alpha = 1.0f - lifeT;
			if (m_fadeMode == ParticleFade::Shrink || m_fadeMode == ParticleFade::ShrinkFade)
				scaleMul = 1.0f - lifeT;

			const float finalScale = p.scale * scaleMul;
			if (finalScale <= 0.0001f || alpha <= 0.0001f)
				continue;

			RenderCommand cmd;
			cmd.rendererID = renderIndex;
			cmd.castShadow = false;
			cmd.receiveShadow = false;
			cmd.particleAlpha = alpha;
			cmd.useParticleAlpha = wantsAlphaOverride;
			cmd.forceAlphaClipOff = (m_fadeMode == ParticleFade::Fade || m_fadeMode == ParticleFade::ShrinkFade);
			if (wantsColorOverride)
			{
				Vector4 finalColor = tintedBase;
				finalColor.w *= alpha;
				cmd.useParticleColor = true;
				cmd.particleColor = finalColor;
			}

			cmd.camDistance = Vector3::Distance(camPos, p.position);

			if (m_particleType == ParticleType::Quad)
			{
				if (!m_quadVB.Get() || !m_quadIB.Get())
					continue;

				cmd.vertexBuffer = m_quadVB.Get();
				cmd.indexBuffer = m_quadIB.Get();
				cmd.indiciesSize = 6;
				cmd.material = useMaterial;

				Matrix billboard = Matrix::CreateWorld(p.position, -camForward, camUp);
				Matrix spin = Matrix::CreateRotationZ(DegToRad(p.rotationZ));
				Matrix world = Matrix::CreateScale(finalScale) * spin * billboard;

				cmd.worldMatIndex = RenderManager::Get().AddMatrix(world);
				RenderManager::Get().AddCommand(RenderType::R_PARTICLE, std::move(cmd));
			}
			else
			{
				if (!m_mesh || m_mesh->gpuBuffer.vertexBuffers.empty())
					continue;
				if (!useMaterial)
					continue;

				Matrix world = Matrix::CreateScale(finalScale) *
					Matrix::CreateFromQuaternion(p.rotation) *
					Matrix::CreateTranslation(p.position);

				const int worldIdx = RenderManager::Get().AddMatrix(world);

				for (auto& [matIdx, meshIndices] : m_mesh->meshGroupData)
				{
					ResPtr<Material> mat = useMaterial;
					if (!mat)
						continue;

					for (const auto& idx : meshIndices)
					{
						RenderCommand meshCmd = cmd;
						meshCmd.vertexBuffer = m_mesh->gpuBuffer.vertexBuffers[idx].Get();
						meshCmd.indexBuffer = m_mesh->gpuBuffer.indexBuffers[idx].Get();
						meshCmd.indiciesSize = m_mesh->indexSizes[idx];
						meshCmd.material = mat;
						meshCmd.worldMatIndex = worldIdx;
						RenderManager::Get().AddCommand(RenderType::R_PARTICLE, std::move(meshCmd));
					}
				}
			}
		}
	}

	void ParticleRenderer::UpdateSimulation(float dt, bool allowSpawn)
	{
		if (allowSpawn && m_spawnRate > 0.0f)
		{
			m_spawnAccumulator += m_spawnRate * dt;
			const int spawnCount = static_cast<int>(m_spawnAccumulator);
			if (spawnCount > 0)
				m_spawnAccumulator -= static_cast<float>(spawnCount);

			auto tr = GetTransform();
			if (tr)
			{
				const Matrix emitterWorld = tr->GetWorldMatrix();
				const Quaternion emitterRot = tr->GetWorldRotation();

				for (int i = 0; i < spawnCount && m_particles.size() < m_maxParticles; ++i)
					SpawnParticle(emitterWorld, emitterRot);
			}
		}

		for (auto& p : m_particles)
		{
			p.age += dt;
			p.velocity += m_updateFormula * dt;
			p.position += p.velocity * dt;

			p.rotationZ += p.angularVelocity * dt;
			if (m_particleType == ParticleType::Mesh)
			{
				if (p.angularAxis.LengthSquared() > 0.0001f)
				{
					Quaternion delta = Quaternion::CreateFromAxisAngle(p.angularAxis, DegToRad(p.angularVelocity * dt));
					p.rotation = delta * p.rotation;
					p.rotation.Normalize();
				}
			}
		}

		m_particles.erase(
			std::remove_if(m_particles.begin(), m_particles.end(),
				[](const Particle& p) { return p.age >= p.lifetime; }),
			m_particles.end());
	}

	void ParticleRenderer::SpawnParticle(const Matrix& emitterWorld, const Quaternion& emitterRot)
	{
		Particle p;
		p.age = 0.0f;

		float lifetimeMin = ClampMin(m_lifetimeMin, 0.01f);
		float lifetimeMax = ClampMin(m_lifetimeMax, 0.01f);
		if (lifetimeMax < lifetimeMin)
			std::swap(lifetimeMin, lifetimeMax);
		p.lifetime = RandomRange(lifetimeMin, lifetimeMax);

		float scaleMin = std::min(m_scaleMin, m_scaleMax);
		float scaleMax = std::max(m_scaleMin, m_scaleMax);
		p.scale = RandomRange(scaleMin, scaleMax);

		const Vector3 localPos = SampleShape();
		p.position = Vector3::Transform(localPos, emitterWorld);

		Vector3 localDir = SampleDirection(localPos);

		Matrix rotMat = Matrix::CreateFromQuaternion(emitterRot);
		Vector3 worldDir = Vector3::TransformNormal(localDir, rotMat);
		if (worldDir.LengthSquared() < 0.0001f)
			worldDir = Vector3::Up;
		worldDir.Normalize();

		float speedMin = std::min(m_linearSpeedMin, m_linearSpeedMax);
		float speedMax = std::max(m_linearSpeedMin, m_linearSpeedMax);
		const float speed = RandomRange(speedMin, speedMax);
		p.velocity = worldDir * speed + m_spawnFormula;

		float angMin = std::min(m_angularSpeedMin, m_angularSpeedMax);
		float angMax = std::max(m_angularSpeedMin, m_angularSpeedMax);
		p.angularVelocity = RandomRange(angMin, angMax);

		float rotMin = std::min(m_startRotationMin, m_startRotationMax);
		float rotMax = std::max(m_startRotationMin, m_startRotationMax);
		p.rotationZ = RandomRange(rotMin, rotMax);
		p.rotation = emitterRot;
		p.angularAxis = RandomUnitVector();

		m_particles.push_back(p);
	}

	Vector3 ParticleRenderer::SampleShape()
	{
		switch (m_shape)
		{
		case EmitShape::Sphere:
		{
			const float radius = std::max(0.0f, m_shapeParams.x);
			Vector3 dir = Vector3::Zero;
			float lenSq = 0.0f;
			do
			{
				dir.x = RandomRange(-1.0f, 1.0f);
				dir.y = RandomRange(-1.0f, 1.0f);
				dir.z = RandomRange(-1.0f, 1.0f);
				lenSq = dir.LengthSquared();
			} while (lenSq > 1.0f || lenSq < 0.0001f);

			dir.Normalize();
			float dist = RandomRange(0.0f, radius);
			return dir * dist;
		}
		case EmitShape::Ring:
		{
			const float major = std::max(0.0f, m_shapeParams.x);
			const float minor = std::max(0.0f, m_shapeParams.y);
			const float angle = RandomRange(0.0f, XM_2PI);
			const float tube = RandomRange(-minor, minor);
			const float x = (major + tube) * std::cos(angle);
			const float z = (major + tube) * std::sin(angle);
			return { x, 0.0f, z };
		}
		case EmitShape::Box:
		{
			const Vector3 half = m_shapeParams * 0.5f;
			return {
				RandomRange(-half.x, half.x),
				RandomRange(-half.y, half.y),
				RandomRange(-half.z, half.z)
			};
		}
		case EmitShape::Cone:
		default:
		{
			const float radius = std::max(0.0f, m_shapeParams.x);
			const float height = std::max(0.0f, m_shapeParams.y);
			const float h = RandomRange(0.0f, height);
			const float r = (height > 0.0001f) ? radius * (h / height) : 0.0f;
			const float angle = RandomRange(0.0f, XM_2PI);
			return { r * std::cos(angle), h, r * std::sin(angle) };
		}
		}
	}

	Vector3 ParticleRenderer::SampleDirection(const Vector3& localPos)
	{
		Vector3 axis = m_emitDirection;
		if (axis.LengthSquared() < 0.0001f)
		{
			if (localPos.LengthSquared() > 0.0001f)
				axis = localPos;
			else
				axis = Vector3::Up;
		}
		axis.Normalize();

		switch (m_directionRange)
		{
		case EmitDirectionRange::Fixed:
			return axis;
		case EmitDirectionRange::Cone:
		{
			float minAngle = std::min(m_directionAngleRange.x, m_directionAngleRange.y);
			float maxAngle = std::max(m_directionAngleRange.x, m_directionAngleRange.y);
			minAngle = std::clamp(minAngle, 0.0f, 180.0f);
			maxAngle = std::clamp(maxAngle, 0.0f, 180.0f);
			return RandomDirectionInCone(axis, DegToRad(minAngle), DegToRad(maxAngle));
		}
		case EmitDirectionRange::Hemisphere:
			return RandomDirectionInCone(axis, 0.0f, XM_PIDIV2);
		case EmitDirectionRange::Sphere:
			return RandomUnitVector();
		case EmitDirectionRange::Box:
		{
			Vector3 v = {
				RandomRange(-m_directionBoxRange.x, m_directionBoxRange.x),
				RandomRange(-m_directionBoxRange.y, m_directionBoxRange.y),
				RandomRange(-m_directionBoxRange.z, m_directionBoxRange.z)
			};
			if (v.LengthSquared() < 0.0001f)
				v = axis;
			else
				v.Normalize();
			return v;
		}
		default:
			return axis;
		}
	}

	Vector3 ParticleRenderer::RandomDirectionInCone(const Vector3& axis, float minAngleRad, float maxAngleRad)
	{
		const float minCos = std::cos(std::clamp(maxAngleRad, 0.0f, XM_PI));
		const float maxCos = std::cos(std::clamp(minAngleRad, 0.0f, XM_PI));

		const float u = RandomRange(0.0f, 1.0f);
		const float cosTheta = minCos + (maxCos - minCos) * u;
		const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
		const float phi = RandomRange(0.0f, XM_2PI);

		Vector3 ref = (std::abs(axis.y) < 0.999f) ? Vector3::Up : Vector3::Right;

		// axis x ref
		Vector3 right = axis.Cross(ref);
		if (right.LengthSquared() < 0.0001f)
			right = Vector3::Right;
		else
			right.Normalize();

		// right x axis
		Vector3 up = right.Cross(axis);
		up.Normalize();

		Vector3 dir =
			right * (sinTheta * std::cos(phi)) +
			up * (sinTheta * std::sin(phi)) +
			axis * cosTheta;

		dir.Normalize();
		return dir;
	}

	Vector3 ParticleRenderer::RandomUnitVector()
	{
		float z = RandomRange(-1.0f, 1.0f);
		float t = RandomRange(0.0f, XM_2PI);
		float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
		return { r * std::cos(t), z, r * std::sin(t) };
	}

	void ParticleRenderer::EnsureAutoMaterial()
	{
		const std::wstring unlitPath = L"Shader/Particle/ParticleUnlitPS.hlsl";
		const std::wstring unlitFallback = L"../Common/Shader/Particle/ParticleUnlitPS.hlsl";
		if (m_autoMaterial)
		{
			auto ps = m_autoMaterial->GetPShader();
			if (ps)
			{
				const std::wstring& pPath = ps->GetFilePath();
				if (pPath == unlitPath || pPath == unlitFallback)
					return;
			}
		}

		auto mat = std::make_shared<Material>();
		auto vShader = ShaderInfo::Get().GetDefaultVShader();
		auto pShader = ResourceManager::Get().Load<PShader>(unlitPath);
		if (!pShader)
			pShader = ResourceManager::Get().Load<PShader>(unlitFallback);
		if (!pShader)
			pShader = ShaderInfo::Get().GetDefaultPShader();

		if (vShader)
			mat->SetVShader(vShader);
		if (pShader)
			mat->SetPShader(pShader);

		if (pShader)
		{
			const auto shaderType = ShaderInfo::Get().GetShaderType(pShader->GetFilePath());
			ShaderInfo::Get().ConvertMaterialType(shaderType, mat.get());
		}

		mat->SetSurfaceType(Material::SurfaceType::Transparent);
		mat->AddProperty(L"mBaseColor", Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		mat->AddProperty(L"mUseAlphaClip", 0.0f);

		m_autoMaterial = std::move(mat);
	}

	float ParticleRenderer::RandomRange(float min, float max)
	{
		if (max < min)
			std::swap(min, max);
		std::uniform_real_distribution<float> dist(min, max);
		return dist(m_rng);
	}
}
