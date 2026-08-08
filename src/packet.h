#pragma once
#include "../netlib_/src/networking.h"
#include "registry.h"
#include "deserialize.h"
#include "mc_types.h"
#include "user.h"
#include "log.h"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <variant>
#include "chunk_send.h"


int chat_id = 0;
std::vector<std::string> biomes;
std::vector<std::string> dimensions;
int spawn_y = 100;
std::unordered_map<int, std::string> items;

static std::vector<std::bitset<6>> proc_6bit(std::string hi)
{
	std::string bits;
	std::vector<std::bitset<8>> vec;
	std::vector<std::bitset<6>> vec2;
	int index = 0;
	std::string res;
	for (int i = 0; i < hi.length(); i++)
	{
		vec.emplace_back(hi[i]);
	}
	for (int i = 0; i < vec.size(); i++)
		bits.append(vec[i].to_string());

	while(index < bits.length())
	{
		if (index > 0 && index%6 == 0)
		{
			vec2.emplace_back(res);
			res.clear();
		}
		res.push_back(bits[index]);
		index++;
	}
	while (res.length() < 6)
		res.push_back('0');
	vec2.emplace_back(res);

	return vec2;
}

std::string base64_encode(std::string file_path)
{
	std::string ret;
	if (std::filesystem::exists(file_path))
	{
		std::ifstream file(file_path, std::ios::binary);
		std::string buf;
		std::stringstream buffer;
		buffer << file.rdbuf();
		buf = buffer.str();
		std::vector<std::bitset<6>>nums = proc_6bit(buf); //converts 8 bit nums into a 6 bit array

		for (int i = 0; i < nums.size();i++)
		{
			unsigned long num = nums[i].to_ulong();
			if (num < 26)
				ret.push_back(num+ 'A');
			else if (num >= 26 && num < 52)
				ret.push_back((num - 26) + 'a');
			else if(num >= 52 && num < 62)
				ret.push_back((num - 52) + '0');
			else if (num == 62)
				ret.push_back('+');
			else if (num == 63)
				ret.push_back('/');
		}
		if (ret.length()%4 != 0)
		{
			while(ret.length()%4 != 0)
				ret.push_back('=');
		}
	}
	return ret;
}


struct packet_base
{
	virtual ~packet_base() = default;

	std::string name;
	std::function<void(server &sv, std::map<int, user> &users,std::vector<int> &disconnected, int fd)> handle;
	virtual void parse(packet &pkt) = 0;
};

template <typename T>
struct packet_executer: public packet_base
{
	packet_executer(std::string n, std::function<void(server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, T &data)> h)
	{
		this->name = n;
		this->handle = [this, h](server& sv, std::map<int, user>& users, std::vector<int>& disconnected, int fd)
		{
			h(sv, users, disconnected, fd, this->contents);
		};
	}

	T contents;

	void parse(packet &pkt)
	{
		deserialize(contents, pkt.data.get() + pkt.header_offset);
	}
};


struct handshake
{
	minecraft::varint version;
	minecraft::string address;
	unsigned short port;
	minecraft::varint intent;
};

struct status_response
{
	std::string data;
};

struct pong
{
	uint64_t timestamp;
};

struct login_start
{
	minecraft::string name;
};

struct login_success
{
	minecraft::uuid uuid;
	std::string name;
	minecraft::varint size;
};

struct client_info 
{
	minecraft::string locale;
	char view_distance;
	minecraft::varint chat_mode;
	bool char_colors;
	unsigned char skin_parts;
	minecraft::varint main_hand;
	bool text_filtering;
	bool server_list;
	minecraft::varint particle_level;
};

struct known_packs
{
	minecraft::varint pack_num;
	std::string namesp;
	std::string id;
	std::string version;
};

struct login_play 
{
	int32_t entity_id;
	bool is_hardcore;
	minecraft::varint num;
	std::string dimension_names;
	minecraft::varint max_players;
	minecraft::varint view_distance;
	minecraft::varint simulation_distance;
	bool reduced_debug_info;
	bool enable_respawn_screen;
	bool do_limited_crafting;
	minecraft::varint dimension_type;
	std::string dimension_name;
	int64_t hashed_seed;
	unsigned char game_mode;
	signed char previous_game_mode;
	bool is_debug;
	bool is_flat;
	bool has_death_location;
	minecraft::varint portal_cooldown;
	minecraft::varint sea_level;
	bool enforces_secure_chat;
};


struct player_position
{
	minecraft::varint teleport_id;
	double x;
	double y;
	double z;
	double velocity_x;
	double velocity_y;
	double velocity_z;
	float yaw;
	float pitch;
	int32_t flags;
};

struct game_event
{
	uint8_t event;
	float value;
};

struct set_center_chunk
{
	minecraft::varint x;
	minecraft::varint z;
};

struct entity_effect
{
	minecraft::varint entity_id;
	minecraft::varint effect_id;
	minecraft::varint amplifier;
	minecraft::varint duration;
	char flags;
	bool a;
};

