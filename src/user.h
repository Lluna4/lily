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

class user
{
	public:
		user(int sock)
			:fd(sock)
		{
			state = STATE::HANDSHAKE;
			x = 0.0f, y = 65.0f, z = 0.0f;
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

		STATE state;
		int fd;
		double x, y, z;
		float yaw, pitch;
		double prev_x, prev_y, prev_z;
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

