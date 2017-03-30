#pragma once

#include <array>
#include <bitset>
#include <list>
#include <map>
#include <tuple>
#include <unordered_set>
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
    struct Group : fix_value_base {
        using group_element_type = msg_t<Head, Tail...>;
        using underlying_type = std::vector<group_element_type>;
        
        underlying_type value;
        
        void clear() {
            value.clear(); }
        
        bool present() const {
            return !value.empty(); }
        
        virtual bool serialize(write_cursor& dst) const override {
            Int group_size(value.size());
            if(group_size.serialize(dst)) {
                for(auto const& msg : value)
                    if(!msg.serialize(dst))
                        return false;
                return true;
            }
            return false;
        }
        
        virtual bool deserialize(read_cursor& src) override {
            Int group_size;
            if(group_size.deserialize(src)) {
                resize(group_size.value);
                for(int i = 0; i < group_size.value; ++i)
                    if(!value[i].deserialize(src))
                        return false;
                return true;
            }
            return false;
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
        bool serialize(write_cursor& dst) const {
            if(type::present()) {
                Int tag_val(tag);
                if(serialize_tag(tag_val, dst))
                    return type::serialize(dst);
                return false;
            }
            return true;
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
        
        bool deserialize_impl(read_cursor& src) {
            using idx_map = preFIX::details::index_map<(T::tag)...>;
            using filler = const int[sizeof...(T)];
            
            std::bitset<sizeof...(T)> found_idxes{};
            std::array<fix_value_base*, sizeof...(T)> ptrs_arr;
            (void)filler{(ptrs_arr[idx_map::idx_of(T::tag)] = &get_field<T>(), 0)...};
            
            while(src.left() > 0) {
                // Here we have unread data
                
                Int tag;
                auto left = src.left();
                
                if(!deserialize_tag(tag, src))
                    return false;
                
                int idx = idx_map::idx_of(tag.value);
                
                // If tag is belonging to msg
                if(idx != idx_map::size && !found_idxes[idx]) {
                    found_idxes[idx].flip();
                    auto value_ptr = ptrs_arr[idx];
                    if(!value_ptr->deserialize(src))
                        return false;
                
                // If tag has repeated or doesn't belong to msg
                } else {
                    src.step(src.left() - left); // step backward
                    break;
                }
            }
            // src.left() == 0 or unrecognized tag
            return true;
        }
        
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
            at<U>().value = std::forward<Arg>(arg);
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
        bool serialize(write_cursor& dst) const {
            bool res[] = {(get_field<T>().serialize(dst))...};
            return std::accumulate(std::begin(res), std::end(res), 0) == sizeof...(T);
        }
        
        /// Recursively parses given buffer
        bool deserialize(read_cursor& src) {
            return deserialize_impl(src); }
    };
    
    
    /// ------------------------! Mandatory fields !------------------------ ///
    
    struct BeginString  : field_base<8,     String      >{}; // Header.BeginString
    struct Length       : field_base<9,     Fixed<5>    >{}; // Header.Length
    struct CheckSum     : field_base<10,    Fixed<3>    >{}; // Trailer.CheckSum
    
    namespace details {
        /// Performs header+body serialization and Length calculation
        template <typename H, typename Msg>
        bool serialize_body(write_cursor& dst, H& header, Msg const& msg) {
            char* b_ptr = dst.pointer();
            int init_left = dst.left();
            
            header.template set<Length>(0);
            
            if(!header.serialize(dst) || !msg.serialize(dst))
                return false;
            
            auto l_ptr = std::find(b_ptr, dst.pointer(), char(SOH)) + 1;
            auto r_ptr = std::find(l_ptr, dst.pointer(), char(SOH)) + 1;
            
            int written = init_left - dst.left();
            header.template set<Length>(written - (r_ptr - b_ptr));
            
            write_cursor lc(l_ptr, dst.pointer() - l_ptr);
            return header.template at<Length>().serialize(lc);
        }
        
        /// Performs trailer serialization and CheckSum calculation
        template <typename T>
        bool serialize_trailer(write_cursor& dst, T& trailer) {
            auto base = dst.pointer() - dst.processed();
            int sum = 0;
            for(int i = 0; i < dst.processed(); ++i)
                sum += int(base[i]);
            
            trailer.template set<CheckSum>(sum % 256);
            return trailer.serialize(dst);
        }
    } // details
    
    /// Serializes given message (header+body+trailer) and fills Length and CheckSum
    template <typename H, typename Msg, typename T>
    bool serialize_message(write_cursor& dst, H& header, Msg const& msg, T& trailer) {
        return  details::serialize_body(dst, header, msg) &&
                details::serialize_trailer(dst, trailer);
    }
    
    
    /// ------------------------! Deserialization !------------------------ ///
    
    

} // dict
} // preFIX

/// Example of FIX-dictionary
namespace test_dict {
    using namespace preFIX::types; // isn't necessary
    using namespace preFIX::dict;
    
    /// via "struct"
    using MsgType       = field_base<35,    String>;
    using SenderCompID  = field_base<49,    String>;
    using TargetCompID  = field_base<56,    String>;
    using MsgSeqNum     = field_base<34,    Int>;
    using PossDupFlag   = field_base<43,    Char>;
    
    /// via "using"
    using Account       = field_base<1,     String>;
    using EncryptMethod = field_base<98,    Int>;
    using HeartBtInt    = field_base<108,   Int>;
    using Password      = field_base<554,   String>;
    
    using ClOrdID       = field_base<11,    String>;
    using PartyID       = field_base<448,   String>;
    using PartyIDSource = field_base<447,   Char>;
    using PartyRole     = field_base<452,   Int>;
    using Price         = field_base<44,    Float>;
    using Side          = field_base<54,    Char>;
    
    
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
    
    
    using NoOrders = group_base<999,
        ClOrdID,
        NoPartyID
    >;
    
    using NestedGroupsOrder = msg_t<
        Account,
        NoOrders,
        Password
    >;

} // test_dict
