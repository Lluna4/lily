#pragma once
#include <sys/socket.h>
#include <concepts>
#include <cstring>
#include <tuple>
#include <bitset>
#include <unistd.h>
#include <print>
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
void write_type(buffer<char> *v, T value)
{
    switch (sizeof(T))
    {
        case 2:
        {
            uint16_t conv = htobe16((*(uint16_t *)&value));
            v->write(&conv, sizeof(T));
            break;
        }
        case 4:
        {
            uint32_t conv = htobe32((*(uint32_t *)&value));
            v->write(&conv, sizeof(T));
            break;
        }
        case 8:
        {
            uint64_t conv = htobe64((*(uint64_t *)&value));
            v->write(&conv, sizeof(T));
            break;
        }
        default:
            v->write(&value, sizeof(T));
    }
}

template <>
inline void write_type<minecraft::varint>(buffer<char> *v, minecraft::varint value)
{
    v->allocate(v->size + 5);
    v->size += minecraft::write_varint(&v->data[v->size], value.num);
}

template <>
inline void write_type<std::string>(buffer<char> *v, std::string value)
{
    v->allocate(v->size + 5);
    v->size += minecraft::write_varint(&v->data[v->size], value.length());
    v->write(value.c_str(), value.length());
}

template <>
inline void write_type<minecraft::uuid>(buffer<char> *v, minecraft::uuid value)
{
    v->write(value.data.c_str(), value.data.length());
}

template <>
inline void write_type<minecraft::string_tag>(buffer<char> *v, minecraft::string_tag value)
{
    char start_comp = 0x0a;
    char string_tag = 0x08;
    std::string dummy_name = "text";
    short dummy_name_size = 4;
    dummy_name_size = htobe16(*(uint16_t*)&dummy_name_size);
    short value_size = value.str.length();
    value_size = htobe16(*(uint16_t*)&value_size);

    v->write(&start_comp, 1);
    v->write(&string_tag, 1);
    v->write(&dummy_name_size, sizeof(short));
    v->write(dummy_name.data(), 4);
    v->write(&value_size, sizeof(short));
    v->write(value.str.data(), value.str.length());
    v->size++;
}

template <>
inline void write_type<chunk>(buffer<char> *v, chunk value)
{
    buffer<char> t;
    for (auto &sec: value.sections)
    {
        t.write(&sec.non_air_blocks, sizeof(short));
        if (sec.non_air_blocks == 4096 && sec.palette.size() == 2)
        {
            t.data[t.size] = 0;
            t.size++;
            t.allocate(t.size + 5);
            t.size += minecraft::write_varint(&t.data[t.size], sec.palette[0]);
        }
        else
        {
            t.data[t.size] = 8;
            t.size++;
            t.allocate(t.size + 5);
            t.size += minecraft::write_varint(&t.data[t.size], sec.palette.size());
            for (auto &p: sec.palette)
            {
                t.allocate(t.size + 5);
                t.size += minecraft::write_varint(&t.data[t.size], p);
            }
            for (int i = 0; i < 512; i++)
            {
                int64_t tmp = 0;
                memcpy(&tmp, &sec.blocks[i * 8], sizeof(int64_t));
                t.write(&tmp, sizeof(int64_t));
            }
        }
        t.data[t.size] = 0;
        t.size++;
        t.allocate(t.size + 5);
        t.size += minecraft::write_varint(&t.data[t.size], 1);
    }
    v->allocate(v->size + 5);
    v->size += minecraft::write_varint(&v->data[v->size], t.size);
    v->write(t.data, t.size);
}

template<typename T>
concept arithmetic = std::integral<T> or std::floating_point<T>;

template<typename T>
concept IsChar = std::same_as<T, char *>;

template<typename T>
concept IsPointer = std::is_pointer_v<T>;

template<typename T>
struct write_var
{
    static void call(buffer<char> *v, T value)
    {
        write_type<T>(v, value);
    }
};

namespace netlib
{
    template<typename ...T>
    int send_packet(std::tuple<T...> packet, int sock, unsigned long id)
    {
        buffer<char> buf;
        constexpr std::size_t size = std::tuple_size_v<decltype(packet)>;
        write_comp_pkt(size, buf, packet);

        buffer<char> dummy;
        dummy.allocate(5);
        size_t id_size = minecraft::write_varint(dummy.data, id);
        buffer<char> header;
        header.allocate(10); //the max size for 2 varints
        header.size += minecraft::write_varint(header.data, buf.size + id_size);
        header.size += minecraft::write_varint(&header.data[header.size], id);
        header.write(buf.data, buf.size);

    	int sent = 0;
        while (sent < header.size)
        {
            int ret = send(sock, &header.data[sent], header.size - sent, 0);
            if (ret == -1 || ret == 0)
                return ret;
            sent += ret;
        }
        std::println("Sent {}B", sent);
        return sent;
    }

    template<typename ...T>
    int send_packet(int sock, unsigned long id)
    {
        buffer<char> dummy;
        dummy.allocate(5);
        size_t id_size = minecraft::write_varint(dummy.data, id);
        buffer<char> header;
        header.allocate(10); //the max size for 2 varints
        header.size += minecraft::write_varint(header.data, id_size);
        header.size += minecraft::write_varint(&header.data[header.size], id);

        int sent = 0;
        while (sent < header.size)
        {
            int ret = send(sock, &header.data[sent], header.size - sent, 0);
            if (ret == -1 || ret == 0)
                return ret;
            sent += ret;
        }
        std::println("Sent {}B", sent);
        return sent;
    }

    template<typename ...T>
    int send_packet_headless(std::tuple<T...> packet, int sock)
    {
        buffer<char> buf;
        constexpr std::size_t size = std::tuple_size_v<decltype(packet)>;
        write_comp_pkt(size, buf, packet);

        int sent = 0;
        while (sent < buf.size)
        {
            int ret = send(sock, &buf.data[sent], buf.size - sent, 0);
            if (ret == -1 || ret == 0)
                return ret;
            sent += ret;
        }
        std::println("Sent {}B", sent);
        return sent;
    }
}