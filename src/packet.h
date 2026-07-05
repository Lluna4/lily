#pragma once
#include "../netlib_/src/networking.h"
#include "deserialize.h"
#include "user.h"
#include "log.h"
#include <bitset>
#include <cstddef>
#include <memory>

struct pkt_header
{
	minecraft::varint size;
	minecraft::varint id;
};

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

static packet generate_packet(int fd, int id, size_t size, char *data)
{
	char dummy[10];
	int id_len = minecraft::write_varint(dummy, 0x00);

	int payload_len = size + id_len;
	int packet_len_varint_size = minecraft::write_varint(dummy, payload_len);

	packet pkt;
	pkt.size = packet_len_varint_size + payload_len;
	pkt.data = std::make_unique<char []>(packet_len_varint_size + payload_len);
	pkt.id = id;
	pkt.fd = fd;
	pkt_header head = {.size = minecraft::varint(payload_len), .id = minecraft::varint(pkt.id)};
	int offset = serialize(head, pkt.data.get());
	memcpy(pkt.data.get() + offset, data, size);

	return std::move(pkt);

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

				char dummy[10];
				int str_len_varint_size = minecraft::write_varint(dummy, resp.data.size());
				std::unique_ptr<char []> dat = std::make_unique<char []>(resp.data.size() + str_len_varint_size);
				
				serialize(resp, dat.get());
				packet pkt = generate_packet(fd, 0x00, resp.data.size() + str_len_varint_size, dat.get());

				sv.send_packet(std::move(pkt), fd, 0x00);
			}
		);

	packet_definitions_status[0x01] = std::make_unique<packet_executer<pong>>(
		"Ping",
		[](server &sv, std::map<int, user> &users, std::vector<int> &disconnected, int fd, pong &data)
		{
			log("Ping!", LOG_LEVEL::NORMAL);
			user &u = users.find(fd)->second;
			pong resp = {.timestamp = data.timestamp};
			std::unique_ptr<char []> dat = std::make_unique<char []>(sizeof(long));
			serialize(resp, dat.get());
			packet pkt = generate_packet(fd, 0x01, sizeof(long), dat.get());
			pkt.dc = true;
			sv.send_packet(std::move(pkt), fd, 0x01);
			//disconnected.push_back(fd);
		}
	);
}

