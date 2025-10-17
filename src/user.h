#pragma once
#include <vector>
#include <string>
#include "mc_types.h"

enum class STATE
{
	HANDSHAKE,
	STATUS,
	LOGIN,
	CONFIGURATION,
	PLAY
};

struct position
{
	position(double x, double y, double z)
		:x(x), y(y), z(z)
	{}
	double x, y, z;
};

class user
{
	public:
		user(int sock)
			:fd(sock)
		{
			state = STATE::HANDSHAKE;
			x = 0.0f, y = 65.0f, z = 0.0f;
			chunk_x = 0, chunk_z = 0;
			yaw = 0.0f; pitch = 0.0f;
			on_ground = false;
			inventory.resize(46);
			name = "Placeholder";
			pronouns = "any";
			locale = "en_us";
			view_distance = 12;
			uuid.generate(name);
			ticks_to_keepalive = 500;
			sent = false;
			held_item = 0;
		}

		bool check_collision_block(position block);
		STATE state;
		int fd;
		double x, y, z;
		int chunk_x;
		int chunk_z;
		float yaw, pitch;
		double prev_x, prev_y, prev_z;
		int prev_chunk_x;
		int prev_chunk_z;
		bool on_ground;
		std::vector<unsigned long> inventory;
		std::string name;
		std::string pronouns;
		std::string locale;
		char view_distance;
		minecraft::uuid uuid;
		int ticks_to_keepalive;
		bool sent;
		short held_item;
};

inline bool user::check_collision_block(position block)
{
	return (x - 0.3 < block.x + 1 && x + 0.3 > block.x &&
	y < block.y + 1 && y + 1.8 > block.y &&
	z - 0.3 < block.z + 1 && z + 0.3 > block.z);
}

