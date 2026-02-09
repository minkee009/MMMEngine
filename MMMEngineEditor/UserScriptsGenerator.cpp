#include "UserScriptsGenerator.h"
#include "UserScriptMessageSignatures.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    namespace
    {
        static size_t ComputeFileHash(const fs::path& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return 0;

            // 파일 전체를 문자열로 읽음
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            // 표준 해시 함수 사용
            return std::hash<std::string>{}(content);
        }

        static std::string StripUtf8Bom(std::string text)
        {
            if (text.size() >= 3 &&
                static_cast<unsigned char>(text[0]) == 0xEF &&
                static_cast<unsigned char>(text[1]) == 0xBB &&
                static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }
            return text;
        }

        static std::string NormalizeToCrlf(std::string text)
        {
            text = StripUtf8Bom(std::move(text));
            std::string out;
            out.reserve(text.size() + text.size() / 16);
            for (size_t i = 0; i < text.size(); ++i)
            {
                char c = text[i];
                if (c == '\r')
                {
                    if (i + 1 < text.size() && text[i + 1] == '\n')
                        ++i;
                    out += "\r\n";
                }
                else if (c == '\n')
                {
                    out += "\r\n";
                }
                else
                {
                    out += c;
                }
            }
            return out;
        }

        static bool WriteUtf8BomFile(const fs::path& path, const std::string& text)
        {
            const std::string normalized = NormalizeToCrlf(text);

            // [수정] 파일이 이미 존재한다면 내용을 먼저 읽어 비교함
            if (fs::exists(path))
            {
                std::ifstream in(path, std::ios::binary);
                if (in)
                {
                    // BOM 제거 후 기존 내용 읽기
                    std::string existing((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    if (StripUtf8Bom(existing) == normalized)
                    {
                        return true; // 내용이 완전히 같으면 쓰지 않고 리턴 (타임스탬프 보존)
                    }
                }
            }

            std::ofstream out(path, std::ios::binary);
            if (!out) return false;

            static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            out.write(reinterpret_cast<const char*>(bom), sizeof(bom));
            out.write(normalized.data(), static_cast<std::streamsize>(normalized.size()));
            return static_cast<bool>(out);
        }

        // 클래스 이름을 키로, 상대 헤더 경로를 값으로 가지는 '파일 DB'
        using ScriptDB = std::unordered_map<std::string, fs::path>;

        struct MessageInfo
        {
            std::string messageName;  // BroadCast 시 사용할 이름
            std::string cppName;      // 함수 이름
            std::vector<std::string> paramTypes;
        };

        struct PropertyInfo
        {
            std::string name;
            std::string type;
            bool inspectorHidden = false;
            std::string inspectorChain;
            std::string range;
        };

        struct EnumInfo
        {
            std::string name;
            bool isScoped = false;
            std::vector<std::string> values;
        };

        struct FileStamp {
            long long writeTime = 0;
            long long fileSize = 0;
            size_t contentHash = 0; // [추가] 내용 기반 비교를 위한 해시값
        };

        struct ScriptInfo {
            std::string className;
            fs::path headerPath;        // Scripts 폴더 기준 상대 경로 (서브폴더 포함)
            bool dontAutogen = false;
            std::vector<MessageInfo> messages;
            std::vector<PropertyInfo> properties;
            std::vector<EnumInfo> enums;
            std::set<std::string> dependencies; // ObjPtr<T> 등에서 추출된 의존 클래스 이름
        };


        // ObjPtr<T> 패턴을 찾아 의존성 리스트에 추가하는 로직
        static void ExtractDependencies(const std::string& classBody, MMMEngine::Editor::ScriptInfo& info) {
            // ObjPtr<Type> 패턴을 찾아 Type을 추출
                // 공백(\s*) 처리를 포함하여 정확하게 클래스명만 캡처합니다.
            std::regex ptrRegex(R"re(ObjPtr\s*<\s*([A-Za-z0-9_]+)\s*>)re");

            auto words_begin = std::sregex_iterator(classBody.begin(), classBody.end(), ptrRegex);
            auto words_end = std::sregex_iterator();

            for (std::sregex_iterator i = words_begin; i != words_end; ++i)
            {
                std::string tType = (*i)[1].str();

                // 자기 자신은 인클루드할 필요 없으므로 제외
                if (tType != info.className)
                {
                    info.dependencies.insert(tType);
                }
            }
        }

        static void ExtractDependenciesFromType(const std::string& typeName, ScriptInfo& info)
        {
            static const std::set<std::string> kIgnored = {
                "const", "volatile", "signed", "unsigned",
                "short", "long", "int", "float", "double",
                "char", "bool", "void", "class", "struct", "enum",
                "auto", "typename", "template"
            };

            std::regex tokenRegex(R"re([A-Za-z_][A-Za-z0-9_]*)re");
            auto begin = std::sregex_iterator(typeName.begin(), typeName.end(), tokenRegex);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it)
            {
                const std::string token = (*it)[0].str();
                if (token == info.className)
                    continue;
                if (kIgnored.count(token))
                    continue;
                info.dependencies.insert(token);
            }
        }

        static int64_t ToStampTime(const fs::file_time_type& time)
        {
            return static_cast<int64_t>(time.time_since_epoch().count());
        }

        static bool GetFileStamp(const fs::path& path, FileStamp& outStamp)
        {
            std::error_code ec;
            if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec))
                return false;
            const auto t = fs::last_write_time(path, ec);
            if (ec) return false;
            const auto s = fs::file_size(path, ec);
            if (ec) return false;
            outStamp.writeTime = ToStampTime(t);
            outStamp.fileSize = s;
            return true;
        }

        static size_t HashCombine(size_t seed, size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }

        static size_t ComputeEngineSignatureHash(const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs)
        {
            size_t h = 0;
            std::hash<std::string> hs;
            for (const auto& sig : engineSigs)
            {
                h = HashCombine(h, hs(sig.messageName));
                for (const auto& p : sig.paramTypes)
                    h = HashCombine(h, hs(p));
            }
            return h;
        }

        static fs::path GetCachePath(const fs::path& scriptsDir)
        {
            return scriptsDir / "UserScripts.gen.cache";
        }

        static bool LoadCache(
            const fs::path& scriptsDir,
            size_t& outSigHash,
            std::unordered_map<std::string, FileStamp>& outMap)
        {
            outMap.clear();
            outSigHash = 0;

            fs::path cachePath = GetCachePath(scriptsDir);
            std::ifstream in(cachePath, std::ios::binary);
            if (!in) return false;

            std::string line;
            bool hasVersion = false;
            bool hasSig = false;
            while (std::getline(in, line))
            {
                if (line.empty())
                    continue;
                if (!hasVersion)
                {
                    if (line != "version=1")
                        return false;
                    hasVersion = true;
                    continue;
                }
                if (!hasSig)
                {
                    const std::string prefix = "sigHash=";
                    if (line.rfind(prefix, 0) != 0)
                        return false;
                    outSigHash = static_cast<size_t>(std::stoull(line.substr(prefix.size())));
                    hasSig = true;
                    continue;
                }

                const size_t first = line.find('|');
                const size_t second = (first == std::string::npos) ? std::string::npos : line.find('|', first + 1);
                const size_t third = (second == std::string::npos) ? std::string::npos : line.find('|', second + 1);
                if (first == std::string::npos || second == std::string::npos || third == std::string::npos)
                    continue;
                std::string relPath = line.substr(0, first);
                FileStamp stamp;
                stamp.writeTime = std::stoll(line.substr(first + 1, second - first - 1));
                stamp.fileSize = static_cast<uintmax_t>(std::stoull(line.substr(second + 1)));
                stamp.contentHash = static_cast<size_t>(std::stoull(line.substr(third + 1))); // 해시 읽기 추가
                outMap.emplace(std::move(relPath), stamp);
            }

            return hasVersion && hasSig;
        }

        static bool SaveCache(
            const fs::path& scriptsDir,
            size_t sigHash,
            const std::unordered_map<std::string, FileStamp>& map)
        {
            fs::path cachePath = GetCachePath(scriptsDir);
            std::ofstream out(cachePath, std::ios::binary);
            if (!out) return false;

            out << "version=1\n";
            out << "sigHash=" << sigHash << "\n";
            for (const auto& [rel, stamp] : map)
            {
                out << rel << "|" << stamp.writeTime << "|" << stamp.fileSize << "|" << stamp.contentHash << "\n";
            }
            return true;
        }

        // UTF-8 등으로 파일 읽기
        static std::string ReadFileAsUtf8(const fs::path& path)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f)
                return {};
            std::ostringstream os;
            os << f.rdbuf();
            return os.str();
        }

        // 타입 문자열 정규화 (const, &, * 제거 후 비교용)
        static std::string NormalizeType(std::string s)
        {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                s.pop_back();
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                s.erase(0, 1);
            size_t i = 0;
            while (i < s.size())
            {
                if (s.compare(i, 5, "const") == 0 && (i + 5 >= s.size() || !std::isalnum(static_cast<unsigned char>(s[i + 5]))))
                {
                    s.erase(i, 5);
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                        s.erase(i, 1);
                    continue;
                }
                if (s[i] == '&' || s[i] == '*')
                {
                    s.erase(i, 1);
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                        s.erase(i, 1);
                    continue;
                }
                ++i;
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                s.pop_back();
            return s;
        }

        static std::string TrimCopy(std::string s)
        {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
                s.pop_back();
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
                ++start;
            return s.substr(start);
        }

        static bool IsIdentifierChar(char c)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            return (std::isalnum(uc) != 0) || c == '_';
        }

        static bool StartsWithKeyword(const std::string& text, size_t pos, const char* keyword)
        {
            const size_t kwLen = std::strlen(keyword);
            if (pos + kwLen > text.size())
                return false;
            if (text.compare(pos, kwLen, keyword) != 0)
                return false;
            if (pos > 0 && IsIdentifierChar(text[pos - 1]))
                return false;
            if (pos + kwLen < text.size() && IsIdentifierChar(text[pos + kwLen]))
                return false;
            return true;
        }

        static void SkipWhitespaceAndComments(const std::string& text, size_t& pos)
        {
            while (pos < text.size())
            {
                if (std::isspace(static_cast<unsigned char>(text[pos])))
                {
                    ++pos;
                    continue;
                }

                if (text[pos] == '/' && (pos + 1) < text.size())
                {
                    if (text[pos + 1] == '/')
                    {
                        pos += 2;
                        while (pos < text.size() && text[pos] != '\n')
                            ++pos;
                        continue;
                    }
                    if (text[pos + 1] == '*')
                    {
                        pos += 2;
                        while ((pos + 1) < text.size() && !(text[pos] == '*' && text[pos + 1] == '/'))
                            ++pos;
                        if ((pos + 1) < text.size())
                            pos += 2;
                        continue;
                    }
                }

                break;
            }
        }

        static bool ReadIdentifier(const std::string& text, size_t& pos, std::string& outIdentifier)
        {
            if (pos >= text.size())
                return false;

            const unsigned char first = static_cast<unsigned char>(text[pos]);
            if (!(std::isalpha(first) || text[pos] == '_'))
                return false;

            const size_t begin = pos;
            ++pos;
            while (pos < text.size())
            {
                const unsigned char c = static_cast<unsigned char>(text[pos]);
                if (!(std::isalnum(c) || text[pos] == '_'))
                    break;
                ++pos;
            }

            outIdentifier = text.substr(begin, pos - begin);
            return true;
        }

        static size_t FindMatchingBracket(const std::string& text, size_t openPos, char openChar, char closeChar)
        {
            if (openPos >= text.size() || text[openPos] != openChar)
                return std::string::npos;

            int depth = 0;
            char stringDelim = '\0';
            for (size_t i = openPos; i < text.size(); ++i)
            {
                const char c = text[i];

                if (stringDelim != '\0')
                {
                    if (c == '\\')
                    {
                        ++i;
                        continue;
                    }
                    if (c == stringDelim)
                        stringDelim = '\0';
                    continue;
                }

                if (c == '"' || c == '\'')
                {
                    stringDelim = c;
                    continue;
                }

                if (c == '/' && (i + 1) < text.size())
                {
                    if (text[i + 1] == '/')
                    {
                        i += 2;
                        while (i < text.size() && text[i] != '\n')
                            ++i;
                        continue;
                    }
                    if (text[i + 1] == '*')
                    {
                        i += 2;
                        while ((i + 1) < text.size() && !(text[i] == '*' && text[i + 1] == '/'))
                            ++i;
                        if ((i + 1) < text.size())
                            ++i;
                        continue;
                    }
                }

                if (c == openChar)
                {
                    ++depth;
                }
                else if (c == closeChar)
                {
                    --depth;
                    if (depth == 0)
                        return i;
                }
            }

            return std::string::npos;
        }

        static std::vector<std::string> SplitTopLevelByComma(const std::string& text)
        {
            std::vector<std::string> out;
            size_t tokenStart = 0;
            int parenDepth = 0;
            int bracketDepth = 0;
            int braceDepth = 0;
            int angleDepth = 0;
            char stringDelim = '\0';

            for (size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (stringDelim != '\0')
                {
                    if (c == '\\')
                    {
                        ++i;
                        continue;
                    }
                    if (c == stringDelim)
                        stringDelim = '\0';
                    continue;
                }

                if (c == '"' || c == '\'')
                {
                    stringDelim = c;
                    continue;
                }

                if (c == '/' && (i + 1) < text.size())
                {
                    if (text[i + 1] == '/')
                    {
                        i += 2;
                        while (i < text.size() && text[i] != '\n')
                            ++i;
                        continue;
                    }
                    if (text[i + 1] == '*')
                    {
                        i += 2;
                        while ((i + 1) < text.size() && !(text[i] == '*' && text[i + 1] == '/'))
                            ++i;
                        if ((i + 1) < text.size())
                            ++i;
                        continue;
                    }
                }

                switch (c)
                {
                case '(': ++parenDepth; break;
                case ')': if (parenDepth > 0) --parenDepth; break;
                case '[': ++bracketDepth; break;
                case ']': if (bracketDepth > 0) --bracketDepth; break;
                case '{': ++braceDepth; break;
                case '}': if (braceDepth > 0) --braceDepth; break;
                case '<': ++angleDepth; break;
                case '>': if (angleDepth > 0) --angleDepth; break;
                case ',':
                    if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0 && angleDepth == 0)
                    {
                        out.push_back(text.substr(tokenStart, i - tokenStart));
                        tokenStart = i + 1;
                    }
                    break;
                default:
                    break;
                }
            }

            if (tokenStart <= text.size())
                out.push_back(text.substr(tokenStart));

            return out;
        }

        static size_t FindTopLevelChar(const std::string& text, char target)
        {
            int parenDepth = 0;
            int bracketDepth = 0;
            int braceDepth = 0;
            int angleDepth = 0;
            char stringDelim = '\0';

            for (size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (stringDelim != '\0')
                {
                    if (c == '\\')
                    {
                        ++i;
                        continue;
                    }
                    if (c == stringDelim)
                        stringDelim = '\0';
                    continue;
                }

                if (c == '"' || c == '\'')
                {
                    stringDelim = c;
                    continue;
                }

                if (c == '/' && (i + 1) < text.size())
                {
                    if (text[i + 1] == '/')
                    {
                        i += 2;
                        while (i < text.size() && text[i] != '\n')
                            ++i;
                        continue;
                    }
                    if (text[i + 1] == '*')
                    {
                        i += 2;
                        while ((i + 1) < text.size() && !(text[i] == '*' && text[i + 1] == '/'))
                            ++i;
                        if ((i + 1) < text.size())
                            ++i;
                        continue;
                    }
                }

                switch (c)
                {
                case '(': ++parenDepth; break;
                case ')': if (parenDepth > 0) --parenDepth; break;
                case '[': ++bracketDepth; break;
                case ']': if (bracketDepth > 0) --bracketDepth; break;
                case '{': ++braceDepth; break;
                case '}': if (braceDepth > 0) --braceDepth; break;
                case '<': ++angleDepth; break;
                case '>': if (angleDepth > 0) --angleDepth; break;
                default:
                    break;
                }

                if (c == target && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0 && angleDepth == 0)
                    return i;
            }

            return std::string::npos;
        }

        static std::string ParseEnumeratorName(const std::string& rawEntry)
        {
            std::string entry = TrimCopy(rawEntry);
            if (entry.empty())
                return {};

            const size_t assignPos = FindTopLevelChar(entry, '=');
            if (assignPos != std::string::npos)
                entry = entry.substr(0, assignPos);
            entry = TrimCopy(entry);
            if (entry.empty())
                return {};

            size_t pos = 0;
            SkipWhitespaceAndComments(entry, pos);
            std::string identifier;
            if (!ReadIdentifier(entry, pos, identifier))
                return {};
            return identifier;
        }

        static std::vector<std::string> ParseEnumeratorNames(const std::string& enumBody)
        {
            std::vector<std::string> values;
            std::set<std::string> dedup;
            const std::vector<std::string> entries = SplitTopLevelByComma(enumBody);
            for (const std::string& raw : entries)
            {
                const std::string name = ParseEnumeratorName(raw);
                if (name.empty())
                    continue;
                if (!dedup.insert(name).second)
                    continue;
                values.push_back(name);
            }
            return values;
        }

        static void ParseEnums(const std::string& classBody, ScriptInfo& info)
        {
            std::set<std::string> registeredEnums;
            size_t searchPos = 0;
            while (true)
            {
                const size_t macroPos = classBody.find("USCRIPT_ENUM", searchPos);
                if (macroPos == std::string::npos)
                    break;

                searchPos = macroPos + std::strlen("USCRIPT_ENUM");
                size_t openParen = classBody.find('(', searchPos);
                if (openParen == std::string::npos)
                    break;
                const size_t closeParen = FindMatchingBracket(classBody, openParen, '(', ')');
                if (closeParen == std::string::npos)
                    continue;

                size_t cursor = closeParen + 1;
                SkipWhitespaceAndComments(classBody, cursor);
                if (!StartsWithKeyword(classBody, cursor, "enum"))
                {
                    searchPos = closeParen + 1;
                    continue;
                }
                cursor += 4; // "enum"
                SkipWhitespaceAndComments(classBody, cursor);

                bool isScoped = false;
                if (StartsWithKeyword(classBody, cursor, "class"))
                {
                    isScoped = true;
                    cursor += 5;
                    SkipWhitespaceAndComments(classBody, cursor);
                }
                else if (StartsWithKeyword(classBody, cursor, "struct"))
                {
                    isScoped = true;
                    cursor += 6;
                    SkipWhitespaceAndComments(classBody, cursor);
                }

                std::string enumName;
                if (!ReadIdentifier(classBody, cursor, enumName))
                {
                    searchPos = closeParen + 1;
                    continue;
                }

                while (cursor < classBody.size())
                {
                    if (classBody[cursor] == '{')
                        break;

                    if (classBody[cursor] == '/' && (cursor + 1) < classBody.size())
                    {
                        if (classBody[cursor + 1] == '/')
                        {
                            cursor += 2;
                            while (cursor < classBody.size() && classBody[cursor] != '\n')
                                ++cursor;
                            continue;
                        }
                        if (classBody[cursor + 1] == '*')
                        {
                            cursor += 2;
                            while ((cursor + 1) < classBody.size() && !(classBody[cursor] == '*' && classBody[cursor + 1] == '/'))
                                ++cursor;
                            if ((cursor + 1) < classBody.size())
                                cursor += 2;
                            continue;
                        }
                    }
                    ++cursor;
                }

                if (cursor >= classBody.size() || classBody[cursor] != '{')
                {
                    searchPos = closeParen + 1;
                    continue;
                }

                const size_t bodyOpen = cursor;
                const size_t bodyClose = FindMatchingBracket(classBody, bodyOpen, '{', '}');
                if (bodyClose == std::string::npos)
                {
                    searchPos = closeParen + 1;
                    continue;
                }

                EnumInfo enumInfo;
                enumInfo.name = enumName;
                enumInfo.isScoped = isScoped;
                enumInfo.values = ParseEnumeratorNames(classBody.substr(bodyOpen + 1, bodyClose - bodyOpen - 1));

                if (!enumInfo.values.empty() && registeredEnums.insert(enumInfo.name).second)
                    info.enums.push_back(std::move(enumInfo));

                searchPos = bodyClose + 1;
            }
        }

        // 파라미터 문자열 파싱: "int a, const CollisionInfo& e" -> ["int", "CollisionInfo"]
        static std::vector<std::string> ParseParameterTypes(const std::string& paramsStr)
        {
            std::vector<std::string> out;
            std::string s = paramsStr;
            size_t depth = 0;
            std::string current;
            for (size_t i = 0; i < s.size(); ++i)
            {
                char c = s[i];
                if (c == '<' || c == '(' || c == '[')
                    ++depth;
                else if (c == '>' || c == ')' || c == ']')
                    --depth;
                else if (c == ',' && depth == 0)
                {
                    std::string typePart = current;
                    size_t lastSpace = typePart.rfind(' ');
                    if (lastSpace != std::string::npos)
                        typePart = typePart.substr(lastSpace + 1);
                    else
                    {
                        while (!typePart.empty() && (typePart.back() == ' ' || typePart.back() == '\t'))
                            typePart.pop_back();
                    }
                    out.push_back(NormalizeType(typePart));
                    current.clear();
                    continue;
                }
                current += c;
            }
            if (!current.empty())
            {
                std::string typePart = current;
                size_t lastSpace = typePart.rfind(' ');
                if (lastSpace != std::string::npos)
                    typePart = typePart.substr(lastSpace + 1);
                else
                {
                    while (!typePart.empty() && (typePart.back() == ' ' || typePart.back() == '\t'))
                        typePart.pop_back();
                }
                out.push_back(NormalizeType(typePart));
            }
            return out;
        }

        // 시그니처 목록과 (name, paramTypes) 일치 여부
        static bool MatchesEngineSignature(
            const std::string& funcName,
            const std::vector<std::string>& paramTypes,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs)
        {
            for (const auto& sig : engineSigs)
            {
                if (sig.messageName != funcName)
                    continue;
                if (sig.paramTypes.size() != paramTypes.size())
                    continue;
                bool match = true;
                for (size_t i = 0; i < sig.paramTypes.size(); ++i)
                {
                    if (NormalizeType(paramTypes[i]) != sig.paramTypes[i])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        // 클래스 본문 추출 (class X : public ScriptBehaviour { ... } 에서 { ... } 부분).
        // outBodyEnd = 본문 끝('}' 다음) 위치.
        static std::string ExtractClassBody(const std::string& text, size_t classDeclEnd, size_t& outBodyEnd)
        {
            outBodyEnd = classDeclEnd;
            size_t i = classDeclEnd;
            while (i < text.size() && text[i] != '{')
                ++i;
            if (i >= text.size())
                return {};
            size_t start = i;
            int depth = 1;
            ++i;
            while (i < text.size() && depth > 0)
            {
                if (text[i] == '{')
                    ++depth;
                else if (text[i] == '}')
                    --depth;
                ++i;
            }
            if (depth != 0)
                return {};
            outBodyEnd = i;
            return text.substr(start, i - start);
        }

        // ScriptBehaviour 상속 클래스 이름 추출
        static bool FindScriptBehaviourClass(const std::string& text, std::string& outClassName, size_t& outDeclEnd)
        {
            // class [USERSCRIPTS ]Enemy : public ScriptBehaviour — 클래스명은 콜론 직전 마지막 단어
            std::regex re(R"re(class\s+(?:\w+\s+)*(\w+)\s*:\s*public\s+ScriptBehaviour)re");
            std::smatch m;
            if (!std::regex_search(text, m, re))
                return false;
            outClassName = m[1].str();
            outDeclEnd = static_cast<size_t>(m.position(0)) + m.length();
            return true;
        }

        static void ParseMessagesAndProperties(
            const std::string& classBody,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs,
            ScriptInfo& info)
        {
            ExtractDependencies(classBody, info);

            std::set<std::string> registeredMessages; // 중복 방지

            // 1) USCRIPT_MESSAGE() void Func(params);
            {
                std::regex re(R"re(USCRIPT_MESSAGE\s*\(\s*\)\s*void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string cppName = (*it)[1].str();
                    std::string params = (*it)[2].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = cppName;
                    mi.cppName = cppName;
                    mi.paramTypes = ParseParameterTypes(params);
                    for (const auto& t : mi.paramTypes)
                        ExtractDependenciesFromType(t, info);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 2) USCRIPT_MESSAGE_NAME("X") void Func(params);
            {
                std::regex re(R"re(USCRIPT_MESSAGE_NAME\s*\(\s*"([^"]*)"\s*\)\s*void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string messageName = (*it)[1].str();
                    std::string cppName = (*it)[2].str();
                    std::string params = (*it)[3].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = messageName;
                    mi.cppName = cppName;
                    mi.paramTypes = ParseParameterTypes(params);
                    for (const auto& t : mi.paramTypes)
                        ExtractDependenciesFromType(t, info);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 3) 매크로 없이 void Func(params); 선언만 있는 경우 -> 엔진 시그니처와 일치하면 등록
            {
                std::regex re(R"re(void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string cppName = (*it)[1].str();
                    std::string params = (*it)[2].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    std::vector<std::string> paramTypes = ParseParameterTypes(params);
                    if (!MatchesEngineSignature(cppName, paramTypes, engineSigs))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = cppName;
                    mi.cppName = cppName;
                    mi.paramTypes = std::move(paramTypes);
                    for (const auto& t : mi.paramTypes)
                        ExtractDependenciesFromType(t, info);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 4) USCRIPT_PROPERTY() type name;
            {
                std::regex re(R"re(USCRIPT_PROPERTY\s*\(\s*\)\s+([^;=]+)\s+(\w+)\s*[;=])re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    PropertyInfo pi;
                    pi.type = NormalizeType((*it)[1].str());
                    pi.name = (*it)[2].str();
                    ExtractDependenciesFromType(pi.type, info);
                    info.properties.push_back(std::move(pi));
                }
            }

            // 5) USCRIPT_PROPERTY_CHAIN("...") type name; -> INSPECTOR_CHAIN 메타데이터 등록
            {
                std::regex re(R"re(USCRIPT_PROPERTY_CHAIN\s*\(\s*"([^"]*)"\s*\)\s+([^;=]+)\s+(\w+)\s*[;=])re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    PropertyInfo pi;
                    pi.inspectorChain = (*it)[1].str();
                    pi.type = NormalizeType((*it)[2].str());
                    pi.name = (*it)[3].str();
                    ExtractDependenciesFromType(pi.type, info);
                    info.properties.push_back(std::move(pi));
                }
            }

            // 6) USCRIPT_PROPERTY_RANGE("...") type name; -> RANGE 메타데이터 등록
            {
                std::regex re(R"re(USCRIPT_PROPERTY_RANGE\s*\(\s*"([^"]*)"\s*\)\s+([^;=]+)\s+(\w+)\s*[;=])re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    PropertyInfo pi;
                    pi.range = (*it)[1].str();
                    pi.type = NormalizeType((*it)[2].str());
                    pi.name = (*it)[3].str();
                    ExtractDependenciesFromType(pi.type, info);
                    info.properties.push_back(std::move(pi));
                }
            }

            // 7) USCRIPT_PROPERTY_HIDDEN() type name; -> RTTR 등록하되 인스펙터에서 숨김
            {
                std::regex re(R"re(USCRIPT_PROPERTY_HIDDEN\s*\(\s*\)\s+([^;=]+)\s+(\w+)\s*[;=])re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    PropertyInfo pi;
                    pi.type = NormalizeType((*it)[1].str());
                    pi.name = (*it)[2].str();
                    pi.inspectorHidden = true;
                    ExtractDependenciesFromType(pi.type, info);
                    info.properties.push_back(std::move(pi));
                }
            }

            // 8) USCRIPT_ENUM() enum class/enum ... { ... };
            ParseEnums(classBody, info);
        }

        static std::vector<ScriptInfo> ParseHeaderFile(
            const fs::path& headerPath,
            const fs::path& scriptsDir,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs)
        {
            std::vector<ScriptInfo> result;
            std::string text = ReadFileAsUtf8(headerPath);
            if (text.empty())
                return result;

            std::string className;
            size_t declEnd = 0;
            while (FindScriptBehaviourClass(text, className, declEnd))
            {
                ScriptInfo info;
                info.className = className;
                info.headerPath = fs::relative(headerPath, scriptsDir);
                size_t bodyEnd = 0;
                std::string body = ExtractClassBody(text, declEnd, bodyEnd);
                if (body.empty())
                    break;

                info.dontAutogen = (body.find("USCRIPT_DONT_AUTOGEN") != std::string::npos);
                if (!info.dontAutogen)
                    ParseMessagesAndProperties(body, engineSigs, info);

                result.push_back(std::move(info));

                // 다음 클래스 검색 (이 클래스 본문 이후부터)
                if (bodyEnd >= text.size())
                    break;
                text = text.substr(bodyEnd);
                declEnd = 0;
            }
            return result;
        }

        /// 짝이 되는 .cpp에 RTTR 등록이 이미 있으면 true (gen.cpp에서 스킵용)
        static bool HasOwnRttrRegistration(const fs::path& scriptsDir, const fs::path& headerPath, const std::string& className)
        {
            fs::path cppPath = scriptsDir / headerPath;
            cppPath.replace_extension(".cpp");
            if (!fs::exists(cppPath) || !fs::is_regular_file(cppPath))
                return false;
            std::string cppText = ReadFileAsUtf8(cppPath);
            if (cppText.find("RTTR_PLUGIN_REGISTRATION") == std::string::npos
                && cppText.find("RTTR_REGISTRATION") == std::string::npos)
                return false;
            if (cppText.find("class_<" + className + ">") == std::string::npos)
                return false;
            return true;
        }

        // gen.cpp 생성 (이미 RTTR 등록된 .cpp가 있는 클래스는 제외)
        static bool WriteGenCpp(const fs::path& scriptsDir, const std::vector<ScriptInfo>& scripts, const ScriptDB& db)
        {
            std::vector<const ScriptInfo*> toGen;
            for (const auto& s : scripts)
            {
                if (HasOwnRttrRegistration(scriptsDir, s.headerPath, s.className))
                    continue;
                toGen.push_back(&s);
            }

            fs::path genPath = scriptsDir / "UserScripts.gen.cpp";
            std::ostringstream os;
            os << "// Auto-generated. Do not edit.\n";
            os << "#pragma optimize(\"\", off)\n";
            os << "#include \"Export.h\"\n";
            os << "#include \"ScriptBehaviour.h\"\n";
            os << "#include \"UserScriptsCommon.h\"\n";
            os << "#include \"Object.h\"\n";
            os << "#include \"GameObject.h\"\n";
            os << "#include \"CoreComponents.h\"\n";
            os << "#include \"rttr/registration\"\n";
            os << "#include \"rttr/detail/policies/ctor_policies.h\"\n\n";

            std::set<std::string> includedHeaders;
            for (const auto* s : toGen)
            {
                // gen.cpp 가 Scripts/ 안에 있으므로, 같은 폴더 기준 상대 경로만 사용 (Scripts/ 접두어 없음)
                std::string incPath = s->headerPath.generic_string();
                std::replace(incPath.begin(), incPath.end(), '\\', '/');
                includedHeaders.insert(incPath);
                os << "#include \"" << incPath << "\"\n";
            }

            std::set<std::string> extraIncludes;
            for (const auto* s : toGen) {
                for (const auto& dep : s->dependencies) {
                    if (db.count(dep)) {
                        std::string dPath = db.at(dep).generic_string();
                        std::replace(dPath.begin(), dPath.end(), '\\', '/');
                        extraIncludes.insert(dPath);
                    }
                }
            }

            for (const auto& inc : extraIncludes) {
                if (includedHeaders.insert(inc).second)
                    os << "#include \"" << inc << "\"\n";
            }

            os << "\nusing namespace rttr;\nusing namespace MMMEngine;\n\nRTTR_PLUGIN_REGISTRATION\n{\n";

            for (const auto& s : toGen)
            {
                for (const auto& e : s->enums)
                {
                    os << "\tregistration::enumeration<" << s->className << "::" << e.name << ">(\""
                       << s->className << "::" << e.name << "\")\n";
                    os << "\t\t(\n";
                    for (size_t i = 0; i < e.values.size(); ++i)
                    {
                        const std::string& valueName = e.values[i];
                        os << "\t\t\trttr::value(\"" << valueName << "\", ";
                        if (e.isScoped)
                            os << s->className << "::" << e.name << "::" << valueName;
                        else
                            os << s->className << "::" << valueName;
                        os << ")";
                        if ((i + 1) < e.values.size())
                            os << ",";
                        os << "\n";
                    }
                    os << "\t\t);\n\n";
                }

                os << "\tregistration::class_<" << s->className << ">(\"" << s->className << "\")\n";
                os << "\t\t(rttr::metadata(\"wrapper_type_name\", \"ObjPtr<" << s->className << ">\"))";
                for (const auto& p : s->properties)
                {
                    os << "\n\t\t.property(\"" << p.name << "\", &" << s->className << "::" << p.name << ")";
                    if (p.inspectorHidden)
                        os << "(rttr::metadata(\"INSPECTOR\", \"HIDDEN\"))";
                if (!p.inspectorChain.empty())
                    os << "(rttr::metadata(\"INSPECTOR_CHAIN\", \"" << p.inspectorChain << "\"))";
                if (!p.range.empty())
                    os << "(rttr::metadata(\"RANGE\", \"" << p.range << "\"))";
                }
                os << ";\n\n";

                os << "\tregistration::class_<ObjPtr<" << s->className << ">>(\"ObjPtr<" << s->className << ">\")\n";
                os << "\t\t.constructor([]() { return Object::NewObject<" << s->className << ">(); })\n";
                os << "\t\t.method(\"Inject\", &ObjPtr<" << s->className << ">::Inject);\n\n";
            }
            os << "}\n";
            std::string newText = os.str();
            std::string oldText = ReadFileAsUtf8(genPath);
            if (!oldText.empty())
            {
                std::string oldNormalized = NormalizeToCrlf(std::move(oldText));
                std::string newNormalized = NormalizeToCrlf(newText);
                if (oldNormalized == newNormalized)
                    return true;
            }
            return WriteUtf8BomFile(genPath, newText);
        }

        // 생성자 본문에서 REGISTER_BEHAVIOUR_MESSAGE 제외한 라인들 유지
        static std::vector<std::string> ExtractConstructorBodyWithoutRegisters(const std::string& ctorBody)
        {
            std::vector<std::string> lines;
            std::istringstream is(ctorBody);
            std::string line;
            while (std::getline(is, line))
            {
                std::string trimmed = line;
                while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n'))
                    trimmed.pop_back();
                size_t i = 0;
                while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t'))
                    ++i;
                trimmed = trimmed.substr(i);
                if (trimmed.empty())
                    continue;
                if (trimmed.find("REGISTER_BEHAVIOUR_MESSAGE") != std::string::npos)
                    continue;
                lines.push_back(trimmed);
            }
            return lines;
        }

        // 헤더에 생성자 주입: ClassName() { ... } 를 찾아 REGISTER_BEHAVIOUR_MESSAGE 목록으로 교체
        static std::string InjectConstructor(
            std::string headerText,
            const std::string& className,
            const std::vector<MessageInfo>& messages)
        {
            std::set<std::string> added;
            for (const auto& m : messages)
            {
                if (added.count(m.cppName))
                    continue;
                added.insert(m.cppName);
            }
            std::string registerList;
            for (const std::string& name : added)
                registerList += "        REGISTER_BEHAVIOUR_MESSAGE(" + name + ");\n";

            // ClassName() { ... } 패턴 찾기 (괄호 매칭)
            std::regex ctorRegex(std::string(R"re(\s*)re") + className + R"re(\s*\(\s*\)\s*\{)re");
            std::smatch m;
            if (!std::regex_search(headerText, m, ctorRegex))
                return headerText;

            size_t bodyStart = m.position(0) + m.length();
            int depth = 1;
            size_t i = bodyStart;
            while (i < headerText.size() && depth > 0)
            {
                if (headerText[i] == '{')
                    ++depth;
                else if (headerText[i] == '}')
                    --depth;
                ++i;
            }
            size_t bodyEnd = i - 1;
            std::string oldBody = headerText.substr(bodyStart, bodyEnd - bodyStart);
            std::vector<std::string> keepLines = ExtractConstructorBodyWithoutRegisters(oldBody);

            std::string newBody = "\n";
            for (const auto& l : keepLines)
                newBody += "        " + l + "\n";
            newBody += registerList;
            newBody += "\n        ";  // 닫는 } 를 새 줄 + 들여쓰기로 맞춤

            headerText.replace(bodyStart, bodyEnd - bodyStart, newBody);
            return headerText;
        }
    }

    bool UserScriptsGenerator::Generate(const fs::path& projectRootDir)
    {
        fs::path scriptsDir = projectRootDir / "Source" / "UserScripts" / "Scripts";
        if (!fs::exists(scriptsDir) || !fs::is_directory(scriptsDir))
            return true;

        std::vector<MMMEngine::UserScriptMessageSignature> engineSigs = MMMEngine::GetEngineMessageSignatures();
        const size_t sigHash = ComputeEngineSignatureHash(engineSigs);

        std::unordered_map<std::string, FileStamp> cached;
        size_t cachedSigHash = 0;
        const bool hasCache = LoadCache(scriptsDir, cachedSigHash, cached);

        std::unordered_map<std::string, FileStamp> current;
        std::vector<std::string> headerList;

        bool structureChanged = false;

        std::error_code ecDir;
        auto dirIt = fs::recursive_directory_iterator(scriptsDir, fs::directory_options::skip_permission_denied, ecDir);
        if (ecDir) 
            return true;

        for (const auto& e : dirIt) 
        {
            if (!e.is_regular_file() || e.path().extension() != ".h") continue;

            FileStamp stamp;
            if (!GetFileStamp(e.path(), stamp)) continue;
            std::string rel = fs::relative(e.path(), scriptsDir).generic_u8string();

            auto it = cached.find(rel);
            if (it == cached.end()) 
            {
                structureChanged = true;
                stamp.contentHash = ComputeFileHash(e.path()); // 새 파일 해시 계산
            }
            else if (it->second.writeTime != stamp.writeTime || it->second.fileSize != stamp.fileSize) 
            {
                size_t newHash = ComputeFileHash(e.path()); // 변경된 파일 해시 계산
                if (newHash != it->second.contentHash) 
                {
                    structureChanged = true;
                    stamp.contentHash = newHash;
                }
                else 
                {
                    stamp.contentHash = it->second.contentHash; // 시간만 변한 경우 기존 해시 유지
                }
            }
            else 
            {
                stamp.contentHash = it->second.contentHash; // 변한 게 없는 경우 기존 해시 유지
            }

            current.emplace(rel, stamp); // 최종 결정된 stamp(해시 포함)를 넣어야 함
            headerList.push_back(rel);
        }

        if (!structureChanged && current.size() != cached.size()) 
        {
            structureChanged = true;
        }

        // 3. 엔진 시그니처 변경 확인
        if (cachedSigHash != sigHash) structureChanged = true;

        fs::path genPath = scriptsDir / "UserScripts.gen.cpp";

        if (!structureChanged && fs::exists(genPath)) {
            return true; // 진짜 아무것도 안 해도 되는 상태
        }

        // --- 여기서부터 실제 무거운 작업 시작 ---
        std::sort(headerList.begin(), headerList.end());
        std::vector<ScriptInfo> allScripts;

        std::unordered_map<std::string, std::filesystem::path> scriptDB;

        for (const auto& rel : headerList) 
        {
            const fs::path headerPath = scriptsDir / fs::path(rel);
            // [병목 지점] structureChanged가 true일 때만 전체 파싱을 하거나, 
            // 변경된 파일만 부분 파싱하여 캐시를 업데이트하는 로직이 필요함
            std::vector<ScriptInfo> infos = ParseHeaderFile(headerPath, scriptsDir, engineSigs);
            for (ScriptInfo& info : infos) 
            {
                info.headerPath = fs::relative(headerPath, scriptsDir);
                scriptDB.emplace(info.className, info.headerPath);
                allScripts.push_back(std::move(info));
            }
        }

        // 스크립트 유무와 관계없이 gen.cpp 는 항상 생성 (빌드가 동일한 파일 집합을 사용하도록)
        if (!WriteGenCpp(scriptsDir, allScripts, scriptDB))
            return false;

        for (const ScriptInfo& info : allScripts)
        {
            if (info.dontAutogen || info.messages.empty())
                continue;
            fs::path headerPath = scriptsDir / info.headerPath;
            std::string text = ReadFileAsUtf8(headerPath);
            if (text.empty())
                continue;
            std::string newText = InjectConstructor(text, info.className, info.messages);
            std::string oldNormalized = NormalizeToCrlf(text);
            std::string newNormalized = NormalizeToCrlf(newText);
            if (newNormalized == oldNormalized)
                continue;

            fs::path backupPath = headerPath;
            backupPath += ".bak";
            std::error_code ec;
            if (fs::exists(backupPath))
                fs::remove(backupPath, ec);
            fs::rename(headerPath, backupPath, ec);

            if (!WriteUtf8BomFile(headerPath, newText))
            {
                fs::rename(backupPath, headerPath, ec);
                continue;
            }
            fs::remove(backupPath, ec);
        }

        SaveCache(scriptsDir, sigHash, current);
        return true;
    }
}
