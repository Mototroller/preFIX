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
        stdcout(details::idx_of<Header, BeginString>::value);
        stdcout(details::idx_of<Header, Length>::value);
        stdcout(details::idx_of<Header, MsgType>::value);
        
        header.set<MsgType>(String("A"));
        header.set<MsgType>("A");
        
        String s;
        header.get<MsgType>(s);
        stdcout(s.value);
        
        //header.set<Account>("FIX.4.4");
        header.set<BeginString>("FIX.4.4");
        header.set<SenderCompID>("MYCOMP");
        header.set<TargetCompID>("THEIRTCOMP");
        //header.omit<Length>();
        //header.set<Length>(0);
        
        header.serialize(buf);
        stdcout(replace_SOH(buf));
        
        clrbuf();
        
        NewOrderSingle nos;
        nos.set<ClOrdID>("123ABC");
        nos.set<Account>("ololo//OLOLO");
        nos.set<Price>(66.6625);
        nos.set<Side>('2');
        
        //NoPartyID parties;
        auto& parties = nos.set<NoPartyID>();
        parties[0].set<PartyID>("USER");
        parties[1].set<PartyID>("FIRM");
        
        nos.serialize(buf);
        stdcout(replace_SOH(buf));
        
        clrbuf();
        
        auto written = serialize_body(buf, header, nos);
        written += serialize_chksum(buf, written);
        stdcout(replace_SOH(buf));
        stdcout(written);
    }
    
    clrbuf();
    
    {
        
    }
}