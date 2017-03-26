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

/// Contains basic types arithmetics and mapping (FIX <=> native)
namespace types {
    namespace details {
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
        
        /// Special serializer for fixed-width integer (000123)
        template <size_t Width>
        struct fixed_width_int_serializer {
            template <typename U>
            static size_t serialize(char* dst, U&& value, char delimiter = SOH) {
                std::ostringstream ss;
                bool neg = (value < 0);
                ss << (neg ? "-" : "")
                   << std::setfill('0')
                   << std::setw(Width - neg)
                   << std::abs(std::forward<U>(value));
                std::string str = ss.str();
                std::memcpy(dst, str.data(), str.size());
                dst[str.size()] = delimiter;
                return str.size() + 1;
            }
        };
    } // details
    
    /// Contains static value() function returning null-value of given type
    template <typename T>
    struct null;
    
    /// Base class for all FIX data types
    template <
        typename T,
        class serializer    = details::sstream_serializer<T>,
        class deserializer  = details::sstream_deserializer<T>
    > struct base_t;
    
    template <typename T, class serializer, class deserializer>
    struct base_t : serializer, deserializer {
        using underlying_type = T;
        underlying_type value;
        
        /// By default == null (data is not presented)
        base_t() : value(null<base_t>::value()) {}
        base_t(underlying_type const& v) : value(v) {}
        
        /// Sets value to null
        void clear() {
            value = null<base_t>::value(); }
        
        /// @returns if value != null
        bool present() const {
            return value != null<base_t>::value(); }
        
        using serializer::serialize;
        using deserializer::deserialize;
        
        size_t serialize(char* dst) const {
            return serialize(dst, value); }
        
        size_t deserialize(char const* src) {
            return deserialize(src, value); }
    };
    
    
    /// ------------------------! Main FIX types !------------------------ ///
    
    using Int       = base_t<long>;         // FIX int
    using Float     = base_t<double>;       // FIX float
    using Char      = base_t<char>;         // FIX char
    using String    = base_t<std::string>;  // FIX String
    
    template <size_t Width>
    using Fixed = base_t<int, details::fixed_width_int_serializer<Width>>;
    
    
    /// ------------------------! Their NULL values !------------------------ ///
    
    template <typename Base, typename U, typename V>
    struct null<base_t<Base,U,V>> {
        static_assert(std::is_arithmetic<Base>::value, LOG_HEAD "null isn't defined");
        constexpr static Base value() { return std::numeric_limits<Base>::max(); }
    };
    
    template <>
    struct null<String> { static typename String::underlying_type value() { return {}; } };
    
    
    /// Writes preamble: "TAG=", @returns populated size
    size_t serialize_tag(char* dst, int tag) {
        return types::Int::serialize(dst, tag, '='); }
    
    /// Parses preamble: "TAG=" => tag, @returns read size
    size_t deserialize_tag(char const* src, int& tag) {
        return types::Int::deserialize(src, tag, '='); }
}



} // preFIX

#undef LOG_HEAD
