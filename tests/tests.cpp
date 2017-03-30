#include <ax.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include <preFIX.hpp>
#include <preFIX_dict.hpp>

using namespace ax;

int main() {
    using namespace preFIX;
    using namespace preFIX::types;
    
    char buf[1_KIB] = {0};
    std::string str;
    
    auto clrbuf = [&buf]{ std::memset(buf, 0x0, sizeof(buf)); };
    
    write_cursor wc(buf, sizeof(buf));
    read_cursor  rc(buf, sizeof(buf));
    
    {
        Int i;
        
        i.value = 13;
        i.serialize(wc.reset());
        str = buf;
        str = str.substr(0, str.size() - 1);
        stdcout(str);
        
        i.clear();
        i.deserialize(rc.reset(wc.processed()));
        stdcout(i.value);
        
        LIGHT_TEST(wc.processed() == rc.processed());
    }
    
    clrbuf();
    
    {
        String s;
        
        s.value = "DHKDHK//HKHKHHJ";
        s.serialize(wc.reset());
        str = buf;
        str = str.substr(0, str.size() - 1);
        stdcout(str);
        
        s.clear();
        s.deserialize(rc.reset(wc.processed()));
        stdcout(s.value);
        
        LIGHT_TEST(wc.processed() == rc.processed());
    }
    
    clrbuf();
    
    {
        using namespace test_dict;
        
        Header header;
        
        header.set<MsgType>("A");
        
        // Chaining
        header.set<BeginString> ("FIX.4.4")
              .set<SenderCompID>("MYCOMP")
              .set<TargetCompID>("THEIRTCOMP")
              .set<MsgSeqNum>   (1);
        
        // Omitting
        header.set<PossDupFlag>('Y');
        header.clear<PossDupFlag>();
        
        LIGHT_TEST(header.serialize(wc.reset()));
        stdcout(replace_SOH(buf));
        
        clrbuf();
        
        NewOrderSingle nos;
        nos.set<ClOrdID>("123ABC");
        nos.set<Account>("ololo//OLOLO");
        nos.set<Price>  (66.6625);
        nos.set<Side>   ('2');
        
        // Group => 3 entries, chaining example
        nos.at<NoPartyID>().resize(3);
        nos.at<NoPartyID>()[0]
            .set<PartyID>("USER")
            .set<PartyRole>(12)
            .set<PartyIDSource>('X');
        
        nos.at<NoPartyID>()[1]
            .set<PartyID>("FIRM")
            .set<PartyIDSource>('Y');
        
        nos.at<NoPartyID>()[2].set<PartyID>("KGB");
        
        {
            clrbuf();
            LIGHT_TEST(nos.serialize(wc.reset()));
            stdcout(replace_SOH(buf), "<---- ser");
            
            NewOrderSingle nos2;
            LIGHT_TEST(nos2.deserialize(rc.reset(wc.processed())));
            
            LIGHT_TEST(wc.processed() == rc.processed());
            stdprintf("%% %%", wc.processed(), rc.processed());
            
            clrbuf();
            LIGHT_TEST(nos2.serialize(wc.reset()));
            stdcout(replace_SOH(buf), "<---- des");
        }
        
        clrbuf();
        
        Trailer trailer;
        LIGHT_TEST(serialize_message(wc.reset(), header, nos, trailer));
        stdcout(replace_SOH(buf));
        
        /// Perf
        if(1) {
            const size_t N = 16_KIB;
            size_t t = rdtsc();
            for(size_t i = 0; i < N; ++i)
                serialize_message(wc.reset(), header, nos, trailer);
            t = rdtsc() - t;
            stdprintf("=== Encoding NOS: %% ticks/msg, %% ticks/B",
                double(t)/N, double(t)/(N*wc.processed()));
        }
        
        {
            clrbuf();
            header.serialize(wc.reset());
            stdcout(replace_SOH(buf), "<---- ser");
            
            Header h2;
            h2.deserialize(rc.reset());
            
            clrbuf();
            h2.serialize(wc.reset());
            stdcout(replace_SOH(buf), "<---- des");
        }
        
        /// Perf
        if(1) {
            clrbuf();
            nos.serialize(wc.reset());
            
            NewOrderSingle n2;
            const size_t N = 16_KIB;
            auto t = rdtsc();
            for(size_t i = 0; i < N; ++i)
                n2.deserialize(rc.reset(wc.processed()));
            t = rdtsc() - t;
            stdprintf("=== Decoding NOS: %% ticks/msg, %% ticks/B",
                double(t)/N, double(t)/(N*rc.processed()));
            LIGHT_TEST(wc.processed() == rc.processed());
        }
    }
    
    clrbuf();
    
    {
        using namespace test_dict;
        
        NestedGroupsOrder batch;
        
        batch.set<Account>("Nested!");
        batch.set<Password>("PSSWD");
        
        std::array<std::string,3> ids = {"aaa", "bbb", "ccc"};
        batch.at<NoOrders>().resize(ids.size());
        
        for(size_t i = 0; i < ids.size(); ++i) {
            auto& order = batch.at<NoOrders>()[i];
            order.set<ClOrdID>(ids[i]);
            
            std::array<std::string,3> parties = {"YOU", "ME", "KGB"};
            order.at<NoPartyID>().resize(parties.size());
            
            for(size_t j = 0; j < parties.size(); ++j)
                order.at<NoPartyID>()[j]
                    .set<PartyID>  (parties[j])
                    .set<PartyRole>(0);
        }
        
        LIGHT_TEST(batch.serialize(wc.reset()));
        stdcout(replace_SOH(buf), "<---- ser");
        
        if(1) {
            NestedGroupsOrder n2;
            LIGHT_TEST(n2.deserialize(rc.reset(wc.processed())));
            LIGHT_TEST(wc.processed() == rc.processed());
            
            clrbuf();
            LIGHT_TEST(n2.serialize(wc.reset()));
            stdcout(replace_SOH(buf), "<---- des");
        }
        
        /// Perf
        if(1) {
            NestedGroupsOrder n2;
            const size_t N = 16_KIB;
            auto t = rdtsc();
            for(size_t i = 0; i < N; ++i)
                n2.deserialize(rc.reset(wc.processed()));
            t = rdtsc() - t;
            stdprintf("=== Decoding NESTED: %% ticks/msg, %% ticks/B",
                double(t)/N, double(t)/(N*rc.processed()));
            LIGHT_TEST(wc.processed() == rc.processed());
        }
    }
    
    {
        using namespace dict;
        using namespace preFIX::details;
        
        using map1 = index_map<4,8,15,16,23,42>;
        using map2 = index_map<16,8,23,42,15,4>;
        
        for(int i = 0; i < 50; ++i) {
            LIGHT_TEST(map1::idx_of(i) == map2::idx_of(i));
        }
        
        std::array<int, 6> keys = {4,8,15,16,23,42};
        map_array<int, 4,8,15,16,23,42> m1(-1);
        map_array<int, 16,8,23,42,15,4> m2(-1);
        map_array<int, 42,23,16,15,8,4> m3(-1);
        
        static_assert(map_array_eq<decltype(m1), decltype(m2)>::value, "");
        static_assert(map_array_eq<decltype(m2), decltype(m3)>::value, "");
        static_assert(map_array_eq<decltype(m3), decltype(m1)>::value, "");
        
        LIGHT_TEST(decltype(m1)::idx_map::sorted_seq::to_array() == keys);
        LIGHT_TEST(decltype(m2)::idx_map::sorted_seq::to_array() == keys);
        LIGHT_TEST(decltype(m3)::idx_map::sorted_seq::to_array() == keys);
        
        LIGHT_TEST(std::memcmp(&m1, &m2, sizeof(m1)) == 0);
        
        for(int i = 0; i < 50; ++i) {
            auto it = m1.find(i);
            if(it) *it = i*10;
            it = m2.find(i);
            if(it) *it = i*10;
        }
        
        LIGHT_TEST(std::memcmp(&m1, &m2, sizeof(m1)) == 0);
        
        for(auto key : keys) {
            auto it = m2.find(key);
            LIGHT_TEST(!!it);
            if(key)
                LIGHT_TEST(*it == 10*key);
        }
    }
    
    {
        using namespace preFIX::types::details::example;
        
        for(long i : {-999,0,1,2,34,999,1000,1001,333333,999999999}) {
            clrbuf();
            itoa(buf, i);
            stdprintf("%% -> {%%}, %% digits",
                i, std::string(buf), digits(i));
        }
    }
}