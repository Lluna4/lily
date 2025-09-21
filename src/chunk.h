#pragma once
#include <vector>

struct section
{
	std::vector<u_int64_t> palette;
	std::vector<int8_t> blocks;
	short non_air_blocks;
	int y;
};

struct chunk
{
	chunk(int x, int y)
		:x(x), y(y)
	{
		for (int i = 0; i < 24; i++)
		{
			section sec;
			if (i < 8)
			{
				sec.blocks.resize(4096);
				sec.non_air_blocks = 4096;
				sec.palette.push_back(9);
				sec.palette.push_back(0);
			}
			else
			{
				sec.blocks.resize(4092);
				sec.non_air_blocks = 0;
				sec.palette.push_back(0);
			}
			sections.push_back(sec);
		}
	}
	std::vector<section> sections;
	int x, y;
};