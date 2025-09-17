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
			x = 0.0f, y = 0.0f, z = 0.0f;
			yaw = 0.0f; pitch = 0.0f;
			on_ground = false;
			inventory.resize(46);
			name = "Placeholder";
			pronouns = "any";
			uuid.generate(name);
		}

		STATE state;
		int fd;
		double x, y, z;
		double yaw, pitch;
		bool on_ground;
		std::vector<unsigned long> inventory;
		std::string name;
		std::string pronouns;
		minecraft::uuid uuid;
};