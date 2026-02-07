#pragma once
#include <streambuf>
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <utility>

class ConsoleStreamBuf : public std::streambuf
{
public:
    using Callback = std::function<void(const std::string&)>;

    explicit ConsoleStreamBuf(Callback cb)
        : m_Callback(std::move(cb))
    {
    }

protected:
    int overflow(int ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
        {
            sync();
            return traits_type::not_eof(ch);
        }

        const char c = traits_type::to_char_type(ch);
        return xsputn(&c, 1) == 1 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override
    {
        if (!s || count <= 0)
            return 0;

        std::vector<std::string> lines;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            for (std::streamsize i = 0; i < count; ++i)
            {
                const char c = s[i];
                if (c == '\n' || c == '\r')
                {
                    if (!m_Line.empty())
                    {
                        lines.emplace_back(std::move(m_Line));
                        m_Line.clear();
                    }
                }
                else
                {
                    m_Line.push_back(c);
                }
            }
        }

        if (m_Callback)
        {
            for (const auto& line : lines)
                m_Callback(line);
        }
        return count;
    }

    int sync() override
    {
        std::string line;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!m_Line.empty())
            {
                line = std::move(m_Line);
                m_Line.clear();
            }
        }
        if (!line.empty() && m_Callback)
            m_Callback(line);
        return 0;
    }

private:
    std::string m_Line;
    Callback m_Callback;
    std::mutex m_Mutex;
};
