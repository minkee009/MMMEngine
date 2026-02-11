#pragma once

#include "Export.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "rttr/type"

#include <SimpleMath.h>
#include <d3d11_4.h>
#include <wrl/client.h>
#include <random>
#include <vector>

namespace MMMEngine
{
	class StaticMesh;
	class Material;
	class Texture2D;

	class MMMENGINE_API ParticleRenderer : public Renderer
	{
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND

	public:
		enum class EmitShape
		{
			Cone,
			Sphere,
			Ring,
			Box
		};

		enum class EmitDirectionRange
		{
			Fixed,
			Cone,
			Hemisphere,
			Sphere,
			Box
		};

		enum class ParticleType
		{
			Quad,
			Mesh
		};

		enum class ParticleFade
		{
			None,
			Shrink,
			Fade,
			ShrinkFade
		};

	private:
		struct Particle
		{
			DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
			DirectX::SimpleMath::Vector3 velocity = DirectX::SimpleMath::Vector3::Zero;
			DirectX::SimpleMath::Quaternion rotation = DirectX::SimpleMath::Quaternion::Identity;
			DirectX::SimpleMath::Vector3 angularAxis = { 0.0f, 1.0f, 0.0f };
			float rotationZ = 0.0f;		// Quad spin (degrees)
			float angularVelocity = 0.0f; // degrees/sec
			float age = 0.0f;
			float lifetime = 1.0f;
			float scale = 1.0f;
		};

		EmitShape m_shape = EmitShape::Cone;
		EmitDirectionRange m_directionRange = EmitDirectionRange::Fixed;
		ParticleType m_particleType = ParticleType::Quad;
		ParticleFade m_fadeMode = ParticleFade::None;

		DirectX::SimpleMath::Vector3 m_emitDirection = { 0.0f, 1.0f, 0.0f };
		DirectX::SimpleMath::Vector2 m_directionAngleRange = { 0.0f, 30.0f }; // degrees
		DirectX::SimpleMath::Vector2 m_startAngleRange = { 0.0f, 0.0f }; // degrees
		DirectX::SimpleMath::Vector3 m_directionBoxRange = { 1.0f, 1.0f, 1.0f };
		float m_lifetimeMin = 1.0f;
		float m_lifetimeMax = 2.0f;
		uint32_t m_maxParticles = 100;
		float m_spawnRate = 10.0f;
		float m_angularSpeedMin = 0.0f;
		float m_angularSpeedMax = 0.0f;
		float m_linearSpeedMin = 1.0f;
		float m_linearSpeedMax = 3.0f;
		float m_scaleMin = 0.1f;
		float m_scaleMax = 0.2f;
		DirectX::SimpleMath::Vector3 m_shapeParams = { 1.0f, 1.0f, 1.0f };
		DirectX::SimpleMath::Vector3 m_spawnFormula = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector3 m_updateFormula = DirectX::SimpleMath::Vector3::Zero;

		ResPtr<Material> m_material = nullptr;
		ResPtr<StaticMesh> m_mesh = nullptr;
		ResPtr<Material> m_autoMaterial = nullptr;
		ResPtr<Texture2D> m_texture = nullptr;

		std::vector<Particle> m_particles;
		float m_spawnAccumulator = 0.0f;
		bool m_previewEnabled = false;
		bool m_playOnAwake = true;
		bool m_isPlaying = false;
		bool m_prevRuntimeActive = false;

		// One-shot 재생용 상태
		bool m_playOneShot = false;
		float m_oneShotDuration = 0.0f;
		float m_oneShotTimer = 0.0f;
		std::mt19937 m_rng;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadIB;

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;

		void UpdateSimulation(float dt, bool allowSpawn);
		void SpawnParticle(const DirectX::SimpleMath::Matrix& emitterWorld,
			const DirectX::SimpleMath::Quaternion& emitterRot);
		DirectX::SimpleMath::Vector3 SampleShape();
		DirectX::SimpleMath::Vector3 SampleDirection(const DirectX::SimpleMath::Vector3& localPos);
		DirectX::SimpleMath::Vector3 RandomDirectionInCone(const DirectX::SimpleMath::Vector3& axis, float minAngleRad, float maxAngleRad);
		DirectX::SimpleMath::Vector3 RandomUnitVector();
		float RandomRange(float min, float max);
		void EnsureAutoMaterial();

	public:
		static void BeginSceneViewRender(const DirectX::SimpleMath::Matrix& viewMatrix);
		static void EndSceneViewRender();

		EmitShape GetShape() const { return m_shape; }
		void SetShape(EmitShape shape) { m_shape = shape; }

