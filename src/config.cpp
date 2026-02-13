#include "config.h"

void generate_config(std::string path)
{
    std::ofstream file(path);

    std::string config_example = "ip=0.0.0.0\nport=25565\nmotd=Lily server!\nimg-path=ico.png";
    file.write(config_example.c_str(), config_example.size());
    file.close();
}

std::map<std::string, std::string> load_config(std::string path)
{
    std::map<std::string, std::string> ret;
    if (!std::filesystem::exists(path))
		generate_config(path);
    
    std::ifstream file(path);
    
    std::string line;
    while (std::getline(file, line))
    {
        if (line.starts_with('#'))
            continue;

        if (line.contains('='))
        {
            auto l = split_str(line, '=');
            ret.insert({l[0], l[1]});
        }
    }
    file.close();
    return ret;
}

bool isnum(std::string n)
{
    for (auto &c: n)
    {
        if (isdigit(c) == 0)
            return false;
    }
    return true;
}