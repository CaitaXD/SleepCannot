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