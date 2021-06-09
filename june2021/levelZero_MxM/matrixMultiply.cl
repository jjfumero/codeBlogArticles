__kernel void mxm(__global int* a, __global int* b, __global int *c, const int n) {
	uint idx = get_global_id(0);
	uint jdx = get_global_id(1);

	int sum = 0;
	for (int k = 0; k < n; k++) {
		sum += a[idx * n + k] * b[k * n + jdx];
	}

	c[idx * n + jdx] = sum;
}
