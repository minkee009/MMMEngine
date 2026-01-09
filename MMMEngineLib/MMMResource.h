#pragma once
#include <memory>

namespace MMMEngine
{
	template <typename T>
	using ResPtr = std::shared_ptr<T>;
}