#include "comp_time_write.h"



template <int size, typename T>
void write_array(char *v, T value)
{
    std::memcpy(v, value, size);
}