struct keep_alive
{
	long value;
};

struct player_position_play
{
	double x;
	double feet_y;
	double z;
	bool flags;
};

struct player_position_rotation_play
{
	double x;
	double feet_y;
	double z;
	float yaw;
	float pitch;
	bool flags;
};

void stream_world(user &u, server &sv)
{
	if (u.chunk_x != u.prev_chunk_x || u.chunk_z != u.prev_chunk_z)
	{
		set_center_chunk center = {minecraft::varint((unsigned long)(*(unsigned int *)&u.chunk_x)), minecraft::varint((unsigned long)(*(unsigned int *)&u.chunk_z))};
		send_packet(u.fd, 0x5C, center, sv);
	}
	else
		return;
	if (u.chunk_x != u.prev_chunk_x)
	{
		int chunk_start_x = u.chunk_x + u.view_distance;

		if (u.chunk_x < u.prev_chunk_x)
			chunk_start_x = u.chunk_x - u.view_distance;
		std::vector<std::pair<int, int>> positions;
		for (int x = chunk_start_x - 1; x < chunk_start_x + 2; x++)
		{
			for (int z = u.chunk_z - u.view_distance - 1; z < u.chunk_z + u.view_distance + 1; z++)
			{
				positions.push_back(std::make_pair(x, z));
			}
		}
		send_chunks(u.fd, positions);
		//send_system_chat("Moved chunks in x", users, sv);
	}
	if (u.chunk_z != u.prev_chunk_z)
	{
		int chunk_start_z = u.chunk_z + u.view_distance;

		if (u.chunk_z < u.prev_chunk_z)
			chunk_start_z = u.chunk_z - u.view_distance;
		
		std::vector<std::pair<int, int>> positions;
		for (int z = chunk_start_z - 1; z < chunk_start_z + 2; z++)
		{
			for (int x = u.chunk_x - u.view_distance - 1; x < u.chunk_x + u.view_distance + 1; x++)
			{
				positions.push_back(std::make_pair(x, z));
			}
		}
		send_chunks(u.fd, positions);
		//send_system_chat("Moved chunks in z", users, sv);
	}

}


