#include "config.h"

std::map<std::string, std::string> questions_answers = {
{"What ip are you going to bind the server to? (leave blank if unsure)", ""},
{"What port are you going to bind the server to? (default 25565)", ""},
{"What message of the day would you want to use?", ""},
{"What path would you like to use for the server icon? (leave blank if no icon)", ""}};

void generate_config(std::string path)
{
    std::ofstream file(path);
    
    
    std::println("Welcome! We will ask some questions for your first setup of the server");
    
    for (auto question: questions_answers)
    {
        std::println("{}", question.first);
        std::getline(std::cin, question.second);
        if (question.first.contains("ip"))
        {
            std::string ip_conf;
            if (question.second.empty())
                ip_conf = "ip=0.0.0.0\n"; 
            else 
                ip_conf = std::format("ip={}\n", question.second);
            file.write(ip_conf.c_str(), ip_conf.size());
        }
        else if (question.first.contains("port"))
        {
            std::string port_conf;
            if (question.second.empty())
                port_conf = "port=25565\n";
            else 
            {
                if (isnum(port_conf))
                {
                   port_conf = std::format("port={}\n", question.second); 
                }
                else
                    port_conf = "port=25565\n"; 
            }
            file.write(port_conf.c_str(), port_conf.size());
        }
        else if (question.first.contains("message"))
        {
            std::string motd_conf;
            if (question.second.empty())
                motd_conf = "motd=Lily server!\n"; 
            else 
                motd_conf = std::format("motd={}\n", question.second);
            file.write(motd_conf.c_str(), motd_conf.size());
        }
        else if (question.first.contains("icon"))
        {
            std::string icon_conf;
            if (question.second.empty())
                icon_conf = "img-path=ico.png\n"; 
            else 
                icon_conf = std::format("img-path={}\n", question.second);
            file.write(icon_conf.c_str(), icon_conf.size());
        }
    }
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