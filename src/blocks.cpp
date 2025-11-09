#include "blocks.h"
#include "json_reader.h"

json_value block::get_available_propierties()
{
    return propierties.get<json_object>()["properties"];
}
