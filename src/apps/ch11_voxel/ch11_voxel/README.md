# Chapter 11 - Global Illumination

## Voxels & Voxel Cone Tracing Global Illuminatin (VXGI)

Implements a GPU-accelerated voxelization pipeline, accumulations in voxel space,
compute-based mipmap filtering, and cone tracing to achieve different GI effects
(radiance accumulation, AO, visibility).

[![intel_sponza-directional_vxgi](https://adnn.github.io/assets/rtr_prototypes/ch11_voxel/rtr_11-intel_sponza-864.jpg)](https://adnn.github.io/assets/rtr_prototypes/ch11_voxel/rtr_11-intel_sponza-1920.jpg)

[![pica_pica-point_vxgi](https://adnn.github.io/assets/rtr_prototypes/ch11_voxel/rtr_11-pica_pica-864.jpg)](https://adnn.github.io/assets/rtr_prototypes/ch11_voxel/rtr_11-pica_pica-1920.jpg)


### DDS textures

The loader will attempt to load a texture with the `.dds` extension first.
It also outputs a JSON file listing all textures by category, under `sorted_textures.json`.

This file can be used to convert textures to DDS format, using the `convert_dds.py` script.

## References (number is the id from the book)

* 306: Crassin, Cyril, and Simon Green, Octree-Based Sparse Voxelization Using the GPU Hardware
Rasterizer,
  https://research.nvidia.com/labs/rtr/publication/crassin2012voxelization/
  * Detailed description of a method to voxelize mesh models, in regular grids or SVO.
* Finn, Johannes. Evaluation of Performance and Image Quality for Voxel Cone Tracing. n.d.
  https://www.diva-portal.org/smash/get/diva2%3A1637733/FULLTEXT01.pdf
  * Detailed implementation report, proposing to use sparse texture and inject light during main voxelization.
* John Amanatides, Andrew Woo "A Fast Voxel Traversal Algorithm for Ray Tracing"
  http://www.cse.yorku.ca/~amana/research/grid.pdf
  * Fast and simple voxel traversal algorithm (3D DDA).
	Used to render a dense voxel grid via raytracing.
* Building an Orthonormal Basis, Revisited,
  https://graphics.pixar.com/library/OrthonormalB/paper.pdf
* Jose Villegas - VCTRenderer
  https://github.com/jose-villegas/VCTRenderer/tree/master
