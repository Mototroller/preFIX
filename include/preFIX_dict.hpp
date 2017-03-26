#pragma once

#include <array>
#include <list>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <preFIX.hpp>

namespace preFIX { namespace dict {
    
    using namespace preFIX::types;
    
    namespace details {
        /// Contains index of type inside tuple-like class
        template <typename tuple, typename T>
        struct idx_of;
        
        template <template <class...> class tuple, typename T>
        struct idx_of<tuple<>, T> : std::integral_constant<size_t, 0> {
            static_assert(!sizeof(T), "can't find type"); };
        
        template <template <class...> class tuple, typename T, typename... V>
        struct idx_of<tuple<T, V...>, T> : std::integral_constant<size_t, 0> {};
        
        template <template <class...> class tuple, typename T, typename U, typename... V>
        struct idx_of<tuple<U, V...>, T> : std::integral_constant<size_t, (1 + idx_of<tuple<V...>, T>::value)> {};
        
        
        /// Recursively calls "serialize" for every tuple element
        template <size_t N = 0, typename Tuple>
        auto serialize_tuple(char* dst, Tuple const& t) ->
        typename std::enable_if<(N == std::tuple_size<Tuple>::value), size_t>::type {
            return 0; }
        
        template <size_t N = 0, typename Tuple>
        auto serialize_tuple(char* dst, Tuple const& t) ->
        typename std::enable_if<(N != std::tuple_size<Tuple>::value), size_t>::type {
            auto written = std::get<N>(t).serialize(dst); // member func
            return written + serialize_tuple<(N+1)>(dst + written, t);
        }
    } // details
    
    
    /// ------------------------! Main entries hierarchy !------------------------ ///
    
    /// Main tuple-like entity, contains fields to be serialized
    template <typename... T>
    class msg_t;
    
    /**
     * Aggregate type describing repeating group. Example:
     * TAG=GROUP|
     * TAG={NUM|HEAD=A|T1=B|T2=C|...HEAD=E|...TX=Z|}
     */
    template <typename Head, typename... Tail>
    struct Group {
        using group_element_type = msg_t<Head, Tail...>;
        using underlying_type = std::vector<group_element_type>;
        
        underlying_type value;
        
        void clear() {
            value.clear(); }
        
        bool present() const {
            return !value.empty(); }
        
        size_t serialize(char* dst) const {
            size_t written = 0, chunk = 0;
            
            chunk = Int::serialize(dst, int(value.size()));
            written += chunk;
            dst     += chunk;
            
            for(auto const& msg : value) {
                chunk = msg.serialize(dst);
                written += chunk;
                dst     += chunk;
            }
            
            return written;
        }
        
        size_t deserialize(char const* src) {
            size_t read = 0, chunk = 0;
            Int::underlying_type group_size;
            chunk = Int::deserialize(src, group_size);
            read    += chunk;
            src     += chunk;
            
            for(int i = 0; i < group_size; ++i) {
                // msg.deserialize
            }
            
            return read;
        }
        
        /// ------------------------! Group interface !------------------------ ///
        
        Group& resize(size_t new_size) {
            clear();
            value.resize(new_size);
            return *this;
        }
        
        group_element_type const& operator[](size_t idx) const {
            return value[idx]; }
        
        group_element_type& operator[](size_t idx) {
            return value[idx]; }
    };
    
    /**
     * Base class for all fields. T requirements:
     * - t.value (optional, instantiates on demand)
     * - t.clear()
     * - t.present()
     * - t.serialize(dst)
     */
    template <int tag_value, typename T>
    struct field_base : T {
        using type = T;
        enum : int { tag = tag_value };
        
        /// Performs standard serialization: "TAG=VALUE<SOH>", @returns written size
        size_t serialize(char* dst) const {
            if(type::present()) {
                size_t written = 0, chunk = 0;
                chunk = types::serialize_tag(dst, tag);
                written += chunk;
                dst     += chunk;
                return written + type::serialize(dst);
            }
            return 0;
        }
        
        size_t deserialize(char const* src) {
            size_t read = 0, chunk = 0;
            int tag_val;
            chunk = types::deserialize_tag(src, tag_val);
            if(tag_val == tag) {
                read    += chunk;
                src     += chunk;
                return read + type::deserialize(src);
            }
            
            type::clear();
            return 0;
        }
        
        /// TODO: ADL + friend = WIN!
        // friend void ololo(field_base const&) {}
    };
    
    /// Alias for field containing repeating group
    template <int tag_value, typename Head, typename... Tail>
    using group_base = field_base<tag_value, Group<Head, Tail...>>;
    
    template <typename... T>
    class msg_t {
    private:
        using tuple_t = std::tuple<T...>;
        tuple_t fields_;
        
        template <typename U, size_t idx = details::idx_of<tuple_t, U>::value>
        inline U const& get_field() const {
            return std::get<idx>(fields_); }
        
        template <typename U, size_t idx = details::idx_of<tuple_t, U>::value>
        inline U& get_field() {
            return std::get<idx>(fields_); }
        
    public:
        template <typename U>
        inline U const& at() const {
            return get_field<U>(); }
        
        template <typename U>
        inline U& at() {
            return get_field<U>(); }
        
        /// Assigns given value to field's data
        template <typename U, typename Arg>
        msg_t& set(Arg&& arg) {
            auto& field = at<U>();
            field.value = std::forward<Arg>(arg);
            return *this;
        }
        
