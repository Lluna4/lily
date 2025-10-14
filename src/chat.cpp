#include "chat.h"

void send_chat(std::string contents, std::string sender, int chat_id, server &sv, std::map<int, user> &users)
{
	auto message = std::make_tuple((char)0x0a, minecraft::string_tag(contents, "text"), (char)0x00,
							minecraft::varint(chat_id + 1), (char)0x0a, minecraft::string_tag(sender, "text"), (char)0x00, false);
	for (auto &user: users)
	{
		if (user.second.state == STATE::PLAY)
			sv.send_packet(message, user.first, 0x1D);
	}
}

void send_system_chat(std::string contents, std::map<int, user> &users, server &sv)
{
	auto message = std::make_tuple((char)0x0a, minecraft::string_tag(contents, "text"), (char)0x00, false);
	for (auto &user: users)
	{
		if (user.second.state == STATE::PLAY)
			sv.send_packet(message, user.first, 0x72);
	}
}
