#include <map>
#include <string>
#include <fstream>
#include <filesystem>
#include "split.h"

void generate_config(std::string path);
std::map<std::string, std::string> load_config(std::string path);
bool isnum(std::string n);
