#pragma once
#include "ExportSingleton.hpp"
#include "ResourceManager.h"

namespace MMMEngine {
	class StaticMesh;
	class MMMENGINE_API ResourceSerializer : public Utility::ExportSingleton<ResourceSerializer>
	{
	public:
		void Serialize_StaticMesh(const StaticMesh* _in, std::wstring _path);		// 출력Path
		void DeSerialize_StaticMesh(StaticMesh* _out, std::wstring _path);			// 입력Path
	};
}
