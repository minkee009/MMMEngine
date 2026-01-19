#pragma once

#include "Export.h"
#include "Behaviour.h"

namespace MMMEngine {
	class MMMENGINE_API MeshRenderer : public Behaviour
	{
		// GPU ¹öÆÛ
		std::shared_ptr<StaticMesh> mesh = nullptr;
		std::vector<std::weak_ptr<Renderer>> renderers;

		void SetMesh(std::shared_ptr<StaticMesh>& _mesh);
		void Start() override;
		void Update() override;

		~MeshRenderer();
	};
}

