#pragma once
#include <cuda_runtime_api.h>
#include <curand_kernel.h>
#include "Camera.h"
__global__ void IPRSampler(int width, int height,
                           int seed, int spp, int Sampled, int MST, int root,
                           float* output, curandState* const rngStates,Camera* camera);
void SetConstants();