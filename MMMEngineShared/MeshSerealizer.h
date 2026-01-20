#pragma once
#include "ExportSingleton.hpp"
#include "ResourceManager.h"

namespace MMMEngine {
	class StaticMesh;
	class MMMENGINE_API MeshSerealizer : public Utility::ExportSingleton<MeshSerealizer>
	{
	private:

	public:
		void Serealize(StaticMesh* _mesh, std::wstring _path);		// 출력Path
		void UnSerealize(StaticMesh* _mesh, std::wstring _path);	// 입력Path
	};
}


