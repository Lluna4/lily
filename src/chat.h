#pragma once
#include <print>
#include <tuple>
#include "user.h"
#include "networking/mc_netlib.h"

void send_chat(std::string contents, std::string sender, int chat_id, server &sv, std::map<int, user> &users);
void send_system_chat(std::string contents, std::map<int, user> &users, server &sv);
