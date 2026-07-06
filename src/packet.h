#pragma once
#include "../netlib_/src/networking.h"
#include "registry.h"
#include "deserialize.h"
#include "mc_types.h"
#include "user.h"
#include "log.h"
#include <bitset>
#include <cstddef>
#include <memory>
#include <sys/socket.h>
#include <variant>


int chat_id = 0;

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
}

