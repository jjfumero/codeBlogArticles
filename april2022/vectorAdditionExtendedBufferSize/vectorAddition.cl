__kernel void vectorAdd(__global float* a, __global float* b, __global float *c) {
	uint idx = get_global_id(0);
	c[idx] = a[idx] + b[idx];
}
