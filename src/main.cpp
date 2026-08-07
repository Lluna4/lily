#include <chrono>
#include <memory>
#include <print>
#include <thread>
#include <meta>
#include "../netlib_/src/networking.h"
#include "mc_types.h"
#include "deserialize.h"
#include "user.h"
#include "packet.h"
#include "registry.h"
#include "chunk_send.h"
#include "block_registry_processing.h"

std::map<int, user> users;
std::vector<int> disconnected;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_handshake;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_status;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_login;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_config;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_play;
world w;

int read_size(packet &pkt, int fd)
{
    std::unique_ptr<char []> buf = std::make_unique<char []>(5);
    int r = recv(fd, buf.get(), 5, MSG_PEEK);
    if (r == 0 || r == -1)
      return -1;
    minecraft::varint size = minecraft::read_varint(buf.get());
    pkt.size = size.num;
    return size.size;
}

void execute_packet(packet &pkt, server &sv)
{
    int fd = pkt.fd;
    if (!users.contains(fd))
    {
        users.emplace(std::piecewise_construct, std::forward_as_tuple(fd), std::forward_as_tuple(fd));
    }

    user &u = users.find(fd)->second;

    if (u.state == STATE::HANDSHAKE)
    {
        auto p = packet_definitions_handshake.find(pkt.id);
        if (p == packet_definitions_handshake.end())
            return;

        auto pt = p->second.get();

        pt->parse(pkt);
        pt->handle(sv, users, disconnected, fd);
    }
    else if (u.state == STATE::STATUS)
    {
        auto p = packet_definitions_status.find(pkt.id);
        if (p == packet_definitions_status.end())
            return;

        auto pt = p->second.get();

        pt->parse(pkt);
        pt->handle(sv, users, disconnected, fd);
    }
    else if (u.state == STATE::LOGIN)
    {
        auto p = packet_definitions_login.find(pkt.id);
        if (p == packet_definitions_login.end())
            return;

        auto pt = p->second.get();

        pt->parse(pkt);
        pt->handle(sv, users, disconnected, fd);
    }
    else if (u.state == STATE::CONFIGURATION)
    {
        auto p = packet_definitions_config.find(pkt.id);
        if (p == packet_definitions_config.end())
            return;

        auto pt = p->second.get();

        pt->parse(pkt);
        pt->handle(sv, users, disconnected, fd);
    }
    else if (u.state == STATE::PLAY)
    {
        auto p = packet_definitions_play.find(pkt.id);
        if (p == packet_definitions_play.end())
            return;

        auto pt = p->second.get();

        pt->parse(pkt);
        pt->handle(sv, users, disconnected, fd);
    }

}

void update_keep_alive(server &sv)
{
    for (auto &u: users)
    {
        u.second.ticks_to_keepalive--;
        if (u.second.ticks_to_keepalive == 0)
        {
            keep_alive k = {4};
            send_packet(u.first, 0x2B, k, sv);
            u.second.sent = true;
            log("Sent keep alive", LOG_LEVEL::NORMAL);
        }
        if (u.second.ticks_to_keepalive == 3000)
        {
            sv.disconnect_client(u.first);
            users.erase(u.first);
            log("User timed out", LOG_LEVEL::WARNING);
        }
    }
}


int main()
{
    server s(server_mode::SIZE_READ, read_size);
    s.open_server("0.0.0.0", 25565);
    set_packets(packet_definitions_handshake, packet_definitions_status, packet_definitions_login, packet_definitions_config, packet_definitions_play);
    get_registry(biomes, dimensions);
    std::thread world_th(world_thread, std::ref(s), std::ref(w));
	world_th.detach();
	process_item_registry("../generated/reports/registries.json", items);
	w.set_blocks(std::move(process_block_registry("../generated/reports/blocks.json")));
	log("Added block registry", LOG_LEVEL::NORMAL);
	w.generate_seeds();
	get_registry(w.biomes, dimensions);
	w.get_chunk(0, 0, &spawn_y);
    while (true)
    {
        auto pkts = s.get_packets();
        for (auto &pkt: pkts)
        {
            if (pkt.id == -1)
            {
                log("Disconnected", LOG_LEVEL::WARNING);
                users.erase(pkt.fd);
                continue;
            }
            pkt_header head;
            deserialize(head, pkt.data.get());
            pkt.size = head.size.num;
            pkt.id = head.id.num;
            pkt.header_offset = head.size.size + head.id.size;
            execute_packet(pkt, s);

            //std::println("Header is {} {}", head.size.num, head.id.num);
        }
        update_keep_alive(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
}
