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


void send_registry(int fd)
{
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
					netlib::send_packet(register_ent, fd, 0x07);
					//std::println("Sending file {}", e.path().filename().string());
				}
				continue;
			}
			if (!ent.is_regular_file() || ent.path().filename().string().starts_with("."))
				continue;
			auto register_ent = std::make_tuple(std::format("minecraft:{}",entry.path().filename().string()),
									minecraft::varint(1), std::format("minecraft:{}", ent.path().stem().string()), false);
			netlib::send_packet(register_ent, fd, 0x07);
			//std::println("Sending file {}", ent.path().filename().string());
		}
	}
}
