#pragma once
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#define BUFFER_SIZE 1024

template<typename T>
struct buffer
{
	buffer()
	{
		data = nullptr;
		extra = nullptr;
		size = 0;
		allocated = 0;
		allocations = 0;
		parse_consumed_size = 0;
	}
	T *data;
	T *extra;
	size_t size;
	size_t parse_consumed_size;
	size_t allocated;
	int allocations;

	void write(T *data_in, size_t data_size);
	void remove(int offset, int remove_size);
	void allocate(size_t s);

	buffer(const buffer&) = delete;
	~buffer()
	{
		if (allocated > 0)
			free(data);
	}
};

template<typename T>
void buffer<T>::write(T *data_in, size_t data_size)
{
	if (data_size > allocated - size)
	{
		allocated += (size + data_size) + BUFFER_SIZE;
		data = (T *)realloc(data, allocated);
		allocations++;
		if (!data)
		{
			throw std::runtime_error("Allocation failed");
		}
	}
	std::memcpy(&data[size], data_in, data_size);
	size += data_size;
}

template<typename T>
void buffer<T>::remove(int offset, int remove_size)
{
	if (offset + remove_size > size)
		return ;
	T *new_data = (T *)calloc(allocated, sizeof(T));
	if (!new_data)
	{
		throw std::runtime_error("Allocation failed");
	}
	if (offset > 0)
	{
		std::memcpy(new_data, data, offset);
	}
	int new_size = size - remove_size;
	std::memcpy(&new_data[offset], &data[offset + remove_size], new_size - offset);
	free(data);
	data = new_data;
	size = new_size;
	allocations++;
}

template<typename T>
void buffer<T>::allocate(size_t s)
{
	data = (T *)realloc(data, allocated + s);
	if (!data)
	{
		throw std::runtime_error("Allocation failed");
	}
	allocations++;
	allocated += s;
}
