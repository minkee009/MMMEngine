#pragma once

#include <string>
#include <optional>
#include <stdexcept>
#include <random>
#include <array>
#include <algorithm>

#include "uuid/uuid.h"

namespace MMMEngine
{
	// uuid 라이브러리를 감싸는 GUID 클래스, 함수 호출 인터페이스 제공 (정적)
	class GUID
	{
	private:
		uuids::uuid m_uuid;

	public:
		// 기본 생성자 (nil UUID)
		GUID() : m_uuid() {}

		// 내부 uuid 객체로부터 생성
		explicit GUID(const uuids::uuid& uuid) : m_uuid(uuid) {}

		// 정적 팩토리 메서드들

		// 새로운 랜덤 GUID 생성
		static GUID NewGuid() {
			std::random_device rd;
			auto seed_data = std::array<int, std::mt19937::state_size>{};
			std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
			std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
			std::mt19937 generator(seq);

			uuids::uuid_random_generator gen{ generator };
			return GUID(gen());
		}

		// Nil (빈) GUID 생성
		static GUID Empty() {
			return GUID();
		}

		// 문자열로부터 GUID 파싱
		static std::optional<GUID> Parse(const std::string& str) {
			try {
				auto uuid = uuids::uuid::from_string(str);
				if (uuid.has_value()) {
					return GUID(uuid.value());
				}
				return std::nullopt;
			}
			catch (...) {
				return std::nullopt;
			}
		}

		// 문자열로부터 GUID 파싱 (예외 던짐)
		static GUID ParseOrThrow(const std::string& str) {
			auto result = Parse(str);
			if (!result.has_value()) {
				throw std::invalid_argument("Invalid GUID string: " + str);
			}
			return result.value();
		}

		// 이름 기반 GUID 생성 (DNS 네임스페이스)
		static GUID FromName(const std::string& name) {
			static auto generator = uuids::uuid_name_generator(uuids::uuid::from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c8").value());
			return GUID(generator(name));
		}

		// 변환 메서드들

		// 문자열로 변환
		std::string ToString() const {
			return uuids::to_string(m_uuid);
		}

		// 대문자 문자열로 변환
		std::string ToUpperString() const {
			std::string str = ToString();
			for (char& c : str) {
				c = std::toupper(c);
			}
			return str;
		}

		// 하이픈 없는 문자열로 변환
		std::string ToStringWithoutHyphens() const {
			std::string str = ToString();
			str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
			return str;
		}

		// 바이트 배열로 변환
		std::vector<uint8_t> ToBytes() const {
			std::vector<uint8_t> bytes;
			bytes.reserve(16);

			auto span = m_uuid.as_bytes(); // span<const std::byte, 16>

			for (std::byte b : span) {
				bytes.push_back(static_cast<uint8_t>(b));
			}

			return bytes;
		}

		// 상태 확인

		// Nil (빈) GUID인지 확인
		bool IsEmpty() const {
			return m_uuid.is_nil();
		}

		// 유효한 GUID인지 확인
		bool IsValid() const {
			return !m_uuid.is_nil();
		}

		// 비교 연산자들

		bool operator==(const GUID& other) const {
			return m_uuid == other.m_uuid;
		}

		bool operator!=(const GUID& other) const {
			return m_uuid != other.m_uuid;
		}

		bool operator<(const GUID& other) const {
			return m_uuid < other.m_uuid;
		}

		bool operator>(const GUID& other) const {
			return other.m_uuid < m_uuid;
		}

		bool operator<=(const GUID& other) const {
			return !(other.m_uuid < m_uuid);
		}

		bool operator>=(const GUID& other) const {
			return !(m_uuid < other.m_uuid);
		}

		// 스트림 출력
		friend std::ostream& operator<<(std::ostream& os, const GUID& guid) {
			os << guid.ToString();
			return os;
		}

		// 내부 uuid 객체 접근
		const uuids::uuid& GetUuid() const {
			return m_uuid;
		}

		// 해시 지원 (unordered_map/set에서 사용)
		struct Hash {
			std::size_t operator()(const GUID& guid) const {
				return std::hash<uuids::uuid>{}(guid.m_uuid);
			}
		};
	};
} // namespace MMMEngine