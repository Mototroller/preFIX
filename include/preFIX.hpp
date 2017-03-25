#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#define LOG_HEAD "[preFIX]: "

namespace preFIX {

using size_t = std::size_t;

enum : char { SOH = 0x1 };

/// Peplaces SOH inside string by given symbol ('|' by default)
std::string replace_SOH(std::string str, char symbol = '|') {
    std::replace(str.begin(), str.end(), char(SOH), symbol);
    return str;
}

/*// Contains compile-time djb2 hash implementation
namespace hash {
    constexpr size_t djb2_impl(char const* str, size_t hash) {
        return  *str == char{0} ? hash :
                djb2_impl(str + 1, ((hash << 5) + hash) + *str);
    }

    constexpr size_t djb2(char const* str) {
        return djb2_impl(str, 5381); }
    
    /// some_tpl<H("string")>
    constexpr size_t H(char const* str) {
        return djb2(str); }
    
    /// some_tpl<"string"__>
    constexpr size_t operator ""__(char const* str, size_t) {
        return H(str); }
}

namespace meta {
    /// Concatenates two tuple-like classes
    template <class U, class V>
    struct tuple_concat;

    template <
        template <class...> class T,
        class... Alist,
        class... Blist
    > struct tuple_concat<T<Alist...>, T<Blist...>> { using type = T<Alist..., Blist...>; };

    template <typename U, typename V>
    using tuple_concat_t = typename tuple_concat<U,V>::type;


    /// Pushes type to tuple-like class
    template <class Tuple, class T>
    struct tuple_push;

    template <
        template <class...> class Tuple,
        class... Args,
        class T
    > struct tuple_push<Tuple<Args...>,T> { using type = Tuple<Args..., T>; };

    template <typename Tuple, typename T>
    using tuple_push_t = typename tuple_push<Tuple,T>::type;
}
//*/

/// Contains basic types arithmetics and mapping (FIX <=> native)
namespace types {
    
    /// Default serializer: {ostringstream(dst) << value}
    template <typename T>
    struct sstream_serializer {
        template <typename U>
        static size_t serialize(char* dst, U&& value, char delimiter = SOH) {
            std::ostringstream ss; ss << std::forward<U>(value);
            std::string str = ss.str();
            std::memcpy(dst, str.data(), str.size());
            dst[str.size()] = delimiter;
            return str.size() + 1;
        }
    };
    
    /// Default deserializer: {istringstream(src) >> value}
    template <typename T>
    struct sstream_deserializer {
        template <typename U>
        static size_t deserialize(char const* src, U& value, char delimiter = SOH) {
            std::string str(src, static_cast<char const*>(std::strchr(src, delimiter)));
            std::istringstream ss(str); ss >> value;
            return str.size() + 1;
        }
    };
    
    /// Special serializer
    template <size_t Width>
    struct fixed_width_int_serializer {
        template <typename U>
        static size_t serialize(char* dst, U&& value, char delimiter = SOH) {
            std::ostringstream ss;
            ss << std::setfill('0') << std::setw(Width) << std::forward<U>(value);
            std::string str = ss.str();
            std::memcpy(dst, str.data(), str.size());
            dst[str.size()] = delimiter;
            return str.size() + 1;
        }
    };
    
    template <
        typename T,
        class serializer    = sstream_serializer<T>,
        class deserializer  = sstream_deserializer<T>
    >
    struct base_t : serializer, deserializer {
        using underlying_type = T;
        underlying_type value;
        
        base_t() = default;
        explicit base_t(underlying_type const& v) : value(v) {}
        
        template <typename U>
        base_t& operator=(U&& v) {
            value = std::forward<U>(v);
            return *this;
        }
        
        using serializer::serialize;
        using deserializer::deserialize;
        
        size_t serialize(char* dst) const {
            return serialize(dst, value); }
        
        size_t deserialize(char const* src) {
            return deserialize(src, value); }
    };
    
    using Int       = base_t<size_t>;
    using Float     = base_t<double>;
    using Char      = base_t<char>;
    using String    = base_t<std::string>;
    
    template <size_t Width>
    using Fixed = base_t<int, fixed_width_int_serializer<Width>>;
    
    /// Writes preamble: "TAG=", @returns populated size
    size_t serialize_tag(char* dst, int tag) {
        return types::Int::serialize(dst, tag, '='); }
    
    /// Parses preamble: "TAG=" => tag, @returns read size
    size_t deserialize_tag(char const* src, int& tag) {
        return types::Int::deserialize(src, tag, '='); }
}



} // preFIX

#undef LOG_HEAD
