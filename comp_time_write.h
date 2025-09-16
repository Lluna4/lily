#pragma once
#include <sys/socket.h>
#include <concepts>
#include <cstring>
#include <tuple>
#include <bitset>
#include <unistd.h>
#include <print>
#include "buffer.h"
#include "mc_types.h"
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

#define write_comp_pkt(size, ptr, t) const_for<size>([&](auto i){write_var<std::tuple_element_t<i.value, decltype(t)>>::call(&ptr, std::get<i.value>(t));});

template <typename Integer, Integer ...I, typename F> constexpr void const_for_each(std::integer_sequence<Integer, I...>, F&& func)
{
    (func(std::integral_constant<Integer, I>{}), ...);
}

template <auto N, typename F> constexpr void const_for(F&& func)
{
    if constexpr (N > 0)
        const_for_each(std::make_integer_sequence<decltype(N), N>{}, std::forward<F>(func));
} 

template <typename T>
void write_type(char *v, T value)
{
    switch (sizeof(T))
    {
        case 2:
        {
            uint16_t conv = htobe16((*(uint16_t *)&value));
            std::memcpy(v, &conv, sizeof(T));
            break;
        }
        case 4:
        {
            uint32_t conv = htobe32((*(uint32_t *)&value));
            std::memcpy(v, &conv, sizeof(T));
            break;
        }
        case 8:
        {
            uint64_t conv = htobe64((*(uint64_t *)&value));
            std::memcpy(v, &conv, sizeof(T));
            break;
        }
        default:
            std::memcpy(v, &value, sizeof(T));
    }
}

template <int size, typename T>
void write_array(char *v, T value);

template<typename T>
concept arithmetic = std::integral<T> or std::floating_point<T>;

template<typename T>
concept IsChar = std::same_as<T, char *>;

template<typename T>
concept IsPointer = std::is_pointer_v<T>;

template<typename T>
struct write_var
{
    static void call(buffer<char> *v, T value) requires (arithmetic<T>)
    {
        if (v->parse_consumed_size >= v->size || v->parse_consumed_size + sizeof(T) >= v->size)
        {
            v->allocate(v->size + 1024);
            v->extra = v->data + v->parse_consumed_size;
        }
        write_type<T>(v->data, value);
        v->extra += sizeof(T);
        v->parse_consumed_size += sizeof(T);
    }
};

template<typename ...T>
struct write_var<std::tuple<T...>>
{
    static void call(buffer<char> *v, std::tuple<T...> value)
    {
        constexpr std::size_t size = std::tuple_size_v<decltype(value)>;
        write_comp_pkt(size, *v, value);
    }
};

template<>
struct write_var<std::string>
{
    static void call(buffer<char> *v, std::string value)
    {
        while (v->parse_consumed_size >= v->size || v->parse_consumed_size + value.size() >= v->size)
        {
            v->allocate(v->size + 1024);
            v->extra = v->data + v->parse_consumed_size;
        }
        memcpy(v->data, value.c_str(), value.size());
        v->extra += value.size();
        v->parse_consumed_size += value.size();
    }
};


template<typename T>
struct write_var<std::vector<T, std::allocator<T>>>
{
    static void call(buffer<char> *v, std::vector<T, std::allocator<T>> value)
    {
        for (auto val : value)
        {
            std::tuple<T> va = val;
            constexpr std::size_t size = std::tuple_size_v<decltype(va)>;
            write_comp_pkt(size, *v, va);
        }
    }
};


namespace netlib
{
    template<typename ...T>
    int send_packet(std::tuple<T...> packet, int sock)
    {
        buffer<char> buf;
        buf.allocate(1024);
        buf.extra = buf.data;
        constexpr std::size_t size = std::tuple_size_v<decltype(packet)>;
        write_comp_pkt(size, buf, packet);

        buffer<char> header;
        header.allocate(5); //the max size of a varint
        header.size += minecraft::write_varint(header.data, buf.parse_consumed_size);
        header.write(buf.data, buf.parse_consumed_size);

        int ret = send(sock, buf.data, buf.parse_consumed_size, 0);
        std::println("Sent {}B", ret);
        return ret;
    }
}