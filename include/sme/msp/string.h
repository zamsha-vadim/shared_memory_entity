#ifndef SME_MSP_STRING_H
#define SME_MSP_STRING_H

#include <string>
#include <string_view>

#include "sme/internal/string_hash.h"
#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename CharT, class Traits = std::char_traits<CharT>>
using basic_string = std::basic_string<CharT, Traits, MemorySpaceAllocator<CharT>>;

using string = sme::msp::basic_string<char>;
using wstring = sme::msp::basic_string<wchar_t>;
using u16string = sme::msp::basic_string<char16_t>;
using u32string = sme::msp::basic_string<char32_t>;

}  // namespace msp
}  // namespace sme

template <>
class std::hash<sme::msp::string> : public sme::StringHashImpl<sme::msp::string> {
};

template <>
class std::hash<sme::msp::wstring> : public sme::StringHashImpl<sme::msp::wstring> {
};

template <>
class std::hash<sme::msp::u16string> : public sme::StringHashImpl<sme::msp::u16string> {
};

template <>
class std::hash<sme::msp::u32string> : public sme::StringHashImpl<sme::msp::u32string> {
};

#endif  //  SME_MS_STRING_H
