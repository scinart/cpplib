#ifndef  _GITHUB_SCINART_CPPLIB_DECODER_UTILS_H_
#define  _GITHUB_SCINART_CPPLIB_DECODER_UTILS_H_

#include "macro.hpp"

#include <boost/locale.hpp>

static constexpr wchar_t CJK_Unified_Ideographs_begin = 0x4E00;
static constexpr wchar_t CJK_Unified_Ideographs_end = 0x9FCC+1;
constexpr inline bool isCJK(const wchar_t wc) { return wc>=CJK_Unified_Ideographs_begin && wc < CJK_Unified_Ideographs_end; }

inline auto gbk_to_utf8(const std::string& gbk_str)    { return boost::locale::conv::to_utf<char>(gbk_str, std::string("GBK")); }
inline auto utf8_to_gbk(const std::string& str)        { return boost::locale::conv::from_utf<char>(str, std::string("GBK")); }
inline auto gbk_to_wstring(const std::string& gbk_str) { return boost::locale::conv::to_utf<wchar_t>(gbk_str, std::string("GBK")); }
inline auto wstring_to_gbk(const std::wstring& wstr)   { return boost::locale::conv::from_utf<wchar_t>(wstr, std::string("GBK")); }
inline auto wchar_to_utf8(const wchar_t& wc)           { return boost::locale::conv::utf_to_utf<char>(&wc, &wc+1); }
inline auto wchar_to_utf8(const wchar_t* const wc_b, const wchar_t* const wc_e) { return boost::locale::conv::utf_to_utf<char>(wc_b,wc_e); }

#endif