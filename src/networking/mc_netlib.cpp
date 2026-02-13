#include "mc_netlib.h"
#include "comp_time_read.h"
#include <cstddef>
#include <stdexcept>
#include <unordered_map>

std::unordered_map<int, std::coroutine_handle<>> clients_to_read;
std::unordered_map<int, std::coroutine_handle<>> clients_to_send;

struct await_socket
{
	int fd;

	bool await_ready() { return false;};
	void await_suspend(std::coroutine_handle<> h)
	{
		clients_to_read.insert({fd, h});
	}
	void await_resume() {};
};

struct read_return
{
	struct promise_type 
	{
        read_return get_return_object() 
		{ 
            return read_return{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() {return std::suspend_never{};}
        auto final_suspend() noexcept {return std::suspend_never{};}

        void unhandled_exception() {std::runtime_error("Read failed");}
        void return_void() {}
    };

	std::coroutine_handle<promise_type> handle;
};

struct await_socket_send
{
	int fd;

	bool await_ready() { return false;};
	void await_suspend(std::coroutine_handle<> h)
	{
		clients_to_send.insert({fd, h});
	}
	void await_resume() {};
};

struct send_return
{
	struct promise_type 
	{
        send_return get_return_object() 
		{ 
            return send_return{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() {return std::suspend_never{};}
        auto final_suspend() noexcept {return std::suspend_never{};}

        void unhandled_exception() {std::runtime_error("Send failed");}
        void return_void() {}
    };

	std::coroutine_handle<promise_type> handle;
};

read_return read_coroutine(int fd, std::vector<netlib::packet> &packets, std::mutex &mut, std::function<void(int)> disconnect_client)
{
	netlib::packet dummy_pkt(0);
	dummy_pkt.data.allocate(5); //max for 1 varint
	int ret = recv(fd, dummy_pkt.data.data, 5, MSG_PEEK);
	if (ret == -1 || ret == 0)
	{
		disconnect_client(fd);
		co_return;
	}
	minecraft::varint size = minecraft::read_varint(dummy_pkt.data.data);
	if (size.num > 60000)
	{
		disconnect_client(fd);
		co_return;
	}
	dummy_pkt.data.size = 0;
	unsigned long already_read = 0;
	unsigned long total_to_read = size.num + size.size;
	dummy_pkt.data.allocate(total_to_read);
	bool cont = false;
	while (already_read < total_to_read)
	{
		co_await await_socket(fd);
		int r = recv(fd, &dummy_pkt.data.data[already_read], total_to_read - already_read, 0);
		if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			continue;
		}
		else if (r == -1 || r == 0)
		{
			disconnect_client(fd);
			cont = true;
			break;
		}
		already_read += r;
	}
	if (cont == true)
		co_return;
	dummy_pkt.data.size = total_to_read;
	std::tuple<minecraft::varint, minecraft::varint> head;

	netlib::read_packet(head, dummy_pkt);
	unsigned long header_size = (std::get<0>(head).size + std::get<1>(head).size);
	std::lock_guard lock(mut);
	packets.emplace_back(std::get<0>(head).num, std::get<1>(head).num, header_size, std::move(dummy_pkt.data), fd);
	
	co_return;
}

send_return send_coroutine(int fd, netlib::packet pkt, std::function<void(int)> disconnect_client)
{
	int already_read = 0;
	while (already_read < pkt.data.size)
	{
		ssize_t ret = send(pkt.fd, &pkt.data.data[already_read], pkt.data.size - already_read, 0);
		if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			co_await await_socket_send(fd);
		}
		else if (ret == 0 || ret == -1 || pkt.dc == true)
		{
			disconnect_client(pkt.fd);
			co_return;
		}
		already_read += ret;
	}
	//log(std::format("Sent {}B", already_read), LOG_LEVEL::NORMAL);
	co_return;
}

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
			#if defined(__APPLE__) || defined(__FreeBSD__)
			if (events[i].fflags & EVFILT_WRITE)
			#elif defined(__linux__)
			if (events[i].events & EPOLLOUT)
			#endif
			{
				if (clients_to_send.find(current_fd) != clients_to_send.end())
				{
					auto handle = clients_to_send.find(current_fd)->second;
					clients_to_send.erase(current_fd);
					handle.resume();
					continue;
				}
			}
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
			if (clients_to_read.find(current_fd) != clients_to_read.end())
			{
				auto handle = clients_to_read.find(current_fd)->second;
				clients_to_read.erase(current_fd);
				handle.resume();
			}
			else 
			{
				std::function<void(int)> func = [this](int fd)
				{
					disconnect_client(fd);
				};
				read_coroutine(current_fd, packets, mut, func);
			}
		}
	}
}

void server::send_thread()
{
	while (threads == true)
	{
		std::unique_lock lock(send_mut);
		notify_send.wait_for(lock, std::chrono::milliseconds(5));
		std::vector<netlib::packet> s_packets = std::move(send_packets);
		send_packets.clear();
		lock.unlock();
		for (auto &pkt: s_packets)
		{
			if (std::find(connections.begin(), connections.end(), pkt.fd) == connections.end())
				continue;
			std::function<void(int)> func = [this](int fd)
			{
				disconnect_client(fd);
			};
			send_coroutine(pkt.fd, std::move(pkt), func);
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
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
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
