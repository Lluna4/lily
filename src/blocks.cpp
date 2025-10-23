#include "blocks.h"
#include "json_reader.h"

void block::add_propierty(std::string propierty, json_value name)
{
    added_propierties.emplace(propierty, name);

    json_value ret = propierties.get<json_object>()["states"];
    for (auto &state: ret.get<json_array>())
    {
        bool contains_everything = true;
        for (auto &[prop, value]: added_propierties)
        {
            if (state.get<json_object>()["properties"].get<json_object>().contains(prop))
            {
                if (state.get<json_object>()["properties"].get<json_object>()[prop].get_type() == value.get_type())
                {
                    if (value.get_type() == TYPE_JSON::STRING)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<std::string>() != value.get<std::string>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                    else if (value.type == TYPE_JSON::BOOL)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<bool>() != value.get<bool>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                    else if (value.type == TYPE_JSON::NUMBER)
                    {
                        if (state.get<json_object>()["properties"].get<json_object>()[prop].get<long>() != value.get<long>())
                        {
                            contains_everything = false;
                            break;
                        }
                    }
                }
                else
                {
                    contains_everything = false;
                    break;
                }
            }
            else
            {
                contains_everything = false;
                break;
            }
        }

        if (contains_everything)
        {
            actual_id = state.get<json_object>()["id"].get<long>();
            break;
        }
    }
}

json_value block::get_available_propierties()
{
    return propierties.get<json_object>()["properties"];
}

block block_index::get_block(std::string name)
{
    json_object b = blocks.get<json_object>()[name].get<json_object>();

    json_value val(b);
    block ret(val);

    return ret;
}
