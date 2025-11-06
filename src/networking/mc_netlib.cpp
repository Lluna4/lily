#include "mc_netlib.h"

void server::disconnect_client(int remove_fd)
{
	packets.emplace_back(-1, remove_fd);
	std::lock_guard lock(connections_mut);
	connections.erase(std::remove(connections.begin(), connections.end(), remove_fd), connections.end());
	if (remove_from_epoll(epfd, remove_fd) == -1)
	{
		log(std::format("Removing from epoll failed {}", strerror(errno)), LOG_LEVEL::ERROR);
	}
	close(remove_fd);
}

std::vector<netlib::packet> server::get_packets()
{
	std::lock_guard lock(mut);
	std::vector<netlib::packet> ret = std::move(packets);
	packets.clear();
	return ret;
}


void server::recv_thread()
{
	#if defined(__APPLE__) || defined(__FreeBSD__)
	struct kevent events[1024];
	struct timespec timeout;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 10000000;
	#elif defined(__linux__)
	epoll_event events[1024];
	#endif

	int ev = 0;
	while (threads == true)
	{
		#if defined(__APPLE__) || defined(__FreeBSD__)
		ev = kevent(epfd, NULL, 0, events, 1024, &timeout);
		#elif defined(__linux__)
		ev = epoll_wait(epfd, events, 1024, 10);
		#endif

		for (int i = 0; i < ev; i++)
		{
			#if defined(__APPLE__) || defined(__FreeBSD__)
			int current_fd = events[i].ident;
			#elif defined(__linux__)
			int current_fd = events[i].data.fd;
			#endif

			if (current_fd == fd)
			{
				int new_client = accept(fd, nullptr, nullptr);
				add_to_epoll(epfd, new_client);
				std::unique_lock lock(connections_mut);
				connections.push_back(new_client);
				lock.unlock();
				log("A client connected!", LOG_LEVEL::NORMAL);
				continue;
			}
			if (std::find(connections.begin(), connections.end(), current_fd) == connections.end())
				continue;
			netlib::packet dummy_pkt(0);
			dummy_pkt.data.allocate(5); //max for 1 varint
			int ret = recv(current_fd, dummy_pkt.data.data, 5, MSG_PEEK);
			if (ret == -1 || ret == 0)
			{
				disconnect_client(current_fd);
				continue;
			}
			minecraft::varint size = minecraft::read_varint(dummy_pkt.data.data);
			if (size.num > 60000)
			{
				disconnect_client(current_fd);
				continue;
			}
			dummy_pkt.data.size = 0;
			unsigned long already_read = 0;
			unsigned long total_to_read = size.num + size.size;
			dummy_pkt.data.allocate(total_to_read);
			bool cont = false;
			while (already_read < total_to_read)
			{
				int r = recv(current_fd, &dummy_pkt.data.data[already_read], total_to_read - already_read, 0);
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
			dummy_pkt.data.size = total_to_read;
			//std::println("Read a packet with size {}", total_to_read);
			std::tuple<minecraft::varint, minecraft::varint> head;

			head = netlib::read_packet(head, dummy_pkt);
			unsigned long header_size = (std::get<0>(head).size + std::get<1>(head).size);
			std::lock_guard lock(mut);
			packets.emplace_back(std::get<0>(head).num, std::get<1>(head).num, header_size, std::move(dummy_pkt.data), current_fd);
		}
	}
}

void server::send_thread()
{
	while (threads == true)
	{
		std::unique_lock lock(send_mut);
		notify_send.wait_for(lock, std::chrono::milliseconds(10));
		std::vector<netlib::packet> s_packets = std::move(send_packets);
		send_packets.clear();
		lock.unlock();
		for (auto &pkt: s_packets)
		{
			if (std::find(connections.begin(), connections.end(), pkt.fd) == connections.end())
				continue;
			ssize_t ret = send(pkt.fd, pkt.data.data, pkt.data.size, 0);
			if (ret == 0 || ret == -1)
			{
				disconnect_client(pkt.fd);
				break;
			}
			//log(std::format("Sent {}B", ret), LOG_LEVEL::NORMAL);
		}
		s_packets.clear();
	}
}

void server::clear_packets()
{
	std::lock_guard<std::mutex> lock(mut);
	packets.clear();
}

std::expected<bool, server_error> server::open_server(const char *ip, unsigned short port)
{
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		log(std::format("Failed to create socket {}", strerror(errno)), LOG_LEVEL::ERROR);
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
		log(std::format("Bind failed! {}", strerror(errno)), LOG_LEVEL::ERROR);
		return std::unexpected(server_error::BIND_ERROR);
	}

	if (listen(fd, 1024) == -1)
	{
		close(fd);
		log(std::format("Listen failed! {}", strerror(errno)), LOG_LEVEL::ERROR);
		return std::unexpected(server_error::LISTEN_ERROR);
	}
	#if defined(__APPLE__) || defined(__FreeBSD__)
	epfd = kqueue();
	#elif defined(__linux__)
	epfd = epoll_create1(0);
	#endif
	add_to_epoll(epfd, fd);
	threads = true;
	recv_th = std::thread([this]() {this->recv_thread();});
	log("Started receiving thread!", LOG_LEVEL::NORMAL);
	send_th = std::thread([this]() {this->send_thread();});
	log("Started send thread!", LOG_LEVEL::NORMAL);
	return true;
}
