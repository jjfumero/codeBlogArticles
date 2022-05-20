__kernel void vectorAddition(__global int* input, __global int* output) {
	uint idx = get_global_id(0);
	output[idx] = input[idx] + 100;
}
