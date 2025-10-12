#include <rad/string.h>

#include <iostream>
#include <random>
#include <sstream>

using namespace RAD_LIB_NAMESPACE;

namespace {

    void do_tests_internal() {
        // operator string + string_view
        {
            std::string str1 = "str1";
            std::string_view str2 = "str2";
            if (str1 + str2 != "str1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator string_view + string
        {
            std::string_view str1 = "str1";
            std::string str2 = "str2";
            if (str1 + str2 != "str1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator string_view + string_view
        {
            std::string_view str1 = "str1";
            std::string_view str2 = "str2";
            if (str1 + str2 != "str1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator string_view + const CharT*
        {
            std::string_view str1 = "str1";
            const char* str2 = "str2";
            if (str1 + str2 != "str1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator const CharT* + string_view
        {
            const char* str1 = "str1";
            std::string_view str2 = "str2";
            if (str1 + str2 != "str1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator string_view + CharT
        {
            std::string_view str1 = "str1";
            char str2 = '2';
            if (str1 + str2 != "str12") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // operator CharT + string_view
        {
            char str1 = '1';
            std::string_view str2 = "str2";
            if (str1 + str2 != "1str2") {
                throw std::runtime_error("[!] string append failed !");
            }
        }

        // iequal
        {
            static_assert(iequal("str1text2some", "StR1TeXT2SOme"));
            static_assert(iequal(L"str1text2some", L"StR1TeXT2SOme"));
            static_assert(iequal(u8"str1text2some", u8"StR1TeXT2SOme"));
            static_assert(iequal(u"str1text2some", u"StR1TeXT2SOme"));
            static_assert(iequal(U"str1text2some", U"StR1TeXT2SOme"));
        }

        // ifind
        {
            static_assert(ifind("findPos", "pOs") == 4);
            static_assert(ifind(L"search foR str", L"FoR") == 7);
            static_assert(ifind("where is the PoS after first poS "
                                "before third POS",
                                "POS", 14) == 29);
        }

        // starts_with
        {
            static_assert(starts_with("start-end", "start-"));
        }

        // istarts_with
        {
            static_assert(istarts_with("sTaRt-end", "StART-"));
        }

        // ends_with
        {
            static_assert(ends_with("start-end", "-end"));
        }

        // iends_with
        {
            static_assert(iends_with("start-eNd", "-EnD"));
        }

        // contains
        {
            static_assert(contains("textintext", "int"));
        }

        // icontains
        {
            static_assert(icontains("textinText", "iNt"));
        }

        // count_occurance
        {
            static_assert(count_occurance("how many how was seen as 'how' in "
                                          "these 'how's words",
                                          "how") == 4);
        }

        // icount_occurance
        {
            static_assert(icount_occurance("hOW many HoW was seen as 'HOW' "
                                           "in these 'HOw's words",
                                           "HOw") == 4);
        }

        // find_nth
        {
            static_assert(find_nth("where is the pos of third pos word in "
                                   "this text "
                                   "containg many pos at different poses",
                                   "pos", 2) == 62);
        }

        // ifind_nth
        {
            static_assert(ifind_nth("where is the pOS of third POS word in "
                                    "this text "
                                    "containg many Pos at different poSes",
                                    "PoS", 2) == 62);
        }

        // subview
        {
            std::string str = "substr this string using subview";
            if (subview(str, 7, 11) != "this string") {
                throw std::runtime_error("[!] string subview failed !");
            }
        }

        // erase_head
        {
            std::string str = "erase start of this string";
            erase_head(str, 6);
            if (str != "start of this string") {
                throw std::runtime_error("[!] string erase_head failed !");
            }
        }

        // erase_tail
        {
            std::string str =
                "start of this string end of this string erase it";
            erase_tail(str, 9);
            if (str != "start of this string end of this string") {
                throw std::runtime_error("[!] string erase_tail failed !");
            }
        }
    }

} // namespace

namespace tests_fn {
    bool do_string_tests() {
        try {
            do_tests_internal();
        }
        catch (const std::exception& ex) {
            std::cout << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] string tests passed\n";
        return true;
    }
} // namespace tests_fn