        /// Set field to null == omitting during serialization
        template <typename U>
        void clear() {
            at<U>().clear(); }
        
        /// Extracts value from field
        template <typename U, typename Arg>
        msg_t const& get(Arg&& arg) const {
            std::forward<Arg>(arg) = at<U>().value;
            return *this;
        }
        
        /// Recursively serializes fields to given buffer
        size_t serialize(char* dst) const {
            return details::serialize_tuple(dst, fields_); }
    };
    
    
    /// ------------------------! Mandatory fields !------------------------ ///
    
    struct BeginString  : field_base<8,     String      >{}; // Header.BeginString
    struct Length       : field_base<9,     Fixed<5>    >{}; // Header.Length
    struct CheckSum     : field_base<10,    Fixed<3>    >{}; // Trailer.CheckSum
    
    namespace details {
        /// Performs header+body serialization and Length calculation
        template <typename H, typename Msg>
        size_t serialize_body(char* dst, H& header, Msg const& msg) {
            char* b_ptr = dst;
            size_t written = 0, chunk = 0;
            header.template set<Length>(0);
            
            chunk = header.serialize(dst);
            written += chunk;
            dst     += chunk;
            
            chunk = msg.serialize(dst);
            written += chunk;
            dst     += chunk;
            
            auto l_ptr = std::find(b_ptr, dst, char(SOH)) + 1;
            auto r_ptr = std::find(l_ptr, dst, char(SOH)) + 1;
            
            header.template set<Length>(written - (r_ptr - b_ptr));
            header.template at<Length>().serialize(l_ptr);
            
            return written;
        }
        
        /// Performs trailer serialization and CheckSum calculation
        template <typename T>
        size_t serialize_trailer(char* base, size_t length, T& trailer) {
            int sum = 0;
            for(size_t i = 0; i < length; ++i)
                sum += int(base[i]);
            
            trailer.template set<CheckSum>(sum % 256);
            return trailer.serialize(base + length);
        }
    } // details
    
    /// Serializes given message (header+body+trailer) and fills Length and CheckSum
    template <typename H, typename Msg, typename T>
    size_t serialize_message(char* dst, H& header, Msg const& msg, T& trailer) {
        size_t written = details::serialize_body(dst, header, msg); // so good
        return written + details::serialize_trailer(dst, written, trailer);
    }
    
    
    /// ------------------------! Deserialization !------------------------ ///
    
    /// tag <=> data coordinate
    using offsets_map = std::unordered_multimap<int, size_t>;
    
    offsets_map build_offsets_map(char const* src, size_t size) {
        offsets_map map; int tag;
        for(size_t offset = 0; offset < size;) {
            auto read = deserialize_tag(src + offset, tag);
            map.emplace(tag, offset);
            offset += read;
            if(tag == CheckSum::tag) break;
            while((offset < size) && (src[offset++] != SOH));
        }
        return map;
    }
    
    template <typename Field>
    Field extract_field(char const* src, offsets_map const& offsets) {
        Field field;
        auto it = offsets.find(Field::tag);
        if(it != offsets.cend())
            field.deserialize(src + it->second);
        return field;
    }
    
    template <typename Field, typename Msg>
    bool transfer_field(char const* src, Msg& message, offsets_map const& offsets) {
        auto it = offsets.find(Field::tag);
        if(it != offsets.cend()) {
            auto& field = message.template at<Field>();
            field.deserialize(src + it->second);
            return true;
        }
        return false;
    }

} // dict
} // preFIX

/// Example of FIX-dictionary
namespace test_dict {
    using namespace preFIX::types; // isn't necessary
    using namespace preFIX::dict;
    
    /// via "struct"
    struct MsgType      : field_base<35,    String> {};
    struct SenderCompID : field_base<49,    String> {};
    struct TargetCompID : field_base<56,    String> {};
    struct MsgSeqNum    : field_base<34,    Int>    {};
    struct PossDupFlag  : field_base<43,    Char>   {};
    
    /// via "using"
    using Account       = field_base<1,     String>;
    using EncryptMethod = field_base<98,    Int>;
    using HeartBtInt    = field_base<108,   Int>;
    using Password      = field_base<554,   String>;
    
    struct ClOrdID      : field_base<11,    String> {};
    struct PartyID      : field_base<448,   String> {};
    struct PartyIDSource: field_base<447,   Char>   {};
    struct PartyRole    : field_base<452,   Int>    {};
    struct Price        : field_base<44,    Float>  {};
    struct Side         : field_base<54,    Char>   {};
    
    
    using NoPartyID = group_base<453,
        PartyID,
        PartyIDSource,
        PartyRole
    >;
    
    using Header = msg_t<
        BeginString,
        Length,
        MsgType,
        SenderCompID,
        TargetCompID,
        MsgSeqNum,
        PossDupFlag
    >;
    
    using NewOrderSingle = msg_t<
        ClOrdID,
        Account,
        NoPartyID,
        Price,
        Side
    >;
    
    using Trailer = msg_t<
        CheckSum
    >;
    
    
    struct NoOrders : group_base<999,
        ClOrdID,
        NoPartyID
    > {};
    
    using NestedGroupsOrder = msg_t<
        Account,
        NoOrders,
        Password
    >;

} // test_dict
