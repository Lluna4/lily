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
		}
	}
}

int main()
{
	server sv{};
	auto ret = sv.open_server("0.0.0.0", 25565);
	if (!ret)
	{
		std::println("Opening server failed {}", ret.error());
		return -1;
	}

	while (true)
	{
		auto pkts = sv.get_packets();
		sv.clear_packets();
		for (auto &pkt: pkts)
		{
			if (pkt.second.id == -1)
			{
				std::println("Client disconnected");
				continue;
			}
			std::println("Got a packet from fd {} with id {} and size {}", pkt.first, pkt.second.id, pkt.second.size);
			execute_packet(pkt.first, pkt.second, sv);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	return 0;
}
