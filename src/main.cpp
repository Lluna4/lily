#include <print>
#include <chrono>
#include <map>
#include "user.h"
#include "networking/mc_netlib.h"
#include "packet_arguments.h"
#include "registry.h"
std::map<int, user> users;

template <typename ...T>
void send_all_except_user(std::tuple<T...> packet, user &u, int id)
{
	for (auto &us: users)
	{
		if (us.first != u.fd)
			netlib::send_packet(packet, u.fd, id);
	}
}

template <typename ...T>
void send_all(std::tuple<T...> packet, int id)
{
	for (auto &u: users)
	{
		netlib::send_packet(packet, u.first, id);
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
											(std::string)"minecraft:overworld", (long)128612, (unsigned char)1,
											(char)-1, false, false, false, minecraft::varint(0), minecraft::varint(64),
											false);
				netlib::send_packet(login, fd, 0x2B);
				auto sync_pos = std::make_tuple(minecraft::varint(1), u.x, u.y, u.z, (double)0.0f, (double)0.0f,
												(double)0.0f, u.yaw, u.pitch, (int)0);
				netlib::send_packet(sync_pos, fd, 0x41);
				auto add_to_list = std::make_tuple((char)0x01, minecraft::varint(1), u.uuid, u.name, minecraft::varint(0));
				send_all_except_user(add_to_list, u, 0x3F);
				auto spawn_entity = std::make_tuple(minecraft::varint(fd), u.uuid, minecraft::varint(149),
													u.x, u.y, u.z, (char)(u.pitch/360 * 256), (char)(u.yaw/360 * 256),
													(char)(u.yaw/360 * 256), minecraft::varint(0), (short)0,
													(short)0, (short)0);
				send_all_except_user(spawn_entity, u, 0x01);
				for (auto &us: users)
				{
					if (us.first != u.fd)
					{
						auto add_to_list_user = std::make_tuple((char)0x01, minecraft::varint(1), us.second.uuid, us.second.name, minecraft::varint(0));
						netlib::send_packet(add_to_list_user, u.fd, 0x3F);
						auto spawn_entity_user = std::make_tuple(minecraft::varint(us.first), us.second.uuid, minecraft::varint(149),
									us.second.x, us.second.y, us.second.z, (char)(us.second.pitch/360 * 256),
									(char)(us.second.yaw/360 * 256),(char)(us.second.yaw/360 * 256), minecraft::varint(0), (short)0,
									(short)0, (short)0);
						netlib::send_packet(spawn_entity_user, fd, 0x01);
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
				break;
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
				break;
			}
			case 0x08:
			{
				std::tuple<minecraft::string> chat_message;
				chat_message = netlib::read_packet(std::move(chat_message), packet);

				auto message = std::make_tuple(minecraft::string_tag(std::get<0>(chat_message).data.data),
											minecraft::varint(1), std::string("chat.type.text"), minecraft::varint(2),
											minecraft::varint(0), minecraft::varint(2),
											minecraft::string_tag(std::format("{}<{}>", u.name, u.pronouns)), false);
				send_all(message, 0x1D);
			}
			case 0x1B:
			{
				std::tuple<long> keep_alive_response;
				keep_alive_response = netlib::read_packet(keep_alive_response, packet);
				if (std::get<0>(keep_alive_response) != 4 || u.sent == false)
				{
					sv.disconnect_client(fd);
					users.erase(fd);
					break;
				}
				u.ticks_to_keepalive = 500;
				u.sent = false;
				std::println("Received keep alive");
			}
			case 0x1D:
			{
				std::tuple<double, double, double, char> update_position;
				update_position = netlib::read_packet(update_position, packet);
				u.prev_x = u.x;
				u.prev_y = u.y;
				u.prev_z = u.z;
				u.x = std::get<X>(update_position);
				u.y = std::get<Y>(update_position);
				u.z = std::get<Z>(update_position);
				if (std::get<3>(update_position) == 0x01)
					u.on_ground = true;

				auto update_player_position = std::make_tuple(minecraft::varint(fd), (short)(u.x * 4096 - u.prev_x * 4096),
															(short)(u.y * 4096 - u.prev_y * 4096), (short)(u.z * 4096 - u.prev_z * 4096), u.on_ground);
				send_all_except_user(update_player_position, u, 0x2E);
				break;
			}
			case 0x1E:
			{
				std::tuple<double, double, double, float, float ,char> update_position_rotation;
				update_position_rotation = netlib::read_packet(update_position_rotation, packet);
				u.prev_x = u.x;
				u.prev_y = u.y;
				u.prev_z = u.z;
				u.x = std::get<X>(update_position_rotation);
				u.y = std::get<Y>(update_position_rotation);
				u.z = std::get<Z>(update_position_rotation);
				u.yaw = std::get<YAW>(update_position_rotation);
				u.pitch = std::get<PITCH>(update_position_rotation);
				if (std::get<FLAG>(update_position_rotation) == 0x01)
					u.on_ground = true;

				auto update_player_position = std::make_tuple(minecraft::varint(fd), (short)(u.x * 4096 - u.prev_x * 4096),
											(short)(u.y * 4096 - u.prev_y * 4096), (short)(u.z * 4096 - u.prev_z * 4096),
											(char)(u.yaw/360 * 256), (char)(u.pitch/360 * 256), u.on_ground);
				send_all_except_user(update_player_position, u, 0x2F);
				break;
			}
			case 0x1F:
			{
				std::tuple<float, float, char> update_rotation;
				update_rotation = netlib::read_packet(update_rotation, packet);
				u.yaw = std::get<0>(update_rotation);
				u.pitch = std::get<1>(update_rotation);
				if (std::get<2>(update_rotation) == 0x01)
					u.on_ground = true;
				break;
			}
			case 0x20:
			{
				std::tuple<char> update_flags;
				update_flags = netlib::read_packet(update_flags, packet);
				if (std::get<0>(update_flags) == 0x01)
					u.on_ground = true;
			}
		}
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
			netlib::send_packet(keep_alive, u.first, 0x26);
			u.second.sent = true;
			std::println("Sent keep alive");
		}
		if (u.second.ticks_to_keepalive == 3000)
		{
			sv.disconnect_client(u.first);
			users.erase(u.first);
			std::println("User timed out");
		}
	}
}

int main()
{
	using clock = std::chrono::system_clock;
	using ms = std::chrono::duration<double, std::milli>;
	server sv{};
	auto ret = sv.open_server("0.0.0.0", 25565);
	if (!ret)
	{
		std::println("Opening server failed: {}", ret.error());
		return -1;
	}

	while (true)
	{
		const auto before = clock::now();
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
		update_keep_alive(sv);
		const ms duration = clock::now() - before;
		//std::println("MSPT {}ms", duration.count());
		if (duration.count() <= 50)
			std::this_thread::sleep_for(std::chrono::milliseconds(50) - duration);
	}
	return 0;
}
