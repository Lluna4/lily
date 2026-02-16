#pragma once

#include <vector>
#include <tuple>
#include <string>
#include "mc_types.h"
#include "networking/comp_time_read.h"
#include "networking/mc_netlib.h"
#include "log.h"
#include "registry.h"
#include "chat.h"
#include "chunk_send.h"
#include "split.h"
#include "user.h"

int chat_id = 0;
int spawn_y = 64;
world w;
std::unordered_map<int, std::string> items;
std::vector<std::string> dimensions;
std::string motd = "Lily server!";
std::string img_path = "ico.png";


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


template <typename ...T>
void send_all_except_user(std::tuple<T...> packet, user &u, int id, server &sv, std::map<int, user> &users)
{
	for (auto &us: users)
	{
		//std::println("Checking user {}", us.second.fd);
		if (us.second.fd != u.fd && us.second.state == STATE::PLAY)
		{
			sv.send_packet(packet, us.second.fd, id);
		}
	}
}

template <typename ...T>
void send_all_except_user_render_distance(std::tuple<T...> packet, user &u, int id, server &sv, double x, double z, std::map<int, user> &users)
{
	double chunk_x = floor((float)x/16.0f);
	double chunk_z = floor((float)z/16.0f);

	for (auto &us: users)
	{
		//std::println("Checking user {}", us.second.fd);
		if (us.second.fd != u.fd && us.second.state == STATE::PLAY)
		{
			double user_chunk_x = floor((float)us.second.x/16.0f);
			double user_chunk_z = floor((float)us.second.z/16.0f);
			if (abs(chunk_x - user_chunk_x) <= us.second.view_distance && abs(chunk_z - user_chunk_z) <= us.second.view_distance)
			{
				if (us.second.state == STATE::PLAY)
				{
					sv.send_packet(packet, us.first, id);
				}
			}
		}
	}
}

template <typename ...T>
void send_all(std::tuple<T...> packet, int id, server &sv, std::map<int, user> &users)
{
	for (auto &u: users)
	{
		if (u.second.state == STATE::PLAY)
			sv.send_packet(packet, u.first, id);
	}
}

template <typename ...T>
void send_render_distance(std::tuple<T...> packet, int id, server &sv, double x, double z, std::map<int, user> &users)
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
			}
		}
	}
}

