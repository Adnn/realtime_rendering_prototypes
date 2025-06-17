# Chapter 11 - Global Illumination

## Voxels & Voxel Cone Tracing Global Illuminatin (VXGI)

Implements voxelization pipeline, accumulations in voxel space,
and cone tracing to achieve different GI effects.

### DDS textures

The loader will attempt to load a texture with the `.dds` extension first.
It also outputs a JSON file listing all textures by category, under `sorted_textures.json`.

This file can be used to convert textures to DDS format, using the `convert_dds.py` script.

## References (number is the id from the book)

* 306: Crassin, Cyril, and Simon Green, “Octree-Based Sparse Voxelization Using the GPU Hardware
Rasterizer,”
  https://research.nvidia.com/labs/rtr/publication/crassin2012voxelization/
  * Detailed description of a method to voxelize mesh models, in regular grids or SVO.
* John Amanatides, Andrew Woo "A Fast Voxel Traversal Algorithm for Ray Tracing"
  http://www.cse.yorku.ca/~amana/research/grid.pdf
  * Fast and simple voxel traversal algorithm (3D DDA). 
	Used to render a dense voxel grid via raytracing.

* Building an Orthonormal Basis, Revisited,
  https://graphics.pixar.com/library/OrthonormalB/paper.pdf
