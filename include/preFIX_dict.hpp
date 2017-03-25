#pragma once

#include <array>
#include <tuple>
#include <unordered_map>

#include <preFIX.hpp>

namespace preFIX {

namespace dict {
    
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
    }
    
    /// Main tuple-like entity, contains fields to be serialized
    template <typename... T>
    class msg_t;
    
    /// Base class for all plain fields
    template <int tag_value, typename FIX_type>
    struct field_base {
        using type = FIX_type;
        enum : int { tag = tag_value };
        
        type data;
        bool present = false;
        
        /// Performs standard serialization: "TAG=VALUE<SOH>", @returns written size
        size_t serialize(char* dst) const {
            if(present) {
                size_t written = 0, chunk = 0;
                chunk = types::serialize_tag(dst, tag);
                written += chunk;
                dst     += chunk;
                return written + data.serialize(dst);
            }
            return 0;
        }
    };
    
    /// Base class for repetaing groups
    template <int tag_value, size_t Size, typename... Fields>
    struct group_base {
        using atom_type = msg_t<Fields...>;
        using type = std::array<atom_type, Size>;
        enum : int { tag = tag_value };
        
        type data;
        bool present = false;
        
        size_t serialize(char* dst) const {
            if(present) {
                int group_size = 0;
                for(auto const& msg : data)
                    group_size += msg.is_group_presented();
                
                if(group_size > 0) {
                    size_t written = 0, chunk = 0;
                    
                    chunk = types::serialize_tag(dst, tag);
                    written += chunk;
                    dst     += chunk;
                    
                    chunk = types::Int::serialize(dst, int(group_size));
                    written += chunk;
                    dst     += chunk;
                    
                    for(auto const& msg : data) {
                        chunk = msg.serialize(dst);
                        written += chunk;
                        dst     += chunk;
                    }
                    
                    return written;
                }
            }
            return 0;
        }
        
        size_t max_size() const {
            return data.size(); }
        
        atom_type const& operator[](size_t idx) const {
            return data[idx]; }
        
        atom_type& operator[](size_t idx) {
            return data[idx]; }
    };
    
    template <typename... T>
    class msg_t {
    private:
        using tuple_t = std::tuple<T...>;
        tuple_t fields_;
        
        template <typename U, size_t idx = details::idx_of<tuple_t, U>::value>
        inline auto get_field() -> decltype(std::get<idx>(fields_)) {
            return std::get<idx>(fields_); }
        
        template <typename U, size_t idx = details::idx_of<tuple_t, U>::value>
        inline auto get_field() const -> decltype(std::get<idx>(fields_)) {
            return std::get<idx>(fields_); }
        
    public:
        /// For groups: @returns if first field of group was set
        bool is_group_presented() const {
            return std::get<0>(fields_).present; }
        
        /// Assigns given value to field's data
        template <typename U, typename Arg>
        void set(Arg&& arg) {
            auto& field = get_field<U>();
            field.data = std::forward<Arg>(arg);
            field.present = true;
        }
        
        /// Returns mutable reference to underlying field/group
        template <typename U>
        U& set() {
            auto& field = get_field<U>();
            field.present = true;
            return field;
        }
        
        /// Set field to be omitted during serialization
        template <typename U>
        void omit() {
            get_field<U>().present = false; }
        
        /// Extracts value from field
        template <typename U, typename Arg>
        void get(Arg&& arg) const {
            std::forward<Arg>(arg) = get_field<U>().data; }
        
        /// Returns constant reference to underlying field/group
        template <typename U>
        U const& get() const {
            return get_field<U>(); }
        
        /// Recursively serializes fields to given buffer
        size_t serialize(char* dst) const {
            return details::serialize_tuple(dst, fields_); }
    };
    
    struct BeginString  : field_base<8,     String      >{};
    struct Length       : field_base<9,     Fixed<5>    >{};
    struct CheckSum     : field_base<10,    Fixed<3>    >{};
    
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
        header.template get<Length>().serialize(l_ptr);
        
        return written;
    }
    
    size_t serialize_chksum(char* base, size_t length) {
        int sum = 0;
        for(size_t i = 0; i < length; ++i)
            sum += int(base[i]);
        
        using CS = msg_t<CheckSum>;
        CS cs; cs.set<CheckSum>(sum % 256);
        return cs.serialize(base + length);
    }

} // dict
    
} // preFIX

/// Example of FIX-dictionary
namespace test_dict {
    using namespace preFIX::types; // isn't necessary
    using namespace preFIX::dict;
    
    
    struct MsgType      : field_base<35,    String> {};
    struct SenderCompID : field_base<49,    String> {};
    struct TargetCompID : field_base<56,    String> {};
    struct MsgSeqNum    : field_base<34,    Int>    {};
    
    struct Account      : field_base<1,     String> {};
    struct EncryptMethod: field_base<98,    Int>    {};
    struct HeartBtInt   : field_base<108,   Int>    {};
    struct Password     : field_base<554,   String> {};
    
    struct ClOrdID      : field_base<11,    String> {};
    struct PartyID      : field_base<448,   String> {};
    struct PartyIDSource: field_base<447,   Char>   {};
    struct PartyRole    : field_base<452,   Int>    {};
    struct Price        : field_base<44,    Float>  {};
    struct Side         : field_base<54,    Char>   {};
    
    
    struct NoPartyID : group_base<453, 3,
        PartyID,
        PartyIDSource,
        PartyRole
    > {};
    
    using Header = msg_t<
        BeginString,
        Length,
        MsgType,
        SenderCompID,
        TargetCompID,
        MsgSeqNum
    >;
    
    using NewOrderSingle = msg_t<
        ClOrdID,
        Account,
        NoPartyID,
        Price,
        Side
    >;

} // test_dict
