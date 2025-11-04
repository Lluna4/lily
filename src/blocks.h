#pragma once

#include "log.h"
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
    std::map<std::string, json_value> added_propierties;
    long actual_id;
    void add_propierty(std::string propierty, json_value name);
    json_value get_available_propierties();
};
