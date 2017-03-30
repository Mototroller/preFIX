#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#define LOG_HEAD "[preFIX]: "

namespace preFIX {

    namespace details {
        
        template <int... Idx>
        struct int_seq {
            inline constexpr static std::array<int, sizeof...(Idx)> to_array() {
                return {{ Idx... }}; }
        };
        
        /// Finds minimal value inside sequence
        template <typename>
        struct min_idx;
        
        template <int X>
        struct min_idx<int_seq<X>> { enum : int { value = X }; };
        
        template <int Head, int... Tail>
        struct min_idx<int_seq<Head, Tail...>> {
            enum : int {
                min_tail = min_idx<int_seq<Tail...>>::value,
                value = Head < min_tail ? Head : min_tail
            };
        };
        
        /// Removes first found element from int_seq
        template <int, typename, typename = int_seq<>>
        struct filter_int_seq;
        
        template <int X, typename Seq2>
        struct filter_int_seq<X, int_seq<>, Seq2> {
            using type = Seq2; };
        
        template <int X, int... Seq1, int... Seq2>
        struct filter_int_seq<X, int_seq<X, Seq1...>, int_seq<Seq2...>> {
            using type = int_seq<Seq2..., Seq1...>; };
        
        template <int X, int Y, int... Seq1, int... Seq2>
        struct filter_int_seq<X, int_seq<Y, Seq1...>, int_seq<Seq2...>> {
            using type = typename filter_int_seq<X,
                int_seq<Seq1...>,
                int_seq<Seq2..., Y>
            >::type;
        };
        
        template <typename, typename = int_seq<>>
        struct sort_int_seq;
        
        template <int... Seq2>
        struct sort_int_seq<int_seq<>, int_seq<Seq2...>> {
            using type = int_seq<Seq2...>; };
        
        template <int... Seq1, int... Seq2>
        struct sort_int_seq<int_seq<Seq1...>, int_seq<Seq2...>> {
            enum : int { min = min_idx<int_seq<Seq1...>>::value };
            using filtered = typename filter_int_seq<min, int_seq<Seq1...>>::type;
            using type = typename sort_int_seq<filtered, int_seq<Seq2..., min>>::type;
        };
        
        /// Sorts given integer sequence (by selection sort)
        template <typename Seq>
        using sort_int_seq_t = typename sort_int_seq<Seq>::type;
        
        /// Allows to find index of given key/value in a sorted array
        template <int... Keys>
        struct index_map {
            enum : int { size = sizeof...(Keys) };
            using sorted_seq = sort_int_seq_t<int_seq<Keys...>>;
            
            /// Binary search algorithm
            static int idx_of(int key) {
                constexpr auto keys = sorted_seq::to_array();
                
                int ab[2] = {0, size};
                int idx;
                
                while(ab[0] != ab[1]) {
                    idx = (ab[0] + ab[1])/2;
                    int dir = (keys[idx] < key);
                    ab[1 - dir] = idx + dir;
                }
                
                idx = ab[0];
                return keys[idx] == key ? idx : size;
            }
        };
        
        
        /// Fixed size map, keys are integer numbers
        template <typename T, int... Keys>
        class map_array {
        private:
            std::array<T, sizeof...(Keys)> data_;
            
        public:
            using idx_map = index_map<Keys...>;
            
            map_array() = default;
            map_array(map_array const&) = default;
            map_array& operator=(map_array const&) = default;
            
            explicit map_array(T const& value) : data_{{value}} {}
            
            T const* find(int key) const {
                auto idx = idx_map::idx_of(key);
                return  idx != idx_map::size ? &data_[idx] : nullptr;
            }
            
            T* find(int key) {
                using cthis = map_array const*;
                return const_cast<T*>(static_cast<cthis>(this)->find(key));
            }
        };
        
        
        template <typename m1, typename m2>
        struct map_array_eq : std::integral_constant<bool, std::is_same<
            typename m1::idx_map::sorted_seq,
            typename m2::idx_map::sorted_seq
        >::value> {};
        
    } // details
    
    using size_t = std::size_t;
    enum : char { SOH = 0x1 };

    /// Peplaces SOH inside string by given symbol ('|' by default)
    std::string replace_SOH(std::string str, char symbol = '|') {
        std::replace(str.begin(), str.end(), char(SOH), symbol);
        return str;
    }
    
    
    using    Int_underlying = long;
    using  Float_underlying = double;
    using   Char_underlying = char;
    using String_underlying = std::string;
    
