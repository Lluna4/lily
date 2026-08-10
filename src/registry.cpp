#include "registry.h"
#include "deserialize.h"
#include "log.h"
#include "mc_types.h"
#include <cmath>

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

struct registry_entry
{
	std::string reg_id;
	minecraft::varint entries_num;
	std::string entry_id;
	bool nbt;
};


int send_registry(int fd, server &sv)
{
	int a = 0;
	bool stop_a = false;
	for (const auto & entry : std::filesystem::directory_iterator("../generated/data/minecraft"))
	{
		if (entry.path().filename().string().starts_with("."))
			continue;
		if (entry.path().filename().string().starts_with("enchantment"))
			continue;
		if (entry.path().filename().string().starts_with("datapacks"))
			continue;
		if (entry.path().filename().string().starts_with("dialog"))
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
					registry_entry register_ent = {.reg_id = std::format("minecraft:{}/{}", entry.path().filename().string(), ent.path().filename().string()),
													.entries_num = minecraft::varint(1), .entry_id = std::format("minecraft:{}",e.path().stem().string()),.nbt = false};
					send_packet_compressed(fd, 0x7, register_ent, sv);
				}
				continue;
			}
			if (!ent.is_regular_file() || ent.path().filename().string().starts_with("."))
				continue;
			/*if (ent.path().stem() == "chat")
			{
				auto register_ent = std::make_tuple(std::format("minecraft:{}",entry.path().filename().string()),
						minecraft::varint(1), std::format("minecraft:{}", ent.path().stem().string()), true,
						(char)0x0a, (char)0x0a, minecraft::short_string("chat"),minecraft::string_tag("chat.type.text", "translation_key"),
						(char)0x09, minecraft::short_string("parameters"), (char)0x08, (int)2, minecraft::nameless_string_tag("sender"), minecraft::nameless_string_tag("content"), (char)0x00,
						(char)0x0a, minecraft::short_string("narration"),minecraft::string_tag("chat.type.text.narrate", "translation_key"),
						(char)0x09, minecraft::short_string("parameters"), (char)0x08, (int)2, minecraft::nameless_string_tag("sender"), minecraft::nameless_string_tag("content"), (char)0x00, (char)0x00);
				sv.send_packet(register_ent, fd, 0x07);
				log(std::format("Chat sent at num {}", a), LOG_LEVEL::NORMAL);
				stop_a = true;
			}*/
			else
			{
				registry_entry register_ent = {.reg_id = std::format("minecraft:{}",entry.path().filename().string()),
												.entries_num = minecraft::varint(1), .entry_id = std::format("minecraft:{}", ent.path().stem().string()),.nbt = false};
				send_packet_compressed(fd, 0x7, register_ent, sv);
			}
			if (entry.path().stem() == "chat_type")
			{
				log(std::format("{}", ent.path().stem().string()), LOG_LEVEL::NORMAL);
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

int get_registry(std::vector<std::string> &biomes, std::vector<std::string> &dimensions)
{
	int a = 0;
	bool stop_a = false;
	for (const auto & entry : std::filesystem::directory_iterator("../generated/data/minecraft"))
	{
		if (entry.path().filename().string().starts_with("."))
			continue;
		if (entry.path().filename().string().starts_with("enchantment"))
			continue;
		if (entry.path().filename().string().starts_with("datapacks"))
			continue;
		if (entry.path().filename().string().starts_with("dialog"))
			continue;
		if (entry.path().filename().string() == "dimension_type")
		{
			for (const auto & ent : std::filesystem::directory_iterator(entry.path()))
			{
				dimensions.push_back(ent.path().stem().string());
			}
		}
		for (const auto & ent : std::filesystem::directory_iterator(entry.path()))
		{
			if (ent.is_directory())
			{
				for (const auto & e : std::filesystem::directory_iterator(ent.path()))
				{
					if (!e.is_regular_file() || e.path().filename().string().starts_with("."))
						continue;
					
					if (ent.path().stem().string() == "biome")
						biomes.push_back(e.path().filename().stem().string());
				}
				continue;
			}
			if (!ent.is_regular_file() || ent.path().filename().string().starts_with("."))
				continue;
			if (ent.path().stem() == "chat")
			{
				log(std::format("Chat sent at num {}", a), LOG_LEVEL::NORMAL);
				stop_a = true;
			}
			if (entry.path().stem().string() == "chat_type")
			{
				log(std::format("{}", ent.path().stem().string()), LOG_LEVEL::NORMAL);
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
