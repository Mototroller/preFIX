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
    
    {
        Int i;
        
        i.value = 13;
        Int::serialize(buf, i.value);
        str = buf;
        str = str.substr(0, str.size() - 1);
        stdcout(str);
        
        int x;
        Int::deserialize(buf, x);
        stdcout(x);
    }
    
    clrbuf();
    
    {
        String s;
        
        s.value = "DHKDHK//HKHKHHJ";
        String::serialize(buf, s.value);
        str = buf;
        str = str.substr(0, str.size() - 1);
        stdcout(str);
        
        std::string x;
        String::deserialize(buf, x);
        stdcout(x);
        
        String::deserialize("abcde9823|", x, '|');
        stdcout(x);
    }
    
    clrbuf();
    
    {
        using namespace test_dict;
        
        Header header;
        
        //header.set<MsgType>(String("A"));
        header.set<MsgType>("A");
        
        String s;
        header.get<MsgType>(s);
        stdcout(s.value);
        
        // Chaining
        header.set<BeginString> ("FIX.4.4")
              .set<SenderCompID>("MYCOMP")
              .set<TargetCompID>("THEIRTCOMP")
              .set<MsgSeqNum>   (1);
        
        // Omitting
        header.set<PossDupFlag>('Y');
        header.clear<PossDupFlag>();
        
        header.serialize(buf);
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
        
        nos.serialize(buf);
        stdcout(replace_SOH(buf));
        
        clrbuf();
        
        Trailer trailer;
        serialize_message(buf, header, nos, trailer);
        stdcout(replace_SOH(buf));
        
        auto map = build_offsets_map(buf, sizeof(buf));
        for(auto const& tag : {0}) {
            auto range = map.equal_range(tag);
            for(auto it = range.first; it != range.second; ++it)
                stdprintf("%% at %%", it->first, it->second);
        }
        
        auto account = extract_field<Account>(buf, map);
        stdcout(account.value);
        
        nos.set<Account>("changed");
        transfer_field<Account>(buf, nos, map);
        stdcout(nos.at<Account>().value);
        
        account.clear();
        account.deserialize(buf + map.find(account.tag)->second);
        stdcout(account.value);
    }
    
    clrbuf();
    
    {
        stdcout(null<Int>::value());
        stdcout(null<Float>::value());
        stdcout(int(null<Char>::value()));
        stdcout(null<String>::value());
        stdcout(null<Fixed<6>>::value());
        
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
            
            for(size_t j = 0; j < parties.size(); ++j) {
                order.at<NoPartyID>()[j]
                    .set<PartyID>(parties[j])
                    .set<PartyRole>(0);
            }
        }
        
        batch.serialize(buf);
        stdcout(replace_SOH(buf));
    }
}