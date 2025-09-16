#include <print>
#include "mc_netlib.h"

int main()
{
	server sv{};
	auto ret = sv.open_server("0.0.0.0", 25565);
	if (!ret)
	{
		std::println("Opening server failed {}", ret.error());
		return -1;
	}
	return 0;
}
