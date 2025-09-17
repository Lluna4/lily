#pragma once
#include <algorithm>
#include <concepts>
#include <cstring>
#include <tuple>
#include <stdlib.h>
#include <cstdint>
#include "buffer.h"
#include "../mc_types.h"
#ifdef __FreeBSD__
#include <sys/endian.h>
#endif
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#endif
#define read_comp_pkt(size, ptr, t) const_for_<size>([&](auto i){std::get<i.value>(t) = read_var<std::tuple_element_t<i.value, decltype(t)>>::call(&ptr);});

template <typename T>
T read_type(char *v)
{
    T a;

    std::memcpy(&a, v, sizeof(T));
    switch (sizeof(T))
    {
        case 1:
            break;
        case 2:
            a = be16toh(a);
            break;
        case 4:
            a = be32toh(a);
            break;
        case 8:
            a = be64toh(a);
            break;
    }
    return a;
}
template<>
inline float read_type<float>(char *v)
{
    std::uint32_t num_as_uint32;
    float num;

    memcpy(&num_as_uint32, v, sizeof(std::uint32_t));
    num_as_uint32 = be32toh(num_as_uint32);
    memcpy(&num, &num_as_uint32, sizeof(float));

    return num;
}

template<>
inline double read_type<double>(char *v)
{
    std::uint64_t num_as_uint64;
    double num;

    memcpy(&num_as_uint64, v, sizeof(std::uint64_t));
    num_as_uint64 = be64toh(num_as_uint64);
    memcpy(&num, &num_as_uint64, sizeof(double));

    return num;
}

template<>
inline minecraft::varint read_type<minecraft::varint>(char *v)
{
    return minecraft::read_varint(v);
}

template<>
inline minecraft::string read_type<minecraft::string>(char *v)
{
    minecraft::string ret;
    minecraft::read_string(v, ret);
    return ret;
}

template<typename T>
struct read_var
{
    static T call(buffer<char>* v)
    {
        if constexpr (typeid(T) == typeid(minecraft::varint) || typeid(T) == typeid(minecraft::string))
        {
            if (5 + v->parse_consumed_size > v->size)
                return T{};
        }
        else
        {
            if (sizeof(T) + v->parse_consumed_size > v->size)
                return T{};
        }
        T ret = read_type<T>(v->extra);
        size_t add_size = 0;
        if constexpr (typeid(T) == typeid(minecraft::varint) || typeid(T) == typeid(minecraft::string))
            add_size = ret.size;
        else
            add_size = sizeof(T);

        v->extra += add_size;
        v->parse_consumed_size += add_size;
        return ret;
    }
};


template<typename ...T>
struct read_var<std::tuple<T...>>
{
    static std::tuple<T...> call(buffer<char> *v)
    {
        std::tuple<T...> ret;
        constexpr std::size_t size = std::tuple_size_v<decltype(ret)>;
        if (size + v->parse_consumed_size > v->size)
            return std::tuple<T...>{};
        char *diff = v->data;
        ret = read_comp_pkt(size, diff, ret);
        int size_diff =(diff - v->data); 
        v->parse_consumed_size += size_diff;
        v->data += size_diff;
        return ret;
    }
};

template <typename Integer, Integer ...I, typename F> constexpr void const_for_each_(std::integer_sequence<Integer, I...>, F&& func)
{
    (func(std::integral_constant<Integer, I>{}), ...);
}

template <auto N, typename F> constexpr void const_for_(F&& func)
{
    if constexpr (N > 0)
        const_for_each_(std::make_integer_sequence<decltype(N), N>{}, std::forward<F>(func));
}

namespace netlib
{
    struct packet
    {
        packet(int i)
            :id(i)
        {
            size = 0;
            header_size = 0;
        }
        packet (unsigned long s, unsigned long i, unsigned long header_s,buffer<char> buf)
            :id(i), size(s),header_size(header_s) ,data(buf)
        {}
        unsigned long id;
        unsigned long size;
        unsigned long header_size;
        buffer<char> data;
    };
    template<typename ...T>
    std::tuple<T...> read_packet(std::tuple<T...> packet, netlib::packet &pkt)
    {
        constexpr std::size_t size = std::tuple_size_v<decltype(packet)>;
        pkt.data.extra = pkt.data.data + pkt.header_size;
        read_comp_pkt(size, pkt.data, packet);
        return packet;
    }
}