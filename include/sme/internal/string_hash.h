#ifndef SME_INTERNAL_STRING_HASH_H
#define SME_INTERNAL_STRING_HASH_H

#include <string_view>

namespace sme {

template <typename StringType>
struct StringHashImpl {
    auto operator()(const StringType& s) const -> size_t
    {
        using StringView = std::basic_string_view<typename StringType::value_type,
                                                  typename StringType::traits_type>;

        const StringView sv(s.c_str(), s.size());
        std::hash<StringView> sv_hash;

        return sv_hash(sv);
    }
};

}  // namespace sme

#endif  // SME_INTERNAL_STRING_HASH_H
