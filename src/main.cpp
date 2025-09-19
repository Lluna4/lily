#include <print>
#include <chrono>
#include <map>
#include "user.h"
#include "networking/mc_netlib.h"

std::map<int, user> users;

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
				std::println("Received handshake packet with version {} address {} port {} intent {}", std::get<0>(handshake).num, std::get<1>(handshake).data.data, std::get<2>(handshake), std::get<3>(handshake).num);
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
				minecraft::string res;
				minecraft::read_string(packet.data.data + packet.header_size, res);
				u.name = res.data.data;
				u.uuid.generate(u.name);
				auto login_success = std::make_tuple(u.uuid, u.name, minecraft::varint(0));
				netlib::send_packet(login_success, u.fd, 0x02);
				u.state = STATE::CONFIGURATION;
				break;
		}
	}
	else if (u.state == STATE::CONFIGURATION)
	{
		switch (packet.id)
		{
			case 0x00:
				std::tuple<minecraft::string, char, minecraft::varint, bool, unsigned char, minecraft::varint, bool, bool, minecraft::varint> client_info;
				client_info = netlib::read_packet(std::move(client_info), packet);

				std::println("Locale {} View distance {}", std::get<0>(client_info).data.data, (int)std::get<1>(client_info));
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
