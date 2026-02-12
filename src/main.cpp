#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <print>
#include <chrono>
#include <map>
#include <cmath>
#include <tuple>
#include "json_reader.h"
#include "mc_types.h"
#include "user.h"
#include "networking/mc_netlib.h"
#include "registry.h"
#include "block_registry_processing.h"
#include "chat.h"
#include "log.h"
#include "chunk.h"
#include "blocks.h"
#include "packet.h"
#include "chunk_send.h"
#include <csignal>

std::map<int, user> users;
std::vector<int> disconnected;
std::vector<block> modified_blocks;

std::map<int, std::unique_ptr<packet_base>> packet_definitions_handshake;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_status;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_login;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_config;
std::map<int, std::unique_ptr<packet_base>> packet_definitions_play;
server sv{};

long log_id = 0;
long leaves_id = 0;
long grass_id;
long dirt_id;

void execute_packet(int fd, netlib::packet &packet)
{
	if (!users.contains(fd))
	{
		users.emplace(std::piecewise_construct, std::forward_as_tuple(fd), std::forward_as_tuple(fd));
	}

	user &u = users.find(fd)->second;

	if (u.state == STATE::HANDSHAKE)
	{
		auto pkt = packet_definitions_handshake.find(packet.id);
		if (pkt == packet_definitions_handshake.end())
			return;

		auto p = pkt->second.get();

		p->parse(packet);
		p->handle(sv, users, disconnected, fd);
	}
	else if (u.state == STATE::STATUS)
	{
		auto pkt = packet_definitions_status.find(packet.id);
		if (pkt == packet_definitions_status.end())
			return;

		auto p = pkt->second.get();

		p->parse(packet);
		p->handle(sv, users, disconnected, fd);
	}
	else if (u.state == STATE::LOGIN)
	{
		auto pkt = packet_definitions_login.find(packet.id);
		if (pkt == packet_definitions_login.end())
			return;

		auto p = pkt->second.get(); 

		p->parse(packet);
		p->handle(sv, users, disconnected, fd);
	}
	else if (u.state == STATE::CONFIGURATION)
	{
		auto pkt = packet_definitions_config.find(packet.id);
		if (pkt == packet_definitions_config.end())
			return;

		auto p = pkt->second.get();

		p->parse(packet);
		p->handle(sv, users, disconnected, fd);
	}
	else if (u.state == STATE::PLAY)
	{
		auto pkt = packet_definitions_play.find(packet.id);
		if (pkt == packet_definitions_play.end())
			return;

		auto p = pkt->second.get();

		p->parse(packet);
		p->handle(sv, users, disconnected, fd);
	}
}

void update_keep_alive(server &sv)
{
	for (auto &u: users)
	{
		u.second.ticks_to_keepalive--;
		if (u.second.ticks_to_keepalive == 0)
		{
			auto keep_alive = std::make_tuple((long)4);
			sv.send_packet(keep_alive, u.first, 0x26);
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


void signal_handler(int sig)
{
	log("Got SIGPIPE", LOG_LEVEL::ERROR);
}

int main()
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;

	std::signal(SIGPIPE, signal_handler);

	if(!create_log_file())
		log("Creating log file failed", LOG_LEVEL::WARNING);
	auto ret = sv.open_server("0.0.0.0", 25565);
	if (!ret)
	{
		log(std::format("Opening server failed: {}", ret.error()), LOG_LEVEL::ERROR);
		return -1;
	}
	std::thread world_th(world_thread, std::ref(sv), std::ref(w));
	world_th.detach();
	process_item_registry("../generated/reports/registries.json", items);
	w.set_blocks(std::move(process_block_registry("../generated/reports/blocks.json")));
	log("Added block registry", LOG_LEVEL::NORMAL);
	w.generate_seeds();
	get_registry(w.biomes, dimensions);
	w.get_chunk(0, 0, &spawn_y);
	set_packets(packet_definitions_handshake, packet_definitions_status, packet_definitions_login, packet_definitions_config, packet_definitions_play);
	while (true)
	{
		const auto before = clock::now();
		auto packets = sv.get_packets();
		for (auto &pkt: packets)
		{
			if (pkt.id == -1)
			{
				user &u = users.find(pkt.fd)->second;
				if (u.state == STATE::PLAY)
				{
					auto remove_entity = std::make_tuple(minecraft::varint(1), minecraft::varint(pkt.fd));
					send_all_except_user(remove_entity, u, 0x46, sv, users);
					auto remove_info = std::make_tuple(minecraft::varint(1), u.uuid);
					send_all_except_user(remove_info, u, 0x3E, sv, users);
					send_system_chat(std::format("{} disconnected", u.name), users, sv);
				}
				log("Client disconnected", LOG_LEVEL::NORMAL);
				users.erase(pkt.fd);
				continue;
			}
			//log(std::format("Got a packet from fd {} with id {} and size {}", pkt.fd, pkt.id, pkt.size), LOG_LEVEL::NORMAL);
			execute_packet(pkt.fd, pkt);
			for (auto &disconnect: disconnected)
				users.erase(disconnect);
		}
		update_keep_alive(sv);
		const ms duration = clock::now() - before;
		//log(std::format("MSPT {}ms", duration.count()), LOG_LEVEL::NORMAL);
		if (duration.count() <= 50)
			std::this_thread::sleep_for(std::chrono::milliseconds(50) - duration);
	}
	return 0;
}