		EmitDirectionRange GetDirectionRange() const { return m_directionRange; }
		void SetDirectionRange(EmitDirectionRange range) { m_directionRange = range; }

		ParticleType GetParticleType() const { return m_particleType; }
		void SetParticleType(ParticleType type) { m_particleType = type; }

		ParticleFade GetFadeMode() const { return m_fadeMode; }
		void SetFadeMode(ParticleFade mode) { m_fadeMode = mode; }

		const DirectX::SimpleMath::Vector3& GetEmitDirection() const { return m_emitDirection; }
		void SetEmitDirection(const DirectX::SimpleMath::Vector3& dir) { m_emitDirection = dir; }

		const DirectX::SimpleMath::Vector2& GetDirectionAngleRange() const { return m_directionAngleRange; }
		void SetDirectionAngleRange(const DirectX::SimpleMath::Vector2& range) { m_directionAngleRange = range; }

		const DirectX::SimpleMath::Vector2& GetStartAngleRange() const { return m_startAngleRange; }
		void SetStartAngleRange(const DirectX::SimpleMath::Vector2& range) { m_startAngleRange = range; }

		const DirectX::SimpleMath::Vector3& GetDirectionBoxRange() const { return m_directionBoxRange; }
		void SetDirectionBoxRange(const DirectX::SimpleMath::Vector3& range) { m_directionBoxRange = range; }

		float GetLifetimeMin() const { return m_lifetimeMin; }
		void SetLifetimeMin(float v) { m_lifetimeMin = v; }

		float GetLifetimeMax() const { return m_lifetimeMax; }
		void SetLifetimeMax(float v) { m_lifetimeMax = v; }

		uint32_t GetMaxParticles() const { return m_maxParticles; }
		void SetMaxParticles(uint32_t v) { m_maxParticles = v; }

		float GetSpawnRate() const { return m_spawnRate; }
		void SetSpawnRate(float v) { m_spawnRate = v; }

		float GetAngularSpeedMin() const { return m_angularSpeedMin; }
		void SetAngularSpeedMin(float v) { m_angularSpeedMin = v; }

		float GetAngularSpeedMax() const { return m_angularSpeedMax; }
		void SetAngularSpeedMax(float v) { m_angularSpeedMax = v; }

		float GetLinearSpeedMin() const { return m_linearSpeedMin; }
		void SetLinearSpeedMin(float v) { m_linearSpeedMin = v; }

		float GetLinearSpeedMax() const { return m_linearSpeedMax; }
		void SetLinearSpeedMax(float v) { m_linearSpeedMax = v; }

		float GetScaleMin() const { return m_scaleMin; }
		void SetScaleMin(float v) { m_scaleMin = v; }

		float GetScaleMax() const { return m_scaleMax; }
		void SetScaleMax(float v) { m_scaleMax = v; }

		const DirectX::SimpleMath::Vector3& GetShapeParams() const { return m_shapeParams; }
		void SetShapeParams(const DirectX::SimpleMath::Vector3& v) { m_shapeParams = v; }

		const DirectX::SimpleMath::Vector3& GetSpawnFormula() const { return m_spawnFormula; }
		void SetSpawnFormula(const DirectX::SimpleMath::Vector3& v) { m_spawnFormula = v; }

		const DirectX::SimpleMath::Vector3& GetUpdateFormula() const { return m_updateFormula; }
		void SetUpdateFormula(const DirectX::SimpleMath::Vector3& v) { m_updateFormula = v; }

		const ResPtr<Material>& GetMaterial() const { return m_material; }
		void SetMaterial(const ResPtr<Material>& mat) { m_material = mat; }

		const ResPtr<StaticMesh>& GetMesh() const { return m_mesh; }
		void SetMesh(const ResPtr<StaticMesh>& mesh) { m_mesh = mesh; }

		const ResPtr<Texture2D>& GetTexture() const { return m_texture; }
		void SetTexture(const ResPtr<Texture2D>& tex) { m_texture = tex; }

		bool GetPlayOnAwake() const { return m_playOnAwake; }
		void SetPlayOnAwake(bool v) { m_playOnAwake = v; }
		bool IsPlaying() const { return m_isPlaying; }

		// 루프 재생
		void Play();
		// 지정 시간 동안만 발사하는 원샷 재생
		void PlayOneShot(float duration);
		void Stop();
		void Pause();
		void Clear();

		void StopImmediate();

		bool IsPreviewing() const { return m_previewEnabled; }
		void StartPreview();
		void StopPreview();
		void PreviewBurst();
	};
}
