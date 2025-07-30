#if !defined(VOXELS_TEXTURES_GLSL_INCLUDE_GUARD)
#define VOXELS_TEXTURES_GLSL_INCLUDE_GUARD


#define ANISO_DIRECTIONS 6

uniform sampler3D u_VoxelsAlbedoTexture;
uniform sampler3D u_VoxelsNormalsTexture;
uniform sampler3D u_VoxelsIrradianceTexture;
uniform layout(binding = 16) sampler3D u_VoxelsIrradianceAnisoMipmap[ANISO_DIRECTIONS];

uniform bool u_AnisotropicIrradianceMipmaps;
// Only used for debug view presenting the irradiance aniso levels
uniform uint u_HardcodedAnisoDirection = 5;


bool isTextureOccupied(ivec3 aVoxel, int aLevel, uint aVoxelMode)
{
	switch(aVoxelMode)
	{
		case CLIENT_VOXEL_MODE_IRRADIANCE:
			vec4 irradiance;
            if(u_AnisotropicIrradianceMipmaps && (aLevel > 0))
            {
				irradiance = texelFetch(u_VoxelsIrradianceAnisoMipmap[u_HardcodedAnisoDirection],
									    aVoxel,
										aLevel - 1);
            }
            else
            {
				irradiance = texelFetch(u_VoxelsIrradianceTexture, aVoxel, aLevel);
            }
			return irradiance.a > 0;
		default:
			return false;
	}
}


#endif //VOXELS_TEXTURES_GLSL_INCLUDE_GUARD
