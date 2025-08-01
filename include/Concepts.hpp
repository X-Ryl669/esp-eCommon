#ifndef hpp_Concepts_hpp
#define hpp_Concepts_hpp

#include <initializer_list>
#include <type_traits>

/** A collection of useful concept that's simplifying the code a lot */
namespace Concepts
{
    template <typename>                     struct IsStdInitList : std::false_type { };
    template <typename T>                   struct IsStdInitList<std::initializer_list<T>> : std::true_type { };
    /** Check if the given type is an initializer list */
    template <typename T>                   constexpr bool is_stdinitlist_v = IsStdInitList<T>::value;

    /** Check if the Returned type is compatible with the Required type */
    template <typename Returned, typename Required> constexpr bool compatible_return_type_v = std::is_void_v<Required> || std::is_convertible_v<Returned, Required>;

    namespace Details { struct Unobtainium {}; }
    template <class... T> struct always_false : std::false_type {};
    template <> struct always_false<Details::Unobtainium> : std::true_type {};
}

#endif
