# LILY

## About

Lily is a 1.21.8 minecraft server in c++, mainly for learning purposes, even though its being built to be as production ready as possible

All code is from scratch and made by me, except the [perlin noise library](https://github.com/Reputeless/PerlinNoise)

It features an async networking system that is based on epoll and kqueue for reads

Also it has a packet serialization/deserialization system that works with template metaprogramming so its as fast as possible

And also the memory consumption is pretty low, at 40 MB with 12 chunk radius spawn generated

For now you can place blocks in a randomly generated infinite world in creative, some blocks dont place properly yet like doors


## Building

Lily supports linux, freebsd and macos and its built with cmake `mkdir build && cd build` to create the build directory and then configure and build with `cmake -DCMAKE_BUILD_TYPE=Release ..` and `cmake --build .`

note that the server does require data from the vanilla server to function, you can extract the data from it with this command `java -DbundlerMainClass="net.minecraft.data.Main" -jar server.jar --all` and then drop the `generated` folder into this project's root


## Credits


Uses a [perlin noise library](https://github.com/Reputeless/PerlinNoise) from [Reputless](https://github.com/Reputeless)
