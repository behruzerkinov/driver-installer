#include "DeviceInfo.h"
#include <algorithm>
#include <cctype>
#include <cwctype>

namespace DriverNest::Hw {

// ---------------------------------------------------------------------------
// Version string comparison.
//
// Drivers typically use NotProductVersion style values like "10.0.22621.1503"
// or simpler "2535.1.0.0". This parser treats each dot-separated numeric field
// as an unsigned and compares them left-to-right. Trailing zero fields are
// ignored beyond the length of the shorter string so "10" == "10.0".
// ---------------------------------------------------------------------------
int CompareVersion(std::wstring_view a, std::wstring_view b) noexcept {
    std::size_t ia = 0;
    std::size_t ib = 0;

    while (ia < a.size() || ib < b.size()) {
        // skip separators
        while (ia < a.size() && a[ia] == '.') ++ia;
        while (ib < b.size() && b[ib] == '.') ++ib;

        if (ia == a.size() && ib == b.size()) break;
        if (ia == a.size()) return -1; // a is a prefix; shorter means less
        if (ib == b.size()) return 1;

        // parse field a
        unsigned long va = 0;
        while (ia < a.size() && std::iswdigit(a[ia])) {
            va = va * 10 + (a[ia] - L'0');
            ++ia;
        }
        // parse field b
        unsigned long vb = 0;
        while (ib < b.size() && std::iswdigit(b[ib])) {
            vb = vb * 10 + (b[ib] - L'0');
            ++ib;
        }

        if (va < vb) return -1;
        if (va > vb) return 1;
    }
    return 0;
}

} // namespace DriverNest::Hw
