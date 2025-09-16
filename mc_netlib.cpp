#include "mc_netlib.h"

void server::disconnect_client(int fd)
{
	packets.emplace(std::piecewise_construct, std::forward_as_tuple(fd), std::forward_as_tuple(-1));
	if (remove_from_epoll(epfd, fd) == -1)
	{
		std::println("Removing from epoll failed {}", strerror(errno));
	}
	close(fd);
}


void server::recv_thread()
{
	epoll_event events[1024];
	int ev = 0;
	while (threads == true)
	{
		ev = epoll_wait(epfd, events, 1024, 500);

		for (int i = 0; i < ev; i++)
		{
			int current_fd = events[i].data.fd;

			if (current_fd == fd)
			{
				int new_client = accept(fd, nullptr, nullptr);
				connections.push_back(new_client);
				std::println("A client connected!");
				continue;
			}
			buffer<char> pkt;
			pkt.allocate(5); //max for 1 varint
			int ret = recv(current_fd, pkt.data, 5, 0);
			if (ret == -1 || ret == 0)
			{
				disconnect_client(current_fd);
				continue;
			}
			minecraft::varint size = minecraft::read_varint(pkt.data);

			pkt.allocate(size.num + ret);

			unsigned long already_read = ret;
			bool cont = false;
			while (already_read < size.num)
			{
				int r = recv(current_fd, &pkt.data[already_read], size.num - already_read, 0);
				if (r == -1 || r == 0)
				{
					disconnect_client(current_fd);
					cont = true;
					break;
				}
				already_read += r;
			}
			if (cont == true)
				continue;

			std::tuple<minecraft::varint, minecraft::varint> head;
			head = netlib::read_packet(head, pkt);
			pkt.extra = pkt.data + (std::get<0>(head).size + std::get<1>(head).size);
			auto returned = packets.emplace(std::piecewise_construct, std::forward_as_tuple(current_fd), std::forward_as_tuple(std::get<1>(head).num));
			if (returned.second == true)
			{
				returned.first->second.size = std::get<0>(head).num;
				returned.first->second.data = pkt;
				pkt.data = nullptr;
				pkt.allocated = 0;
			}
		}
	}
}

std::expected<bool, server_error> server::open_server(const char *ip, unsigned short port)
{
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		std::println("Failed to create socket {}", strerror(errno));
		return std::unexpected(server_error::SOCKET_ERROR);
	}

	sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
	int ret = inet_pton(AF_INET, ip, &addr.sin_addr);

	if (ret == 0 || ret == -1)
	{
		close(fd);
		return std::unexpected(server_error::NON_VALID_IP);
	}

	if (bind(fd, (sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		std::println("Bind failed! {}", strerror(errno));
		return std::unexpected(server_error::BIND_ERROR);
	}

	if (listen(fd, 1024) == -1)
	{
		close(fd);
		std::println("Listen failed! {}", strerror(errno));
		return std::unexpected(server_error::LISTEN_ERROR);
	}

	epfd = epoll_create1(0);
	add_to_epoll(epfd, fd);
	recv_th = std::thread([this]() {this->recv_thread();});
	std::println("Started receiving thread!");
	return true;
}
