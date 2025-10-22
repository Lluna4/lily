#pragma once

#include "log.h"
#include "block_registry_processing.h"
#include "json_reader.h"
#include <map>


struct block
{
    block(json_value prop)
        :propierties(std::move(prop))
    {
        json_value ret = propierties.get<json_object>()["states"];
        for (auto &state: ret.get<json_array>())
        {
            if (state.get<json_object>().contains("default"))
                actual_id = state.get<json_object>()["id"].get<long>();
        }
    }
    json_value propierties;
    std::map<std::string, std::string> added_propierties;
    long actual_id;
    void add_propierty(std::string propierty, std::string value);
    json_value get_available_propierties();
};

struct block_index
{
    block_index(std::string path)
    {
        blocks = process_block_registry(path);
    }
    json_value blocks;

    block get_block(std::string name);
};