    /// Contains static value() function returning null-value of given type
    template <typename T>
    struct null_value;
    
    template <typename Underlying>
    struct null_value {
        static_assert(std::is_arithmetic<Underlying>::value, LOG_HEAD "null isn't defined");
        constexpr static Underlying value() { return std::numeric_limits<Underlying>::max(); }
    };
    
    template <>
    struct null_value<String_underlying> { static String_underlying value() { return {}; } };
    
    
    /// ------------------------! Read/write cursors !------------------------ ///
    
    /// Iterator-like class
    template <typename Ptr>
    class data_cursor_base {
    private:
        Ptr ptr_;
        int processed_;
        int left_;
    public:
        constexpr data_cursor_base(Ptr ptr, int size) :
            ptr_(ptr), processed_(0), left_(size) {}
        
        /// Moves cursor forward or backward
        data_cursor_base& step(int amount) {
            ptr_        += amount;
            processed_  += amount;
            left_       -= amount;
            return *this;
        }
        
        /// Same as step(amount)
        data_cursor_base& operator+=(int amount) {
            return step(amount); }
        
        /// Resets pointer to begining and assumes new data size
        data_cursor_base& reset(int new_left) {
            reset();
            left_ = new_left;
            return *this;
        }
        
        /// Resets cursor to the initial state
        data_cursor_base& reset() {
            return step(-processed_); }
        
        inline Ptr pointer()   const { return ptr_; }
        inline int processed() const { return processed_; }
        inline int left()      const { return left_; }
    };
    
    using write_cursor = data_cursor_base<char*>;
    using read_cursor  = data_cursor_base<char const*>;
}

#include <preFIX_config.hpp>

namespace preFIX {

/// Contains basic types arithmetics and mapping (FIX <=> native)
namespace types {
    
    struct fix_value_base {
        virtual bool   serialize(write_cursor& dst) const = 0;
        virtual bool deserialize(read_cursor&  src)       = 0;
        virtual ~fix_value_base() {}
    };
    
    template <
        typename Underlying,
        class serializer    = details::serializer  <Underlying>,
        class deserializer  = details::deserializer<Underlying>
    > struct fix_value_type : fix_value_base {
        using underlying_type   = Underlying;
        using serializer_type   = serializer;
        using deserializer_type = deserializer;
        
        underlying_type value;
        
        /// By default == null ("data is not presented")
        fix_value_type() : value(null_value<underlying_type>::value()) {}
        
        /// Implicit conversion
        fix_value_type(underlying_type const& v) : value(v) {}
        
        virtual ~fix_value_type() {}
        
        
        /// Sets value to null
        void clear() {
            value = null_value<underlying_type>::value(); }
        
        /// @returns if value != null
        bool present() const {
            return value != null_value<underlying_type>::value(); }
        
        
        virtual bool serialize(write_cursor& dst) const override {
            return serializer::serialize(dst, value); }
        
        virtual bool deserialize(read_cursor& src) override {
            //std::printf("-- deser of %s\n", typeid(*this).name());
            return deserializer::deserialize(src, value);
        }
    };
    
    
    /// ------------------------! Main FIX types !------------------------ ///
    
    using Int       = fix_value_type<   Int_underlying>;    // FIX int
    using Float     = fix_value_type< Float_underlying>;    // FIX float
    using Char      = fix_value_type<  Char_underlying>;    // FIX char
    using String    = fix_value_type<String_underlying>;    // FIX String
    
    template <size_t Width>
    using Fixed = fix_value_type<Int_underlying, details::fixed_width_int_serializer<Width>>;
    
    /// Writes preamble: "TAG=", @returns populated size
    bool serialize_tag(Int const& tag, write_cursor& dst) {
        return Int::serializer_type::serialize(dst, tag.value, '='); }
    
    /// Parses preamble: "TAG=" => tag, @returns read size
    bool deserialize_tag(Int& tag, read_cursor& src) {
        return Int::deserializer_type::deserialize(src, tag.value, '='); }
    
    /// TODO?
    bool skip_value(read_cursor& src) { return true; }
}



} // preFIX

#undef LOG_HEAD
