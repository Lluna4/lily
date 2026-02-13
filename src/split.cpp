#include "split.h"
std::vector<std::string> split_str(std::string str, char delim)
{
	std::vector<std::string> ret;
	size_t pos = 0;
	while ((pos = str.find(delim)) != std::string::npos)
	{
		std::string token = str.substr(0, pos);
		ret.push_back(token);
		str.erase(0, pos + 1);
	}
	ret.push_back(str);

	return ret;
}