#pragma once
#include <rad/libbase.h>

#include <optional>
#include <variant>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
        template <class... MatchArms>
        struct matcher : MatchArms... {
            template <class... Arms>
            constexpr matcher(Arms&&... arms)
                : MatchArms{std::forward<Arms>(arms)}... {
            }

            using MatchArms::operator()...;
        };

        template <class... Args>
        struct is_instance_of_variant : std::false_type {};

        template <class... Args>
        struct is_instance_of_variant<std::variant<Args...>> : std::true_type {
        };
    } // namespace detail

    template <class Variant>
    concept instance_of_variant =
        detail::is_instance_of_variant<std::remove_cvref_t<Variant>>::value;

    template <class T>
    concept visit_match_arm =
        (std::copy_constructible<std::remove_cvref_t<T>> ||
         std::move_constructible<std::remove_cvref_t<T>>) &&
        !std::is_final_v<T>;

    template <instance_of_variant Variant, visit_match_arm... MatchArms>
    constexpr decltype(auto) match(Variant&& var, MatchArms&&... match_arms) {
        using matcher_fns = detail::matcher<std::remove_cvref_t<MatchArms>...>;
        return std::visit(matcher_fns{std::forward<MatchArms>(match_arms)...},
                          std::forward<Variant>(var));
    }

    template <class T, class Var>
    std::optional<T> if_let(Var&& var) {
        if (std::holds_alternative<T>(var)) {
            return std::get<T>(std::forward<Var>(var));
        }
        else {
            return std::nullopt;
        }
    }
} // namespace RAD_LIB_NAMESPACE
