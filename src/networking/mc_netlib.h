#pragma once
#include <vector>
#include <thread>
#include <expected>
#include <mutex>
#include <format>
#include <tuple>
#include <string>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <print>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <map>
#include "epoll_utils.h"
#include "buffer.h"
#include "comp_time_read.h"
#include "comp_time_write.h"
#include "../mc_types.h"

enum class server_error
{
	BIND_ERROR,
	LISTEN_ERROR,
	MEMORY_ERROR,
	SOCKET_ERROR,
	NON_VALID_PORT,
	NON_VALID_IP
};

template <>
struct std::formatter<server_error>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const server_error& id, std::format_context& ctx) const
	{
		if (id == server_error::BIND_ERROR)
			return std::format_to(ctx.out(), "{}", "Bind failed");
		if (id == server_error::LISTEN_ERROR)
			return std::format_to(ctx.out(), "{}", "Listen failed");
		if (id == server_error::MEMORY_ERROR)
			return std::format_to(ctx.out(), "{}", "Allocating memory failed");
		if (id == server_error::SOCKET_ERROR)
			return std::format_to(ctx.out(), "{}", "Socket failed");
		if (id == server_error::NON_VALID_IP)
			return std::format_to(ctx.out(), "{}", "The provided ip wasn't valid");
		if (id == server_error::NON_VALID_PORT)
			return std::format_to(ctx.out(), "{}", "The provided port wasn't valid");
		return std::format_to(ctx.out(), "{}", "Unknown error");
	}
};


class server
{
	public:
		server()
		{
			fd = 0;
		}
		~server()
		{
			if (threads == true)
			{
				threads = false;
				recv_th.join();
			}
		}
		server(const server&) = delete;
		//std::map<int, netlib::packet> get_packets();
		void clear_packets();
		std::expected<bool, server_error> open_server(const char *ip, unsigned short port);
		void disconnect_client(int fd);
		std::vector<netlib::packet> packets;
		std::mutex mut;


	private:
		void recv_thread();
		std::vector<int> connections;
		int fd;
		int epfd;
		std::thread recv_th;
		std::atomic_bool threads;

};