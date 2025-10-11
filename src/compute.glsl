#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform layout(binding=0,rgba32f) writeonly restrict image2D screen;


void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, vec4(1.0, 1.0, 0.0, 0.0));
}

