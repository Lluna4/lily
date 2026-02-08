#include "chunk_send.h"

std::mutex world_mut;
std::vector<chunk_request> chunks_to_send;
std::condition_variable notify_send;
bool thread = true;

void send_chunks(int fd, std::vector<std::pair<int, int>> &pos)
{
	std::unique_lock lock(world_mut);
	for (auto &p: pos)
	{
		chunks_to_send.emplace_back(p.first, p.second, fd);
	}
	lock.unlock();
	notify_send.notify_all();
}

void world_thread(server &sv, world &w)
{
	while(thread == true)
	{
		std::unique_lock lock(world_mut);
		notify_send.wait_for(lock, std::chrono::milliseconds(5));
        if (chunks_to_send.empty() == false)
        {
            std::vector<chunk_request> chunks = std::move(chunks_to_send);
            chunks_to_send.clear();
            lock.unlock();
            for (auto &ch: chunks)
            {
                chunk &c = w.get_chunk(ch.x, ch.z);
            }
            w.build_trees();
            for (auto &ch: chunks)
            {
                chunk &c = w.get_chunk(ch.x, ch.z);
                auto chunk_data = std::make_tuple(ch.x, ch.z, minecraft::varint(0), std::ref(c), minecraft::varint(0),
                            minecraft::varint(0),minecraft::varint(0),minecraft::varint(0),
                            minecraft::varint(0),minecraft::varint(0), minecraft::varint(0));
                sv.send_packet(chunk_data, ch.fd, 0x27);
            }
            chunks.clear();
        }
	}
}