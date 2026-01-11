#pragma once
#include "IBytesProvider.h"

namespace MMMEngine::Player
{
    class PakFileReader;
    class PakBytesProvider final : public MMMEngine::IBytesProvider
    {
    public:
        explicit PakBytesProvider(PakFileReader* reader) : m_reader(reader) {}
        bool ReadAll(const MMMEngine::AssetEntry& entry, std::vector<uint8_t>& outBytes) override;

    private:
        PakFileReader* m_reader = nullptr;
    };
}
