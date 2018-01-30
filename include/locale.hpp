#ifndef  _GITHUB_SCINART_CPPLIB_LOCALE_HPP_
#define  _GITHUB_SCINART_CPPLIB_LOCALE_HPP_

#include "macro.hpp"

#include <boost/locale.hpp>

namespace oy
{

static constexpr wchar_t CJK_Unified_Ideographs_begin = 0x4E00;
static constexpr wchar_t CJK_Unified_Ideographs_end = 0x9FCC+1;
constexpr inline bool isCJK(const wchar_t wc) { return wc>=CJK_Unified_Ideographs_begin && wc < CJK_Unified_Ideographs_end; }

inline auto gbk_to_utf8(const std::string& gbk_str)    { return boost::locale::conv::to_utf<char>(gbk_str, std::string("GBK")); }
inline auto utf8_to_gbk(const std::string& str)        { return boost::locale::conv::from_utf<char>(str, std::string("GBK")); }
inline auto gbk_to_wstring(const std::string& gbk_str) { return boost::locale::conv::to_utf<wchar_t>(gbk_str, std::string("GBK")); }
inline auto wstring_to_gbk(const std::wstring& wstr)   { return boost::locale::conv::from_utf<wchar_t>(wstr, std::string("GBK")); }
inline auto wchar_to_utf8(const wchar_t& wc)           { return boost::locale::conv::utf_to_utf<char>(&wc, &wc+1); }
inline auto wchar_to_utf8(const wchar_t* const wc_b, const wchar_t* const wc_e) { return boost::locale::conv::utf_to_utf<char>(wc_b,wc_e); }

inline bool is_utf8_string(const std::string& str)
{
    // http://www.zedwood.com/article/cpp-is-valid-utf8-string-function
    for (int i=0, c=0, n=0, len=str.length(); i < len; i++)
    {
        if (0x00 <= c && c <= 0x7f) n=0; // 0bbbbbbb
        else if ((c & 0xE0) == 0xC0) n=1; // 110bbbbb
        else if ( c==0xed && i<(len-1) && ((unsigned char)str[i+1] & 0xa0)==0xa0) return false; //U+d800 to U+dfff
        else if ((c & 0xF0) == 0xE0) n=2; // 1110bbbb
        else if ((c & 0xF8) == 0xF0) n=3; // 11110bbb
        // else if (($c & 0xFC) == 0xF8) n=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
        // else if (($c & 0xFE) == 0xFC) n=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
        else return false;
        for (int j=0; j<n && i<len; j++)
            if ((++i == len) || (( (unsigned char)str[i] & 0xC0) != 0x80))
                return false;
    }
    return true;
}

}

#endif