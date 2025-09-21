#include <print>
#include <chrono>
#include <map>
#include "user.h"
#include "networking/mc_netlib.h"
#include "packet_arguments.h"
#include "registry.h"
#include <filesystem>
#include <fcntl.h>

std::map<int, user> users;

/*
void registry_data(user &u)
{
	for (const auto &file: std::filesystem::recursive_directory_iterator("../Minecraft-DataRegistry-Packet-Generator/registries/1.21-registry/created-packets"))
	{
		int fd = open(file.path().c_str(), O_RDONLY);
		off_t file_size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		sendfile(u.fd, fd, 0, file_size);
		close(fd);
	}
}*/
template <typename ...T>
void send_all_except_user(std::tuple<T...> packet, user &u, int id)
{
	for (auto &us: users)
	{
		if (us.first != u.fd)
			netlib::send_packet(packet, u.fd, id);
	}
}

void execute_packet(int fd, netlib::packet &packet, server &sv)
{
	if (!users.contains(fd))
	{
		users.emplace(std::piecewise_construct, std::forward_as_tuple(fd), std::forward_as_tuple(fd));
	}

	user &u = users.find(fd)->second;

	if (u.state == STATE::HANDSHAKE)
	{
		switch (packet.id)
		{
			case 0:
				std::tuple<minecraft::varint, minecraft::string, unsigned short, minecraft::varint> handshake;
				handshake = netlib::read_packet(std::move(handshake), packet);
				std::println("Received handshake packet with version {} address {} port {} intent {}",
					std::get<VERSION>(handshake).num, std::get<ADDRESS>(handshake).data.data, std::get<PORT>(handshake), std::get<INTENT>(handshake).num);
				if (std::get<0>(handshake).num != 772)
				{
					sv.disconnect_client(fd);
					users.erase(fd);
					break;
				}
				if (std::get<3>(handshake).num == 1)
					u.state = STATE::STATUS;
				else if (std::get<3>(handshake).num == 2)
					u.state = STATE::LOGIN;
				break;
		}
	}
	else if (u.state == STATE::LOGIN)
	{
		switch (packet.id)
		{
			case 0x00:
			{
				minecraft::string res;
				minecraft::read_string(packet.data.data + packet.header_size, res);
				u.name = res.data.data;
				u.uuid.generate(u.name);
				auto login_success = std::make_tuple(u.uuid, u.name, minecraft::varint(0));
				netlib::send_packet(login_success, u.fd, 0x02);
				break;
			}
			case 0x03:
			{
				u.state = STATE::CONFIGURATION;
			}
		}
	}
	else if (u.state == STATE::CONFIGURATION)
	{
		switch (packet.id)
		{
			case 0x00:
			{
				std::tuple<minecraft::string, char, minecraft::varint, bool, unsigned char, minecraft::varint, bool, bool, minecraft::varint> client_info;
				client_info = netlib::read_packet(std::move(client_info), packet);

				std::println("Locale {} View distance {}", std::get<LOCALE>(client_info).data.data, (int)std::get<VIEW_DISTANCE>(client_info));
				u.locale = std::get<LOCALE>(client_info).data.data;
				u.view_distance = std::get<VIEW_DISTANCE>(client_info);

				auto known_packs = std::make_tuple(minecraft::varint(1), std::string("minecraft"), std::string("core"), std::string("1.21.8"));
				netlib::send_packet(known_packs, fd, 0x0E);
				send_registry(fd);
				netlib::send_packet(fd, 0x03);
				break;
			}

			case 0x02:
			{
				std::tuple<minecraft::string> plugin_message;
				plugin_message = netlib::read_packet(std::move(plugin_message), packet);
				std::println("Plugin message sent from channel {}", std::get<0>(plugin_message).data.data);
				break;
			}
			case 0x03:
			{
				u.state = STATE::PLAY;
				auto login = std::make_tuple(fd, false,minecraft::varint(1) ,(std::string)"minecraft:overworld",
											minecraft::varint(20), minecraft::varint(u.view_distance),
											minecraft::varint(12), false, false, false, minecraft::varint(0),
											(std::string)"minecraft:overworld", (long)128612, (unsigned char)0,
											(char)-1, false, false, false, minecraft::varint(0), minecraft::varint(64),
											false);
				netlib::send_packet(login, fd, 0x2B);
				auto sync_pos = std::make_tuple(minecraft::varint(1), u.x, u.y, u.z, (double)0.0f, (double)0.0f,
												(double)0.0f, u.yaw, u.pitch, (int)0);
				netlib::send_packet(sync_pos, fd, 0x41);
				auto add_to_list = std::make_tuple((char)0x01, minecraft::varint(1), u.uuid, u.name, minecraft::varint(0));
				send_all_except_user(add_to_list, u, 0x3F);
				for (auto &us: users)
				{
					if (us.first != u.fd)
					{
						auto add_to_list_user = std::make_tuple((char)0x01, minecraft::varint(1), us.second.uuid, us.second.name, minecraft::varint(0));
						netlib::send_packet(add_to_list_user, u.fd, 0x3F);
					}
				}

				auto game_event = std::make_tuple((unsigned char)13, 0.0f);
				netlib::send_packet(game_event, fd, 0x22);
				auto set_center_chunk = std::make_tuple(minecraft::varint(0), minecraft::varint(0));
				netlib::send_packet(set_center_chunk, fd, 0x57);
				chunk c(0, 0);
				for (int y = -u.view_distance; y < u.view_distance; y++)
				{
					for (int x = -u.view_distance; x < u.view_distance; x++)
					{
						auto chunk_data = std::make_tuple(x, y, minecraft::varint(0),c, minecraft::varint(0),
									minecraft::varint(0),minecraft::varint(0),minecraft::varint(0),
									minecraft::varint(0),minecraft::varint(0), minecraft::varint(0));
						netlib::send_packet(chunk_data, fd, 0x27);
					}
				}
			}
		}
	}
	else if (u.state == STATE::PLAY)
	{
		switch (packet.id)
		{
			case 0x00:
			{
				std::println("teleport confirm");
				std::tuple<minecraft::varint> confirm_teleport;
				confirm_teleport = netlib::read_packet(confirm_teleport, packet);

			}
		}
	}
}

int main()
{
	server sv{};
	auto ret = sv.open_server("0.0.0.0", 25565);
	if (!ret)
	{
		std::println("Opening server failed: {}", ret.error());
		return -1;
	}

	while (true)
	{
		std::unique_lock lock(sv.mut);
		for (auto &pkt: sv.packets)
		{
			if (pkt.id == -1)
			{
				std::println("Client disconnected");
				users.erase(pkt.fd);
				continue;
			}
			std::println("Got a packet from fd {} with id {} and size {}", pkt.fd, pkt.id, pkt.size);
			execute_packet(pkt.fd, pkt, sv);
		}
		sv.packets.clear();
		lock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	return 0;
}
