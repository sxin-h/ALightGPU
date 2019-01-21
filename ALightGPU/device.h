#pragma once
#include "Hitable.h"
#include "BVH.h"
#include "Sphere.h"
#include <curand_discrete2.h>
#include <device_launch_parameters.h>
#include <curand_kernel.h>
#include "Camera.h"


__global__
void IPRSampler(int d_width, int d_height, int seed, int SPP, int MST, Hitable** d_world, int root, float * d_pixeldata, Camera* d_camera, curandState *const rngStates, Material* materials)
{
	const auto tid = blockIdx.x * blockDim.x + threadIdx.x;
	const auto tid2 = blockIdx.y * blockDim.y + threadIdx.y;

	curand_init(seed + tid + tid2 * d_width, 0, 0, &rngStates[tid]);			//��ʼ�������
	Vec3 color(0, 0, 0);

	const int x = blockIdx.x * 16 + threadIdx.x, y = blockIdx.y * 16 + threadIdx.y;
	//int x = 256, y = 256;
	//float u = float(x) / float(512);
	//float v = float(y) / float(512);

	for (auto j = 0; j < SPP; j++)
	{
		const auto u = float(x + curand_uniform(&rngStates[tid])) / float(d_width), v = float(y + curand_uniform(&rngStates[tid])) / float(d_height);

		Ray ray(d_camera->Origin(), d_camera->LowerLeftCorner() + u * d_camera->Horizontal() + v * d_camera->Vertical() - d_camera->Origin());
		Vec3 c(0, 0, 0);
		Vec3 factor(1, 1, 1);
		for (auto i = 0; i < MST; i++)
		{
			Hitable** stackPtr = new Hitable*[512];
			*stackPtr++ = nullptr; // push
			HitRecord rec;
			rec.t = 99999;
			bool hit;
			//�����ݹ���ֲ��Ҷ���ʹ��ѭ�����ֲ��� �Խ���ջ���
			auto node = d_world[root];
			do
			{
				HitRecord recl, recr;
				auto bvh = static_cast<BVHNode*>(node);
				const auto childL = d_world[bvh->left_id];
				const auto childR = d_world[bvh->right_id];
				bool overlarpR, overlarpL;

				if (childL->type == Instance::BVH)
				{
					const auto child_bvh_L = static_cast<BVHNode*>(childL);
					overlarpL = child_bvh_L->Box.Hit(ray, 0.001, 99999);
				}
				else { overlarpL = childL->Hit(ray, 0.001, 99999, recl, materials, d_world); }


				if (childR->type == Instance::BVH)
				{
					const auto child_bvh_R = static_cast<BVHNode*>(childR);
					overlarpR = child_bvh_R->Box.Hit(ray, 0.001, 99999);
				}
				else { overlarpR = childR->Hit(ray, 0.001, 99999, recr, materials, d_world); }

				if (overlarpL&&childL->type != Instance::BVH)
				{
					if (recl.t < rec.t)rec = recl;
				}

				if (overlarpL&&childL->type != Instance::BVH)
				{
					if (recr.t < rec.t)rec = recr;
				}

				const bool traverseL = (overlarpL && childL->type == Instance::BVH);
				const bool traverseR = (overlarpR && childR->type == Instance::BVH);

				if (!traverseL && !traverseR)
					node = *--stackPtr; // pop
				else
				{
					node = (traverseL) ? childL : childR;
					if (traverseL && traverseR)
						*stackPtr++ = childR; // push
				}

			} while (node != nullptr);

			printf("hit: %d  t:", hit, rec.t);
			//if (d_world[root]->Hit(ray, 0.001, 99999, rec, materials, d_world))
			if (hit)
			{

				// random in unit Sphere
				Vec3 random_in_unit_sphere;
				do random_in_unit_sphere = 2.0*Vec3(curand_uniform(&rngStates[tid]), curand_uniform(&rngStates[tid]), curand_uniform(&rngStates[tid])) - Vec3(1, 1, 1);
				while (random_in_unit_sphere.squared_length() >= 1.0);

				Ray scattered;
				Vec3 attenuation;
				if (i < MST&&rec.mat_ptr->scatter(ray, rec, attenuation, scattered, random_in_unit_sphere, curand_uniform(&rngStates[tid])))
				{
					//auto target = rec.p + rec.normal + random_in_unit_sphere;
					factor *= attenuation;
					ray = scattered;
				}
					//****** �����������������غ�ɫ ******
				else
				{
					c = Vec3(0, 0, 0);
					break;
				}
			}
			else
			{
				const auto t = 0.5*(unit_vector(ray.Direction()).y() + 1);
				c = factor * ((1.0 - t)*Vec3(1.0, 1.0, 1.0) + t * Vec3(0.5, 0.7, 1.0));
				//c = factor * ((1.0 - t)*Vec3(1.0, 1.0, 1.0) + t * Vec3(73/255.0, 93/255.0, 160/255.0));
				break;
			}
		}
		color += c;
	}
	//color /= SPP;

	//SetColor
	const auto i = d_width * 4 * y + x * 4;
	d_pixeldata[i] += color.r();
	d_pixeldata[i + 1] += color.g();
	d_pixeldata[i + 2] += color.b();
	d_pixeldata[i + 3] += SPP;
}

__global__ void WorldArrayFixer(Hitable** d_world, Hitable** new_world)//,int rootid,BVHNode* root)//
{
	const auto i = threadIdx.x;//TODO add dim ,1024 is not enough
	switch (d_world[i]->type)
	{
	case 1:
		new_world[i] = new Sphere(d_world[i]);
		break;
	case 2:
		new_world[i] = new BVHNode(d_world[i]);
		break;
	default:;
	}
}

__global__ void ArraySetter(Hitable** location, int i, Hitable* obj)//
{
	location[i] = obj;
}