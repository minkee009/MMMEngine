#include "pch.h"
#include "PhysicsSettings.h"
#include "json/json.hpp"
#include <fstream>
#include "PhysxManager.h"

DEFINE_SINGLETON(MMMEngine::PhysicsSettings)

namespace fs = std::filesystem;
using json = nlohmann::json;


bool MMMEngine::PhysicsSettings::SaveSettings()
{
    // 확장자를 .settings로 강제하거나 확인 (선택 사항)
    fs::path savePath = m_configFilePath / SETTINGS_FILENAME;
    if (savePath.extension() != ".settings") {
        savePath.replace_extension(".settings");
    }

    try {
        if (savePath.has_parent_path() && !fs::exists(savePath.parent_path())) {
            fs::create_directories(savePath.parent_path());
        }

        json j;
        j["LayerNames"] = idToName;

        std::vector<uint32_t> matrixVec(collisionMatrix, collisionMatrix + 32);
        j["CollisionMatrix"] = matrixVec;

        // 1. JSON을 MessagePack 바이너리로 변환
        std::vector<std::uint8_t> v_msgpack = json::to_msgpack(j);

        // 2. 바이너리 모드로 저장
        std::ofstream file(savePath, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(v_msgpack.data()), v_msgpack.size());
            return true;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "PhysicsSettings Save Error (MsgPack): " << e.what() << std::endl;
    }
    return false;
}

bool MMMEngine::PhysicsSettings::LoadSettings()
{
    if (!fs::exists(m_configFilePath / SETTINGS_FILENAME)) return false;

    try {
        std::ifstream file(m_configFilePath / SETTINGS_FILENAME, std::ios::binary);
        if (!file.is_open()) return false;

        // 파일 내용을 벡터로 읽기
        std::vector<std::uint8_t> v_msgpack((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        // 1. MessagePack 바이너리를 JSON으로 변환
        json j = json::from_msgpack(v_msgpack);

        if (j.contains("LayerNames")) {
            idToName = j["LayerNames"].get<std::map<uint32_t, std::string>>();
        }

        if (j.contains("CollisionMatrix")) {
            auto matrixVec = j["CollisionMatrix"].get<std::vector<uint32_t>>();
            for (size_t i = 0; i < 32 && i < matrixVec.size(); ++i) {
                collisionMatrix[i] = matrixVec[i];
            }
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "PhysicsSettings Load Error (MsgPack): " << e.what() << std::endl;
    }
    return false;
}

void MMMEngine::PhysicsSettings::ApplySettings()
{
    auto& physx = MMMEngine::PhysxManager::Get(); // 싱글톤 접근

    for (uint32_t i = 0; i < 32; ++i) {
        for (uint32_t j = 0; j <= i; ++j) { // 대칭이므로 하삼각형만 순회해도 충분
            bool canCollide = (collisionMatrix[i] & (1u << j)) != 0;

            // PhysxManager에 비트 정보 전달
            physx.SetLayerCollision(i, j, canCollide);
        }
    }
}
