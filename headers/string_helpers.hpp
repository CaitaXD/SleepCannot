#include <string>
#include <algorithm>

static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
}

static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}

static inline void trim(std::string &s)
{
    rtrim(s);
    ltrim(s);
}

static inline char ascii_toupper(char c)
{
    return ('a' <= c && c <= 'z') ? c ^ 0x20 : c;
}

static inline void ascii_toupper(std::string &src)
{
    size_t len = src.length();
    for (size_t i = 0; i < len; ++i)
    {
        src[i] = ascii_toupper(src[i]);
    }
}

struct StringEqComparerIgnoreCase
{
    bool operator()(const std::string &lhs, const std::string &rhs) const
    {
        return strcasecmp(lhs.c_str(), rhs.c_str()) == 0;
    }
};

struct StringHashIgnoreCase
{
    std::hash<string> hash_function;
    std::size_t operator()(std::string str) const
    {
        for (std::size_t index = 0; index < str.size(); ++index)
        {
            auto ch = static_cast<unsigned char>(str[index]);
            str[index] = static_cast<unsigned char>(ascii_toupper(ch));
        }
        return hash_function(str);
    }
};
