#if !defined(VOXELS_TEXTURES_GLSL_INCLUDE_GUARD)
#define VOXELS_TEXTURES_GLSL_INCLUDE_GUARD


uniform sampler3D u_VoxelsAlbedoTexture;
uniform sampler3D u_VoxelsNormalsTexture;
uniform sampler3D u_VoxelsIrradianceTexture;
uniform sampler3D u_VoxelsIrradianceAnisoMipmap;


bool isTextureOccupied(ivec3 aVoxel, int aLevel, uint aVoxelMode)
{
	switch(aVoxelMode)
	{
		case CLIENT_VOXEL_MODE_IRRADIANCE:
			vec4 irradiance = texelFetch(u_VoxelsIrradianceTexture, aVoxel, aLevel);
			return irradiance.a > 0;
		default:
			return false;
	}
}


#endif //VOXELS_TEXTURES_GLSL_INCLUDE_GUARD
