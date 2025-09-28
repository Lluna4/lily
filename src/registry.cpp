#include "registry.h"

static int count_files(std::string path)
{
	int ret = 0;
	for (const auto & entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_regular_file())
			ret++;
	}
	return ret;
}


int send_registry(int fd, server &sv)
{
	int a = 0;
	bool stop_a = false;
	for (const auto & entry : std::filesystem::directory_iterator("../minecraft/"))
	{
		if (entry.path().filename().string().starts_with("."))
			continue;
		//std::println("Sending folder {}", entry.path().filename().string());
		for (const auto & ent : std::filesystem::directory_iterator(entry.path()))
		{
			if (ent.is_directory())
			{
				for (const auto & e : std::filesystem::directory_iterator(ent.path()))
				{
					if (!e.is_regular_file() || e.path().filename().string().starts_with("."))
						continue;
					auto register_ent = std::make_tuple(std::format("minecraft:{}/{}", entry.path().filename().string(), ent.path().filename().string()),
											minecraft::varint(1), std::format("minecraft:{}",e.path().stem().string()), false);
					sv.send_packet(register_ent, fd, 0x07);
					//std::println("Sending file {}", e.path().filename().string());
				}
				continue;
			}
			if (!ent.is_regular_file() || ent.path().filename().string().starts_with("."))
				continue;
			if (ent.path().stem() == "chat")
			{
				auto register_ent = std::make_tuple(std::format("minecraft:{}",entry.path().filename().string()),
						minecraft::varint(1), std::format("minecraft:{}", ent.path().stem().string()), true,
						(char)0x0a, (char)0x0a, minecraft::short_string("chat"),minecraft::string_tag("chat.type.text", "translation_key"),
						(char)0x09, minecraft::short_string("parameters"), (char)0x08, (int)2, minecraft::nameless_string_tag("sender"), minecraft::nameless_string_tag("content"), (char)0x00,
						(char)0x0a, minecraft::short_string("narration"),minecraft::string_tag("chat.type.text.narrate", "translation_key"),
						(char)0x09, minecraft::short_string("parameters"), (char)0x08, (int)2, minecraft::nameless_string_tag("sender"), minecraft::nameless_string_tag("content"), (char)0x00, (char)0x00);
				sv.send_packet(register_ent, fd, 0x07);
				std::println("Chat sent at num {}", a);
				stop_a = true;
			}
			else
			{
				auto register_ent = std::make_tuple(std::format("minecraft:{}",entry.path().filename().string()),
										minecraft::varint(1), std::format("minecraft:{}", ent.path().stem().string()), false);
				sv.send_packet(register_ent, fd, 0x07);
			}
			if (entry.path().stem() == "chat_type")
			{
				std::println("{}", ent.path().stem().string());
			}
			if (entry.path().stem().string() == "chat_type" && stop_a == false)
			{
				a++;
			}
			//std::println("Sending file {}", ent.path().filename().string());
		}
	}
	return a;
}
