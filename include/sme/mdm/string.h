#ifndef SME_MDM_STRING_H
#define SME_MDM_STRING_H

#include <string>

#include "sme/internal/string_hash.h"
#include "sme/mdm/allocator.h"
#include "sme/mdm/unique_ptr.h"

namespace sme {
namespace mdm {

template <typename CharT, class Traits = std::char_traits<CharT>>
using basic_string = std::basic_string<CharT, Traits, MemoryDomainAllocator<CharT>>;

using string = sme::mdm::basic_string<char>;
using wstring = sme::mdm::basic_string<wchar_t>;
using u16string = sme::mdm::basic_string<char16_t>;
using u32string = sme::mdm::basic_string<char32_t>;

template <typename StringT = sme::mdm::string, typename... Arg>
auto MakeStringUnique(MemoryDomain& mem_domain, Arg&&... arg) -> auto
{
    return MakeUnique<StringT>(mem_domain, std::forward<Arg>(arg)...,
                               ItemAllocator<StringT>(mem_domain));
}

}  // namespace mdm
}  // namespace sme

template <>
class std::hash<sme::mdm::string> : public sme::StringHashImpl<sme::mdm::string> {};

template <>
class std::hash<sme::mdm::wstring> : public sme::StringHashImpl<sme::mdm::wstring> {};

template <>
class std::hash<sme::mdm::u16string> : public sme::StringHashImpl<sme::mdm::u16string> {};

template <>
class std::hash<sme::mdm::u32string> : public sme::StringHashImpl<sme::mdm::u32string> {};

#endif  //  SME_MDM_STRING_H
