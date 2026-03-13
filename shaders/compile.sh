rm *.spv
glslc compute.comp
glslc biome.comp -o biome.spv
glslc bush.comp -o bush.spv
glslc surface.comp -o surface.spv
glslc tree.comp -o tree.spv