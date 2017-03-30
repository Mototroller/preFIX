#pragma once

#include <preFIX.hpp>

namespace preFIX { namespace types { namespace details {
    
    /// Obvious, simple and stable generic S/D
    namespace defaults {
        
        /// {ostringstream(dst) << value}
        template <typename T>
        struct sstream_serializer {
            template <typename U>
            static bool serialize(write_cursor& dst, U&& value, char delimiter = SOH) {
                std::ostringstream ss;
                ss << std::forward<U>(value) << delimiter;
                std::string str = ss.str();
                
                int need = str.size();
                if(dst.left() >= need) {
                    std::memcpy(dst.pointer(), str.data(), need);
                    dst.step(need);
                    return true;
                }
                
                return false;
            }
        };

        /// {istringstream(src) >> value}
        template <typename T>
        struct sstream_deserializer {
            template <typename U>
            static bool deserialize(read_cursor& src, U& value, char delimiter = SOH) {
                auto ptr = src.pointer();
                auto end = ptr + src.left();
                auto fnd = std::find(ptr, end, delimiter);
                
                if(fnd != end) {
                    std::string str(ptr, fnd - ptr);
                    std::istringstream ss(str); ss >> value;
                    src.step(fnd - ptr + 1);
                    return true;
                }
                
                return false;
            }
        };
        
        /// Special serializer for fixed-width integer: 9=000123<SOH>
        template <size_t Width>
        struct fixed_width_int_serializer {
            template <typename U>
            static bool serialize(write_cursor& dst, U&& value, char delimiter = SOH) {
                bool neg = (value < 0);
                std::ostringstream ss;
                ss << (neg ? "-" : "")
                    << std::setfill('0')
                    << std::setw(Width - neg)
                    << std::abs(std::forward<U>(value))
                    << delimiter;
                std::string str = ss.str();
                
                int need = str.size();
                if(dst.left() >= need) {
                    std::memcpy(dst.pointer(), str.data(), str.size());
                    dst.step(need);
                    return true;
                }
                
                return false;
            }
        };
        
    } // defaults

    
    /// ------------------------! Tiltower Customs Inc. !------------------------ ///
    
    /// Example of customized S/D
    namespace example {
        
        size_t digits(size_t u) {
            constexpr size_t length = 19;
            constexpr size_t pows10[length] = {
                1LLU,10LLU,100LLU,1000LLU,10000LLU,100000LLU,1000000LLU,10000000LLU,100000000LLU,1000000000LLU,
                10000000000LLU,100000000000LLU,1000000000000LLU,10000000000000LLU,100000000000000LLU,
                1000000000000000LLU,10000000000000000LLU,100000000000000000LLU,1000000000000000000LLU
            };
            if(u != 0) {
                size_t sum = 0;
                for(size_t i = 0; i < length; ++i)
                    sum += (pows10[i] <= u);
                return sum;
            }
            return 1;
        }
        
        size_t itoa(char* const dst, int64_t sv) {
            constexpr char digit_pairs[201] = {
                "00010203040506070809101112131415161718192021222324"
                "25262728293031323334353637383940414243444546474849"
                "50515253545556575859606162636465666768697071727374"
                "75767778798081828384858687888990919293949596979899"
            };
            
            uint64_t v = static_cast<uint64_t>(sv);
            if(sv < 0) {
                *dst = '-';
                v = ~v + 1;
            }
            
            auto const size = digits(v) + (sv < 0);
            char* c = &dst[size - 1];
            while (v >= 100) {
                auto const r = v % 100;
                v /= 100;
                std::memcpy(c - 1, digit_pairs + 2*r, 2);
                c -= 2;
            }
            if (v < 10) *c = '0' + v;
            else std::memcpy(c-1, digit_pairs+2*v, 2);
            return size;
        }
        
        template <typename T>
        struct custom_serializer {
            
            static bool serialize(write_cursor& dst, Char_underlying value, char delimiter = SOH) {
                auto ptr = dst.pointer();
                ptr[0] = value;
                ptr[1] = delimiter;
                dst.step(2);
                return true;
            }
            
            static bool serialize(write_cursor& dst, Int_underlying value, char delimiter = SOH) {
                auto ptr = dst.pointer();
                //auto written = std::sprintf(ptr, "%ld", long(value));
                auto written = itoa(ptr, long(value));
                ptr[written] = delimiter;
                dst.step(written + 1);
                return true;
            }
            
            static bool serialize(write_cursor& dst, Float_underlying value, char delimiter = SOH) {
                auto ptr = dst.pointer();
                auto written = std::sprintf(ptr, "%lf", double(value));
                ptr[written] = delimiter;
                dst.step(written + 1);
                return true;
            }
            
            static bool serialize(write_cursor& dst, String_underlying const& value, char delimiter = SOH) {
                auto ptr = dst.pointer();
                std::memcpy(ptr, value.data(), value.size());
                ptr[value.size()] = delimiter;
                dst.step(value.size() + 1);
                return true;
            }
        };
        
    } // example
    
    
    /// ------------------------! Final typedefs here !------------------------ ///
    
    template <typename T>
    //using serializer = defaults::sstream_serializer<T>;
    using serializer = example::custom_serializer<T>;
    
    template <typename T>
    using deserializer = defaults::sstream_deserializer<T>;
    
    template <size_t Width>
    using fixed_width_int_serializer = defaults::fixed_width_int_serializer<Width>;

} // details
} // types
} // preFIX
