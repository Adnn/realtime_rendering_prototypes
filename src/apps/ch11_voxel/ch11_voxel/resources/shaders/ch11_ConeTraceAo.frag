#version 460


#include "ch11_VoxelsSsbo.glsl"
#include "shaders/Helpers.glsl"


in vec3 ex_Normal_world;
in vec3 ex_Position_world;

uniform sampler3D u_VoxelsAlbedoTexture;

uniform float u_TanHalfAperture;

uniform float u_VoxelSize;
uniform vec3 u_AabbMin;
uniform bool u_GridAlign;

out vec4 out_Color;

// Partition the hemisphere with 7 cones is convenient:
// with a half-angle of 30°, 1 cone pointing along the normal, and 5 ring cones
// All cones are touching their neighbors:
// In a vertical plane (e.g.XY), 3 cones are fitting (3 * 2*30 = 180)
// In the horizontal plane XZ, 6 cones are fitting (6 * 2*30 = 360)
//const vec3 diffuseConeAxes[] =
//{
//    vec3(  0.0f, 0.0f,  1.0f), // Along +Z
//    vec3(  0.0f,  0.866025f, 0.5f), // ring, toward +X
//    vec3( 0.75f,  0.433013f, 0.5f), 
//    vec3( 0.75f, -0.433013f, 0.5f),
//    vec3(  0.0f, -0.866025f, 0.5f), // ring, toward -X
//    vec3(-0.75f, -0.433013f, 0.5f),
//    vec3(-0.75f,  0.433013f, 0.5f), 
//};

// But usually found split in 6:
const vec3 diffuseConeDirections[] =
{
    vec3( 0.0f,		 0.0f,  1.0f),
    vec3( 0.0f,		  0.866025f,  0.5),
    vec3( 0.823639f,  0.267617f,  0.5),
    vec3( 0.509037f, -0.7006629f, 0.5),
    vec3(-0.50937f,  -0.7006629f, 0.5),
    vec3(-0.823639f,  0.267617f,  0.5)
};

const float diffuseConeWeights[] =
{
    M_PI / 4.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
    3.0f * M_PI / 20.0f,
};



float traceCone(vec3 position_aabb, vec3 normal_aabb, vec3 coneAxis_aabb, float tanHalfAngle)
{
	// TODO: check reference implementation
	const float maxDistance = 2;

	// TODO (I think this is beta in the explanation)
	const float samplingFactor = 1;

	// TODO determine good range
	const float aoFalloff = 10;
	float falloff = 0.5f * aoFalloff / u_VoxelSize;

	vec3 direction_aabb = coneAxis_aabb;
	if (u_GridAlign)
	{
		position_aabb = (floor(position_aabb / u_VoxelSize) + vec3(0.5)) * u_VoxelSize;
	}
	vec3 startPosition_aabb = position_aabb + normal_aabb * u_VoxelSize; 
	//vec3 startPosition = position_aabb; // or grid centered?

	// Distance marched along the cone, in world unit
	float t = 1.0 * u_VoxelSize; // Offset to limit self-sampling

	// ambient occlusion
	float occlusion = 0;

	while(occlusion < 1.0f && t <= maxDistance)
	{
		float coneDiameter = 2 * t * tanHalfAngle;
		float mipLevel = log2(coneDiameter / u_VoxelSize);

		vec3 samplePosition_aabb = startPosition_aabb + direction_aabb * t;
		vec3 position_uvw = samplePosition_aabb / (u_VoxelSize * ub_GridDimension);
		vec4 albedo = textureLod(u_VoxelsAlbedoTexture, position_uvw, mipLevel);

		// TODO: actual irradiance marching
		// front to back
		//coneSample += (1.0f - coneSample.a) * albedo;

		occlusion += ((1.0f - occlusion) * albedo.a) / (1.0f + falloff * coneDiameter);
		// march the cone
		t += coneDiameter * samplingFactor;
	}

	return occlusion;
}


// see: Building an Orthonormal Basis, Revisited (Pixar)
void revisedONB(vec3 n, out vec3 b1, out vec3 b2)
{
	if (n.z<0.0f)
	{
		const float a = 1.0f / (1.0f - n.z);
		const float b = n.x * n.y * a;
		b1 = vec3(1.0f - n.x * n.x * a, -b, n.x);
		b2 = vec3(b, n.y * n.y*a - 1.0f, -n.y);
	}
	else
	{
		const float a = 1.0f / (1.0f + n.z);
		const float b = -n.x * n.y * a;
		b1 = vec3(1.0f - n.x * n.x * a, b, -n.x);
		b2 = vec3(b, 1.0f - n.y * n.y * a, -n.y);
	}
}


void main(void)
{
	out_Color = vec4(mapToRgb(ex_Normal_world), 1);
	//return;

	// Sample the 3D texture at the fragment position
	{
		// TODO: determine uvw at vertex level
		vec3 position_aabb = ex_Position_world - u_AabbMin;
		//ivec3 position_voxel = ivec3(position_aabb / u_VoxelSize);
		//vec3 uvw = position_voxel / vec3(ub_GridDimension);
		vec3 uvw = (position_aabb / u_VoxelSize) / vec3(ub_GridDimension);
		float occupancy = texture(u_VoxelsAlbedoTexture, uvw).a;

		out_Color = vec4(vec3(occupancy), 1);
	}

	{
		vec3 position_aabb = ex_Position_world - u_AabbMin;
		vec3 normal_world = normalize(ex_Normal_world);

		vec3 tangent, bitangent;
		revisedONB(normal_world, tangent, bitangent);
		mat3 tangentToWorld = mat3(tangent, bitangent, normal_world);

		const uint coneCount = 6;
		float occlusion = 0;
		for(uint i = 0; i != coneCount; ++i)
		{
			vec3 coneAxis_world = tangentToWorld * diffuseConeDirections[i];
			// Note: The AABB is aligned on world axis, so directions are matching
			occlusion += 
				traceCone(position_aabb, normal_world, coneAxis_world, u_TanHalfAperture)
				* diffuseConeWeights[i]
				;
		}

		// is the same direction in AABB.
		//occlusion = traceCone(position_aabb, normal_world, normal_world, u_TanHalfAperture);

		out_Color = vec4(vec3(1-occlusion), 1);
	}
}