void set_packets(std::map<int, std::unique_ptr<packet_base>> &packet_definitions_handshake, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_status, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_login, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_config, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_play)
{
	    packet_definitions_handshake[0x0] = std::make_unique<packet_executer<handshake>>(
		"Handshake",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, handshake &data)
		{
			user &u = users.find(fd)->second;
			log(std::format("Protocol version is {}", data.version.num), LOG_LEVEL::NORMAL);
            log(std::format("fd is {}", fd), LOG_LEVEL::NORMAL);
			if (data.version.num != 773)
			{
				sv.disconnect_client(fd);
				u.state = STATE::DISCONNECTED;
				disconnected.push_back(fd);
				log("Protocol version mismatch!", LOG_LEVEL::WARNING);
				return;
			}
			if (data.intent.num == 1)
				u.state = STATE::STATUS;
			else if (data.intent.num == 2)
				u.state = STATE::LOGIN;
		}
	);

	packet_definitions_status[0x00] = std::make_unique<packet_executer<std::monostate>>(
			"Status request",
			[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, std::monostate &data)
			{
				log("Status request", LOG_LEVEL::NORMAL);
				user &u = users.find(fd)->second;
				int play_users = 0;
				for (auto &u: users)
				{
					if (u.second.state == STATE::PLAY)
						play_users++;
				}

				// Compact JSON string to avoid whitespace/newline bloat
				std::string json_response = std::format("{{ \
										\"version\": {{ \
											\"name\": \"1.21.10\", \
											\"protocol\": 773 \
										}}, \
										\"players\": {{ \
											\"max\": 20, \
											\"online\": {} \
										}}, \
										\"description\": {{ \
											\"text\": \"{}\" \
										}}, \
										\"enforcesSecureChat\": false \
										}}", play_users, "hi!");

				status_response resp = {.data = json_response};
				send_packet(fd, 0x00, resp, sv);

			}
		);

	packet_definitions_status[0x01] = std::make_unique<packet_executer<pong>>(
		"Ping",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, pong &data)
		{
			log("Ping!", LOG_LEVEL::NORMAL);
			user &u = users.find(fd)->second;
			pong resp = {.timestamp = data.timestamp};
			send_packet(fd, 0x01, resp, sv);
			//disconnected.push_back(fd);
		}
	);

	packet_definitions_login[0x00] = std::make_unique<packet_executer<login_start>>(
		"Login start",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, login_start &login)
		{
			user &u = users.find(fd)->second;
            u.name = login.name.data.get();
            u.uuid.generate(login.name.data.get());
            log(std::format("Name is {}", login.name.data.get()), LOG_LEVEL::NORMAL);

            login_success resp = {.uuid = u.uuid, .name = u.name.data(), .size = minecraft::varint(0)};
			send_packet(fd, 0x2, resp, sv);
		}
	);

	packet_definitions_login[0x03] = std::make_unique<packet_executer<std::monostate>>(
		"Login Acknowledged",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, std::monostate &data)
		{
			user &u = users.find(fd)->second;
            u.state = STATE::CONFIGURATION;
            log("Login awknowledged", LOG_LEVEL::NORMAL);
		}
	);

	 packet_definitions_config[0x00] = std::make_unique<packet_executer<client_info>>(
		"Client info",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, client_info &data)
		{
			user &u = users.find(fd)->second;
            log(std::format("Locale {} View distance {}", data.locale.data.get(), (int)data.view_distance), LOG_LEVEL::NORMAL);
            u.locale = data.locale.data.get();
            u.view_distance = data.view_distance;

			known_packs known = {.pack_num = minecraft::varint(1), .namesp = "minecraft", .id = "core", .version = "1.21.10"};
			send_packet(fd, 0xE, known, sv);
            chat_id = send_registry(fd, sv);
			std::monostate dat;
			send_packet(fd, 0x3, dat, sv);
		}
	);

	packet_definitions_config[0x03] = std::make_unique<packet_executer<std::monostate>>(
		"Finish config",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, std::monostate &data)
		{
			user &u = users.find(fd)->second;
            u.state = STATE::PLAY;
			login_play log = {fd, false, minecraft::varint(1), "minecraft:overworld",
								minecraft::varint(20), minecraft::varint(u.view_distance),
								minecraft::varint(u.view_distance), false, true, false,
								minecraft::varint(std::distance(dimensions.begin(), std::find(dimensions.begin(), dimensions.end(), "overworld"))),
								"minecraft:overworld", 128612, 1, -1, false,
								false, false, minecraft::varint(0), minecraft::varint(64), false
							};
			u.y = spawn_y;
			send_packet(fd, 0x30, log, sv);
			player_position pos = {minecraft::varint(1), u.x, u.y, u.z, 0.0f, 0.0f,
									0.0f, u.yaw, u.pitch, 0};
			send_packet(fd, 0x46, pos, sv);
			entity_effect effect = {minecraft::varint(fd), minecraft::varint(15),
				minecraft::varint(1), minecraft::varint(999999), 0x04, false};
			send_packet(fd, 0x82, effect, sv);
			game_event wait_for_chunks = {13, 0.0f};
			send_packet(fd, 0x26, wait_for_chunks, sv);
			set_center_chunk center = {minecraft::varint(0), minecraft::varint(0)};
			send_packet(fd, 0x5C, center, sv);
			
			std::vector<std::pair<int, int>> positions;
            for (int y = -u.view_distance - 2; y < u.view_distance + 2; y++)
            {
                for (int x = -u.view_distance - 2; x < u.view_distance + 2; x++)
                {
                    positions.push_back(std::make_pair(x, y));
                }
            }
			send_chunks(fd, positions);
            u.state = STATE::PLAY;
		}
	);

	packet_definitions_play[0x1B] = std::make_unique<packet_executer<keep_alive>>(
		"Keep alive",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, keep_alive &k)
		{
			user &u = users.find(fd)->second;
			if (k.value != 4 || u.sent == false)
			{
				sv.disconnect_client(fd);
				u.state = STATE::DISCONNECTED;
				disconnected.push_back(fd);
				return;
			}
			u.ticks_to_keepalive = 500;
			u.sent = false;
			log("Received keep alive", LOG_LEVEL::NORMAL);
		}
	);
	packet_definitions_play[0x1D] = std::make_unique<packet_executer<player_position_play>>(
		"Set player position",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, player_position_play &pos)
		{
			user &u = users.find(fd)->second;
			u.prev_x = u.x;
			u.prev_y = u.y;
			u.prev_z = u.z;
			u.prev_chunk_x = u.chunk_x;
			u.prev_chunk_z = u.chunk_z;
			u.x = pos.x;
			u.y = pos.feet_y;
			u.z = pos.z;
			u.chunk_x = floor((float)u.x/16.0f);
			u.chunk_z = floor((float)u.z/16.0f);
			if (pos.flags == 0x01)
				u.on_ground = true;

			stream_world(u, sv);
		}
	);


	packet_definitions_play[0x1E] = std::make_unique<packet_executer<player_position_rotation_play>>(
		"Set player position and rotation",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, player_position_rotation_play &pos)
		{
			user &u = users.find(fd)->second;
			u.prev_x = u.x;
			u.prev_y = u.y;
			u.prev_z = u.z;
			u.prev_chunk_x = u.chunk_x;
			u.prev_chunk_z = u.chunk_z;
			u.x = pos.x;
			u.y = pos.feet_y;
			u.z = pos.z;
			u.chunk_x = floor((float)u.x/16.0f);
			u.chunk_z = floor((float)u.z/16.0f);
			u.yaw = pos.yaw;
			u.pitch = pos.pitch;
			if (pos.flags == 0x01)
				u.on_ground = true;

			stream_world(u, sv);
		}
	);

}

