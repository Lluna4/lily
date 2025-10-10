#include <print>
#include <chrono>
#include <map>
#include "user.h"
#include "networking/mc_netlib.h"
#include "packet_arguments.h"
#include "registry.h"
#include "block_registry_processing.h"

std::map<int, user> users;
int chat_id = 0;
world w;
std::map<int, std::string> items;
json_value blocks;

template <typename ...T>
void send_all_except_user(std::tuple<T...> packet, user &u, int id, server &sv)
{
	for (auto &us: users)
	{
		//std::println("Checking user {}", us.second.fd);
		if (us.second.fd != u.fd && us.second.state == STATE::PLAY)
		{
			std::println("Sending to user {}", us.second.fd);
			sv.send_packet(packet, us.second.fd, id);
		}
	}
}

template <typename ...T>
void send_all(std::tuple<T...> packet, int id, server &sv)
{
	for (auto &u: users)
	{
		if (u.second.state == STATE::PLAY)
			sv.send_packet(packet, u.first, id);
	}
}

template <typename ...T>
void send_render_distance(std::tuple<T...> packet, int id, server &sv, double x, double z)
{
	double chunk_x = floor((float)x/16.0f);
	double chunk_z = floor((float)z/16.0f);

	for (auto &u: users)
	{
		double user_chunk_x = floor((float)u.second.x/16.0f);
		double user_chunk_z = floor((float)u.second.z/16.0f);
		if (abs(chunk_x - user_chunk_x) <= u.second.view_distance && abs(chunk_z - user_chunk_z) <= u.second.view_distance)
		{
			if (u.second.state == STATE::PLAY)
			{
				sv.send_packet(packet, u.first, id);
				std::println("Sent in render distance");
			}
		}
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
				sv.send_packet(login_success, u.fd, 0x02);
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
				sv.send_packet(known_packs, fd, 0x0E);
				chat_id = send_registry(fd, sv);
				sv.send_packet(fd, 0x03);
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
				sv.send_packet(login, fd, 0x2B);
				auto sync_pos = std::make_tuple(minecraft::varint(1), u.x, u.y, u.z, (double)0.0f, (double)0.0f,
												(double)0.0f, u.yaw, u.pitch, (int)0);
				sv.send_packet(sync_pos, fd, 0x41);
				auto add_to_list = std::make_tuple((char)0x01, minecraft::varint(1), u.uuid, u.name, minecraft::varint(0));
				send_all_except_user(add_to_list, u, 0x3F, sv);
				auto spawn_entity = std::make_tuple(minecraft::varint(fd), u.uuid, minecraft::varint(149),
													u.x, u.y, u.z, (char)(u.pitch/360 * 256), (char)(u.yaw/360 * 256),
													(char)(u.yaw/360 * 256), minecraft::varint(0), (short)0,
													(short)0, (short)0);
				send_all_except_user(spawn_entity, u, 0x01, sv);
				for (auto &us: users)
				{
					if (us.second.fd != u.fd)
					{
						auto add_to_list_user = std::make_tuple((char)0x01, minecraft::varint(1), us.second.uuid, us.second.name, minecraft::varint(0));
						sv.send_packet(add_to_list_user, fd, 0x3F);
						auto spawn_entity_user = std::make_tuple(minecraft::varint(us.second.fd), us.second.uuid, minecraft::varint(149),
									us.second.x, us.second.y, us.second.z, (char)((us.second.pitch/360) * 256),
									(char)((us.second.yaw/360) * 256),(char)((us.second.yaw/360) * 256), minecraft::varint(0), (short)0,
									(short)0, (short)0);
						sv.send_packet(spawn_entity_user, fd, 0x01);
					}
				}

				auto set_effect = std::make_tuple(minecraft::varint(fd), minecraft::varint(15), minecraft::varint(1),
												minecraft::varint(99999999), (char)0x04);
				sv.send_packet(set_effect, fd, 0x7D);
				auto game_event = std::make_tuple((unsigned char)13, 0.0f);
				sv.send_packet(game_event, fd, 0x22);
				auto set_center_chunk = std::make_tuple(minecraft::varint(0), minecraft::varint(0));
				sv.send_packet(set_center_chunk, fd, 0x57);
				for (int y = -u.view_distance; y < u.view_distance; y++)
				{
					for (int x = -u.view_distance; x < u.view_distance; x++)
					{
						chunk &c = w.get_chunk(x, y);
						auto chunk_data = std::make_tuple(x, y, minecraft::varint(0),c, minecraft::varint(0),
									minecraft::varint(0),minecraft::varint(0),minecraft::varint(0),
									minecraft::varint(0),minecraft::varint(0), minecraft::varint(0));
						sv.send_packet(chunk_data, fd, 0x27);
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
				std::println("{}", chat_id);
				auto message = std::make_tuple((char)0x0a, minecraft::string_tag(std::get<0>(chat_message).data.data, "text"), (char)0x00,
											minecraft::varint(chat_id + 1), (char)0x0a, minecraft::string_tag(std::format("{} [{}]", u.name, u.pronouns), "text"), (char)0x00, false);
				send_all(message, 0x1D, sv);
				break;
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
				break;
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
				send_all_except_user(update_player_position, u, 0x2E, sv);
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
											(char)((u.yaw/360) * 256), (char)((u.pitch/360) * 256), u.on_ground);
				send_all_except_user(update_player_position, u, 0x2F, sv);

				auto update_head = std::make_tuple(minecraft::varint(fd), (char)((u.yaw/360) * 256));
				send_all_except_user(update_head, u, 0x4C, sv);
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
				auto update_head = std::make_tuple(minecraft::varint(fd), (char)((u.yaw/360) * 256));
				send_all_except_user(update_head, u, 0x4C, sv);
				break;
			}
			case 0x20:
			{
				std::tuple<char> update_flags;
				update_flags = netlib::read_packet(update_flags, packet);
				if (std::get<0>(update_flags) == 0x01)
					u.on_ground = true;
				break;
			}
			case 0x28:
			{
				std::tuple<minecraft::varint, int64_t, char, minecraft::varint> player_action;
				player_action = netlib::read_packet(player_action, packet);

				long pos = std::get<LOCATION>(player_action);
				int x = pos >> 38;
				int y = pos << 52 >> 52;
				int z = pos << 26 >> 38;
				std::println("Setting block at x: {} y: {} z: {}", x, y, z);
				auto ret = w.set_block(x, y, z, 0);
				if (!ret)
					std::println("Block placement failed");

				auto block_update = std::make_tuple((int64_t)((((x & (unsigned long)0x3FFFFFF) << 38) | ((z & (unsigned long)0x3FFFFFF) << 12) | (y & (unsigned long)0xFFF))), minecraft::varint(0));
				send_render_distance(block_update, 0x08, sv, u.x, u.z);
				auto awknowledge_block = std::make_tuple(minecraft::varint(std::get<SEQUENCE>(player_action)));
				sv.send_packet(awknowledge_block, fd, 0x04);
				std::println("Block placed was actually {}", items[0]);
				break;
			}
			case 0x34:
			{
				std::tuple<short> set_held_item;
				set_held_item = netlib::read_packet(set_held_item, packet);
				u.held_item = std::get<0>(set_held_item);

				auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)0, minecraft::varint(1),
												minecraft::varint(u.inventory[u.held_item + 36]), minecraft::varint(0),
												minecraft::varint(0));
				send_all_except_user(set_equipment, u, 0x5F, sv);
				break;
			}
			case 0x37:
			{
				std::tuple<short, minecraft::varint, minecraft::varint> set_creative_slot;
				set_creative_slot = netlib::read_packet(set_creative_slot, packet);
				u.inventory[std::get<SLOT_ID>(set_creative_slot)] = std::get<ITEM_ID>(set_creative_slot).num;
				if (std::get<SLOT_ID>(set_creative_slot) == 45)
				{
					auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)1, minecraft::varint(1),
								minecraft::varint(u.inventory[45]), minecraft::varint(0),
								minecraft::varint(0));
					send_all_except_user(set_equipment, u, 0x5F, sv);
				}
				if (std::get<SLOT_ID>(set_creative_slot) == u.held_item + 36)
				{
					auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)0, minecraft::varint(1),
								minecraft::varint(u.inventory[u.held_item + 36]), minecraft::varint(0),
								minecraft::varint(0));
					send_all_except_user(set_equipment, u, 0x5F, sv);
				}
				break;
			}
			case 0x3C:
			{
				std::tuple<minecraft::varint> swing_arm;
				swing_arm = netlib::read_packet(swing_arm, packet);
				unsigned char anim_id = 0;
				if (std::get<0>(swing_arm).num == 1)
					anim_id = 3;
				auto entity_animation = std::make_tuple(minecraft::varint(fd), anim_id);
				send_all_except_user(entity_animation, u, 0x02, sv);
				break;
			}
			case 0x3F:
			{
				std::tuple<minecraft::varint, int64_t, minecraft::varint, float, float, float, bool, bool, minecraft::varint> use_item_on;
				use_item_on = netlib::read_packet(use_item_on, packet);
				long pos = std::get<1>(use_item_on);
				int x = pos >> 38;
				int y = pos << 52 >> 52;
				int z = pos << 26 >> 38;
				minecraft::varint face = std::get<2>(use_item_on);
				std::println("Face {}", face.num);
				switch (face.num)
				{
					case 0:
						y--;
						break;
					case 1:
						y++;
						break;
					case 2:
						z--;
						break;
					case 3:
						z++;
						break;
					case 4:
						x--;
						break;
					case 5:
						x++;
						break;
				}
				bool colliding = false;
				for (auto &u: users)
				{
					if (u.second.state == STATE::PLAY)
					{
						colliding = u.second.check_collision_block(position(x, y, z));
					}
				}
				if (colliding)
					break;
				json_value block_states = blocks.object[items[u.inventory[u.held_item + 36]]].object["states"];
				long id = 0;
				for (auto &state: block_states.array)
				{
					if (state.object.contains("default"))
					{
						id = state.object["id"].num;
					}
				}

				std::println("Setting block at x: {} y: {} z: {}", x, y, z);
				auto ret = w.set_block(x, y, z, id);
				if (!ret)
					std::println("Block placement failed");
				auto block_update = std::make_tuple((int64_t)((((x & (unsigned long)0x3FFFFFF) << 38) | ((z & (unsigned long)0x3FFFFFF) << 12) | (y & (unsigned long)0xFFF))), minecraft::varint(id));
				send_render_distance(block_update, 0x08, sv, u.x, u.z);
				auto awknowledge_block = std::make_tuple(std::get<8>(use_item_on));
				sv.send_packet(awknowledge_block, fd, 0x04);
				std::println("Block placed is {}", items[u.inventory[u.held_item + 36]]);
				std::println("Id is {}", id);
				break;
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
			sv.send_packet(keep_alive, u.first, 0x26);
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
	process_item_registry("../registries.json", items);
	blocks = process_block_registry("../blocks.json");
	std::println("Added block registry");
	while (true)
	{
		const auto before = clock::now();
		auto packets = sv.get_packets();
		for (auto &pkt: packets)
		{
			if (pkt.id == -1)
			{
				std::println("Client disconnected");
				users.erase(pkt.fd);
				continue;
			}
			//std::println("Got a packet from fd {} with id {} and size {}", pkt.fd, pkt.id, pkt.size);
			execute_packet(pkt.fd, pkt, sv);
		}
		update_keep_alive(sv);
		const ms duration = clock::now() - before;
		//std::println("MSPT {}ms", duration.count());
		if (duration.count() <= 50)
			std::this_thread::sleep_for(std::chrono::milliseconds(50) - duration);
	}
	return 0;
}
