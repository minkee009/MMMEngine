#pragma once
#include "ScriptBehaviour.h"

namespace MMMEngine
{
	class MMMENGINE_API MissingScriptBehaviour : public ScriptBehaviour
	{
	private:
		RTTR_ENABLE(ScriptBehaviour)
		RTTR_REGISTRATION_FRIEND


		std::string m_originalTypeName;
		std::vector<uint8_t> m_originalPropsMsgPack; // Props 전체 msgpack (손실 0)
	public:
		MissingScriptBehaviour() = default;
		virtual ~MissingScriptBehaviour() = default;
		void SetOriginalTypeName(const std::string& n) { m_originalTypeName = n; }
		const std::string& GetOriginalTypeName() const { return m_originalTypeName; }

		void SetOriginalPropsMsgPack(std::vector<uint8_t> data) { m_originalPropsMsgPack = std::move(data); }
		const std::vector<uint8_t>& GetOriginalPropsMsgPack() const { return m_originalPropsMsgPack; }

		bool HasOriginalProps() const { return !m_originalPropsMsgPack.empty(); }
		void ClearOriginalProps() { m_originalPropsMsgPack.clear(); }
	};
}
