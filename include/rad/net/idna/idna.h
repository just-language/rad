#pragma once
#include <rad/libbase.h>

#include <string>
#include <system_error>

namespace rad::net::idna {
    /*!
     * @brief Convert a unicode internationalized domain name (IDN)  into an
     * ASCII-compatible encoding.
     *
     * @param domain_name The input unicode internationalized domain name (IDN).
     * After return, it will contain the ASCII-compatible encoded domain name.
     * @param check_hyphens True to check hyphens.
     * @param check_bidi True to check BIDI.
     * @param check_joiners True to check joiners.
     * @param use_std3_ascii_rules Ttrue to use STD3 ASCII rules.
     * @param transitional_processing True to perform Transitional Processing.
     * @param verify_dns_length True to verify DNS length limits.
     * @param ignore_invalid_punycode True to ignore invalud punycode.
     * @param ec Set to indicate error occured, if any.
     * @return True if the conversion is successful, otherwise false on failure.
     */
    RAD_EXPORT_DECL bool
    domain_to_ascii(std::string& domain_name, bool check_hyphens,
                    bool check_bidi, bool check_joiners,
                    bool use_std3_ascii_rules, bool transitional_processing,
                    bool verify_dns_length, bool ignore_invalid_punycode,
                    std::error_code& ec);
} // namespace rad::net::idna