void stream_world(user &u, server &sv)
{
	if (u.chunk_x != u.prev_chunk_x || u.chunk_z != u.prev_chunk_z)
	{
		auto set_center_chunk = std::make_tuple(minecraft::varint((unsigned long)(*(unsigned int *)&u.chunk_x)), minecraft::varint((unsigned long)(*(unsigned int *)&u.chunk_z)));
		sv.send_packet(set_center_chunk, u.fd, 0x57);
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

struct packet_base
{
    virtual ~packet_base() = default;

    std::string name;
    std::function<void(server &sv, std::map<int, user> &users,std::vector<int> &disconnected, int fd)> handle;
    virtual void parse(netlib::packet &pkt) = 0;
};

template <typename ...T>
struct packet: public packet_base
{
    packet(std::string n, std::function<void(server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, T&...)> h)
    {
        this->name = n;
        this->handle = [this, h](server& sv, std::map<int, user>& users, std::vector<int>& disconnected, int fd)
        {
            std::apply([&](auto & ...args)
			{
				h(sv, users, disconnected, fd, args...);
			}, this->contents);
        };
    }

    std::tuple<T...> contents;

    void parse(netlib::packet &pkt)
    {
        netlib::read_packet(contents, pkt);
    }
};

void set_packets(std::map<int, std::unique_ptr<packet_base>> &packet_definitions_handshake, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_status, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_login, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_config, std::map<int, std::unique_ptr<packet_base>> &packet_definitions_play)
{
    packet_definitions_handshake[0x0] = std::make_unique<packet<minecraft::varint, minecraft::string, unsigned short, minecraft::varint>>(
		"Handshake",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::varint &version, minecraft::string &address, unsigned short &port, minecraft::varint &intent)
		{
			user &u = users.find(fd)->second;
			log(std::format("Protocol version is {}", version.num), LOG_LEVEL::NORMAL);
            log(std::format("fd is {}", fd), LOG_LEVEL::NORMAL);
			if (version.num != 772)
			{
				sv.disconnect_client(fd);
				u.state = STATE::DISCONNECTED;
				disconnected.push_back(fd);
				log("Protocol version mismatch!", LOG_LEVEL::WARNING);
				return;
			}
			if (intent.num == 1)
				u.state = STATE::STATUS;
			else if (intent.num == 2)
				u.state = STATE::LOGIN;
		}
	);

	packet_definitions_status[0x00] = std::make_unique<packet<>>(
		"Status request",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd)
		{
			user &u = users.find(fd)->second;
			int play_users = 0;
			for (auto &u: users)
			{
				if (u.second.state == STATE::PLAY)
					play_users++;
			}
			std::string base64 = std::format("data:image/png;base64,{}", base64_encode(img_path));
			std::string json_response = std::format("{{\
											\"version\": {{ \
												\"name\": \"1.21.8\", \
												\"protocol\": 772 \
											}}, \
											\"players\": {{ \
												\"max\": 20, \
												\"online\": {} \
											}}, \
											\"description\": {{ \
												\"text\": \"{}\" \
											}}, \
											\"favicon\": \"{}\", \
											\"enforcesSecureChat\": false \
											}}", play_users, motd, base64);
			auto status_response = std::make_tuple(json_response);
			sv.send_packet(status_response, fd, 0x00);
		}
	);

	packet_definitions_status[0x01] = std::make_unique<packet<long>>(
		"Ping",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, long &timestamp)
		{
			log("Ping!", LOG_LEVEL::NORMAL);
			user &u = users.find(fd)->second;
			auto pong = std::make_tuple(timestamp); 
			sv.send_packet_dc(pong, fd, 0x01);
		}
	);

    packet_definitions_login[0x00] = std::make_unique<packet<minecraft::string, minecraft::uuid>>(
		"Login start",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::string &name, minecraft::uuid &uuid)
		{
			user &u = users.find(fd)->second;
            u.name = name.data.data;
            u.uuid.generate(name.data.data);
            log(std::format("Name is {}", name.data.data), LOG_LEVEL::NORMAL);
            auto login_success = std::make_tuple(u.uuid, u.name, minecraft::varint(0));
			sv.send_packet(login_success, u.fd, 0x02);
		}
	);

    packet_definitions_login[0x03] = std::make_unique<packet<>>(
		"Login Acknowledged",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd)
		{
			user &u = users.find(fd)->second;
            u.state = STATE::CONFIGURATION;
            log("Login awknowledged", LOG_LEVEL::NORMAL);
		}
	);


    packet_definitions_config[0x00] = std::make_unique<packet<minecraft::string, char, minecraft::varint, bool, unsigned char, minecraft::varint, bool, bool, minecraft::varint>>(
		"Client info",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::string &locale, char &view_distance, minecraft::varint &chat_mode, bool &chat_colors, unsigned char &skin_parts, minecraft::varint &main_hand, bool &text_filtering, bool &server_listings, minecraft::varint &particle_status)
		{
			user &u = users.find(fd)->second;
            log(std::format("Locale {} View distance {}", locale.data.data, (int)view_distance), LOG_LEVEL::NORMAL);
            u.locale = locale.data.data;
            u.view_distance = view_distance;

            auto known_packs = std::make_tuple(minecraft::varint(1), std::string("minecraft"), std::string("core"), std::string("1.21.8"));
            sv.send_packet(known_packs, fd, 0x0E);
            chat_id = send_registry(fd, sv);

            sv.send_packet(fd, 0x03);
		}
	);


    packet_definitions_config[0x02] = std::make_unique<packet<minecraft::string>>(
		"Plugin message",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::string &plugin_message)
		{
			user &u = users.find(fd)->second;
            log(std::format("Plugin message sent from channel {}", plugin_message.data.data), LOG_LEVEL::NORMAL);
		}
	);

    packet_definitions_config[0x03] = std::make_unique<packet<>>(
		"Finish configuration",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd)
		{
            user &u = users.find(fd)->second;
            auto login = std::make_tuple((int)fd, false,minecraft::varint(1) ,(std::string)"minecraft:overworld",
                                        minecraft::varint(20), minecraft::varint(u.view_distance),
                                        minecraft::varint(12), false, false, false, minecraft::varint(std::distance(dimensions.begin(), std::find(dimensions.begin(), dimensions.end(), "overworld"))),
                                        (std::string)"minecraft:overworld", (long)128612, (unsigned char)1,
                                        (char)-1, false, false, false, minecraft::varint(0), minecraft::varint(64),
                                        false);
            auto commands = std::make_tuple(minecraft::varint(7),
                            (char)0x00, minecraft::varint(6), minecraft::varint(1), minecraft::varint(2), minecraft::varint(3),minecraft::varint(4), minecraft::varint(5), minecraft::varint(6),
                            (char)0x01, minecraft::varint(1), minecraft::varint(2), std::string("pronouns"),
                            (char)0x02, minecraft::varint(0), std::string("pronouns"), minecraft::varint(5), minecraft::varint(2),
							(char)0x01, minecraft::varint(1), minecraft::varint(4), std::string("tp"),
							(char)0x02, minecraft::varint(1), minecraft::varint(5), std::string("x"), minecraft::varint(4), (char)0x0,
                            (char)0x02, minecraft::varint(1), minecraft::varint(6), std::string("y"), minecraft::varint(4), (char)0x0,
							(char)0x02, minecraft::varint(0), std::string("z"), minecraft::varint(4), (char)0x0,
							minecraft::varint(0));
            sv.send_packet(commands, fd, 0x10);
            u.y = spawn_y;
            sv.send_packet(login, fd, 0x2B);
            auto sync_pos = std::make_tuple(minecraft::varint(1), u.x, u.y, u.z, (double)0.0f, (double)0.0f,
                                            (double)0.0f, u.yaw, u.pitch, (int)0);
            sv.send_packet(sync_pos, fd, 0x41);
            auto add_to_list = std::make_tuple((char)(0x01 | 0x04 | 0x08 | 0x20), minecraft::varint(1), u.uuid, u.name, minecraft::varint(0),
                                                minecraft::varint(1), true, true,(char)0x0a ,minecraft::string_tag(std::format("{} [{}]", u.name, u.pronouns), "text"), (char)0x00);
            send_all(add_to_list,0x3F, sv, users);
			sv.send_packet(add_to_list, fd, 0x3F);
            auto spawn_entity = std::make_tuple(minecraft::varint(fd), u.uuid, minecraft::varint(149),
                                                u.x, u.y, u.z, (char)(u.pitch/360 * 256), (char)(u.yaw/360 * 256),
                                                (char)(u.yaw/360 * 256), minecraft::varint(0), (short)0,
                                                (short)0, (short)0);
            send_all_except_user(spawn_entity, u, 0x01,sv,users);
            for (auto &us: users)
            {
                if (us.second.fd != u.fd)
                {

                    auto add_to_list_user = std::make_tuple((char)(0x01 | 0x08 | 0x20), minecraft::varint(1), us.second.uuid, us.second.name, minecraft::varint(0),
                                                true, true, (char)0x0a, minecraft::string_tag(std::format("{} [{}]", us.second.name, us.second.pronouns), "text"), (char)0x00);
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
			std::vector<std::pair<int, int>> positions;
            for (int y = -u.view_distance - 2; y < u.view_distance + 2; y++)
            {
                for (int x = -u.view_distance - 2; x < u.view_distance + 2; x++)
                {
                    positions.push_back(std::make_pair(x, y));
                }
            }
			send_chunks(u.fd, positions);
            u.state = STATE::PLAY;
            send_system_chat(std::format("{} connected", u.name), users, sv);
		}
	);



	packet_definitions_play[0x00] = std::make_unique<packet<minecraft::varint>>(
		"Teleport confirmed",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::varint &id)
		{
			log(std::format("Teleport confirmed {}", id.num), LOG_LEVEL::NORMAL);
		}
	);


	packet_definitions_play[0x06] = std::make_unique<packet<minecraft::string>>(
		"Command",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::string &command)
		{
			user &u = users.find(fd)->second;
			std::string comm(command.data.data);

			if (comm.starts_with("pronouns"))
			{
				std::string pronouns = comm.substr(comm.find(' ') + 1);
				u.pronouns = pronouns;
			}

			if (comm.starts_with("tp"))
			{
				std::vector<std::string> tokens = split_str(comm, ' ');
				u.x = std::atol(tokens[1].c_str());
				u.y = std::atol(tokens[2].c_str());
				u.z = std::atol(tokens[3].c_str());

				long chunkX = (long)std::floor(u.x / 16.0);
    			long chunkZ = (long)std::floor(u.z / 16.0);

				auto sync_pos = std::make_tuple(minecraft::varint(1), u.x, u.y, u.z, (double)0.0f, (double)0.0f,
                                            (double)0.0f, u.yaw, u.pitch, (int)0);
            	sv.send_packet(sync_pos, fd, 0x41);

				auto teleport_entity = std::make_tuple(minecraft::varint(fd), u.x, u.y, u.z, (double)0.0f,
														(double)0.0f, (double)0.0f, u.yaw, u.pitch, u.on_ground);
				send_all_except_user(teleport_entity, u, 0x1F, sv, users);
				
				auto set_center_chunk = std::make_tuple(minecraft::varint(chunkX), minecraft::varint(chunkZ));
            	sv.send_packet(set_center_chunk, fd, 0x57);
				std::vector<std::pair<int, int>> positions;
				for (long y = (chunkZ - u.view_distance) - 2; y < (chunkZ + u.view_distance) + 2; y++)
				{
					for (long x = (chunkX - u.view_distance) - 2; x < (chunkX + u.view_distance) + 2; x++)
					{
						positions.push_back(std::make_pair(x, y));
					}
				}
				send_chunks(u.fd, positions);
			}
		}
	);


	packet_definitions_play[0x08] = std::make_unique<packet<minecraft::string>>(
		"Chat message",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::string &message)
		{
			user &u = users.find(fd)->second;
			send_chat(message.data.data, std::format("{} [{}]", u.name, u.pronouns), chat_id, sv, users);
		}
	);


	packet_definitions_play[0x1B] = std::make_unique<packet<long>>(
		"Keep alive",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, long &id)
		{
			user &u = users.find(fd)->second;
			if (id != 4 || u.sent == false)
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


	packet_definitions_play[0x1D] = std::make_unique<packet<double, double, double, char>>(
		"Set player position",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, double &x, double &y, double &z, char &flags)
		{
			user &u = users.find(fd)->second;
			u.prev_x = u.x;
			u.prev_y = u.y;
			u.prev_z = u.z;
			u.prev_chunk_x = u.chunk_x;
			u.prev_chunk_z = u.chunk_z;
			u.x = x;
			u.y = y;
			u.z = z;
			u.chunk_x = floor((float)u.x/16.0f);
			u.chunk_z = floor((float)u.z/16.0f);
			if (flags == 0x01)
				u.on_ground = true;

			auto update_player_position = std::make_tuple(minecraft::varint(fd), (short)(u.x * 4096 - u.prev_x * 4096),
														(short)(u.y * 4096 - u.prev_y * 4096), (short)(u.z * 4096 - u.prev_z * 4096), u.on_ground);
			send_all_except_user(update_player_position, u, 0x2E, sv, users);
			stream_world(u, sv);
		}
	);


	packet_definitions_play[0x1E] = std::make_unique<packet<double, double, double, float, float, char>>(
		"Set player position and rotation",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, double &x, double &y, double &z, float &yaw, float &pitch, char &flags)
		{
			user &u = users.find(fd)->second;
			u.prev_x = u.x;
			u.prev_y = u.y;
			u.prev_z = u.z;
			u.prev_chunk_x = u.chunk_x;
			u.prev_chunk_z = u.chunk_z;
			u.x = x;
			u.y = y;
			u.z = z;
			u.chunk_x = floor((float)u.x/16.0f);
			u.chunk_z = floor((float)u.z/16.0f);
			u.yaw = yaw;
			u.pitch = pitch;
			if (flags == 0x01)
				u.on_ground = true;

			auto update_player_position = std::make_tuple(minecraft::varint(fd), (short)(u.x * 4096 - u.prev_x * 4096),
										(short)(u.y * 4096 - u.prev_y * 4096), (short)(u.z * 4096 - u.prev_z * 4096),
										(char)((u.yaw/360) * 256), (char)((u.pitch/360) * 256), u.on_ground);
			send_all_except_user(update_player_position, u, 0x2F, sv, users);

			auto update_head = std::make_tuple(minecraft::varint(fd), (char)((u.yaw/360) * 256));
			send_all_except_user(update_head, u, 0x4C, sv, users);
			stream_world(u, sv);
		}
	);

	packet_definitions_play[0x1F] = std::make_unique<packet<float, float, char>>(
		"Update rotation",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, float &yaw, float &pitch, char &flags)
		{
			user &u = users.find(fd)->second;
			u.yaw = yaw;
			u.pitch = pitch;
			if (flags == 0x01)
				u.on_ground = true;
			auto update_head = std::make_tuple(minecraft::varint(fd), (char)((u.yaw/360) * 256));
			send_all_except_user(update_head, u, 0x4C, sv, users);
		}
	);

	packet_definitions_play[0x20] = std::make_unique<packet<char>>(
		"Update flags",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, char &flags)
		{
			user &u = users.find(fd)->second;
			if (flags == 0x01)
				u.on_ground = true;
		}
	);


	packet_definitions_play[0x28] = std::make_unique<packet<minecraft::varint, int64_t, char, minecraft::varint>>(
		"Player action",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::varint &status, int64_t &location, char &face, minecraft::varint &sequence)
		{
			user &u = users.find(fd)->second;
			long pos = location;
			int x = pos >> 38;
			int y = pos << 52 >> 52;
			int z = pos << 26 >> 38;
			log(std::format("Setting block at x: {} y: {} z: {}", x, y, z), LOG_LEVEL::NORMAL);
			auto ret = w.set_block(x, y, z, w.get_block("minecraft:air", {}));
			if (!ret)
				log("Block placement failed", LOG_LEVEL::ERROR);

			auto block_update = std::make_tuple((int64_t)((((x & (unsigned long)0x3FFFFFF) << 38) | ((z & (unsigned long)0x3FFFFFF) << 12) | (y & (unsigned long)0xFFF))), minecraft::varint(0));
			send_render_distance(block_update, 0x08, sv, u.x, u.z, users);
			auto awknowledge_block = std::make_tuple(minecraft::varint(sequence));
			sv.send_packet(awknowledge_block, fd, 0x04);
		}
	);


	packet_definitions_play[0x34] = std::make_unique<packet<short>>(
		"Set held item",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, short &slot)
		{
			user &u = users.find(fd)->second;
			u.held_item = slot;

			auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)0, minecraft::varint(1),
											minecraft::varint(u.inventory[u.held_item + 36]), minecraft::varint(0),
											minecraft::varint(0));
			send_all_except_user(set_equipment, u, 0x5F, sv, users);
		}
	);


	packet_definitions_play[0x37] = std::make_unique<packet<short, minecraft::varint, minecraft::varint>>(
		"Set creative slot",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, short &slot, minecraft::varint &item_count, minecraft::varint &item_id)
		{
			user &u = users.find(fd)->second;
			u.inventory[slot] = item_id.num;
			if (slot == 45)
			{
				auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)1, minecraft::varint(1),
							minecraft::varint(u.inventory[45]), minecraft::varint(0),
							minecraft::varint(0));
				send_all_except_user(set_equipment, u, 0x5F, sv, users);
			}
			if (slot == u.held_item + 36)
			{
				auto set_equipment = std::make_tuple(minecraft::varint(fd), (char)0, minecraft::varint(1),
							minecraft::varint(u.inventory[u.held_item + 36]), minecraft::varint(0),
							minecraft::varint(0));
				send_all_except_user(set_equipment, u, 0x5F, sv, users);
			}
		}
	);


	packet_definitions_play[0x3C] = std::make_unique<packet<minecraft::varint>>(
		"Swing arm",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::varint &hand)
		{
			user &u = users.find(fd)->second;
			unsigned char anim_id = 0;
			if (hand.num == 1)
				anim_id = 3;
			auto entity_animation = std::make_tuple(minecraft::varint(fd), anim_id);
			send_all_except_user(entity_animation, u, 0x02, sv, users);
		}
	);


	packet_definitions_play[0x3F] = std::make_unique<packet<minecraft::varint, int64_t, minecraft::varint, float, float, float, bool, bool, minecraft::varint>>(
		"Use item on",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, minecraft::varint &hand, int64_t &location, minecraft::varint &face, float &cursor_x, float &cursor_y, float &cursor_z, bool &inside_block, bool &world_border, minecraft::varint &sequence)
		{
			user &u = users.find(fd)->second;
			long pos = location;
			int x = pos >> 38;
			int y = pos << 52 >> 52;
			int z = pos << 26 >> 38;
			log(std::format("Face {}", face.num), LOG_LEVEL::NORMAL);
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
				return;

			auto props = w.get_block_properties(items[u.inventory[u.held_item + 36]]);
			std::map<std::string, json_value> properties;
			if (props.type == TYPE_JSON::OBJECT)
			{
				for (auto &[property, value]: props.get<json_object>())
				{
					if (property == "axis")
					{
						if (face.num == 0 || face.num == 1)
							properties.insert({"axis", json_value("y")});
						else if (face.num == 2 || face.num == 3)
							properties.insert({"axis", json_value("z")});
						else if (face.num == 4 || face.num == 5)
							properties.insert({"axis", json_value("x")});
					}
					if (property == "waterlogged")
					{
						properties.insert({"waterlogged", json_value(false)});
					}
				}
			}
			log(std::format("Setting block at x: {} y: {} z: {}", x, y, z), LOG_LEVEL::NORMAL);
			std::uint64_t id = w.get_block(items[u.inventory[u.held_item + 36]], properties);
			auto ret = w.set_block(x, y, z, w.get_block(items[u.inventory[u.held_item + 36]], properties));
			if (!ret)
				log("Block placement failed", LOG_LEVEL::ERROR);
			auto block_update = std::make_tuple((int64_t)((((x & (unsigned long)0x3FFFFFF) << 38) | ((z & (unsigned long)0x3FFFFFF) << 12) | (y & (unsigned long)0xFFF))), minecraft::varint(id));
			send_render_distance(block_update, 0x08, sv, u.x, u.z, users);
			auto awknowledge_block = std::make_tuple(sequence);
			sv.send_packet(awknowledge_block, fd, 0x04);
			log(std::format("Block placed is {}", items[u.inventory[u.held_item + 36]]), LOG_LEVEL::NORMAL);
			log(std::format("Id is {}", id), LOG_LEVEL::NORMAL);
		}
	);
}