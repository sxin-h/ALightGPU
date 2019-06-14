#include "Material.h"

#include "Ray.h"
#include "SurfaceHitRecord.h"
#include "RTDeviceData.h"


float Schlick(float cosine, float ref_idx) {
	float r0 = (1 - ref_idx) / (1 + ref_idx);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

bool Refract(const float3 & v, const float3 & n, float ni_over_nt, float3 & refracted) {
	const float3 uv = normalize(v);
	const float dt = dot(uv, n);
	const float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1 - dt * dt);
	if (discriminant > 0)
	{
		refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
		return true;
	}
	else
		return false;
}

float3 Reflect(const float3 & v, const float3 & n)
{
	return v - 2 * dot(v, n) * n;
}


bool Material::scatter(const Ray& r_in, const SurfaceHitRecord& rec, float3& attenuation, Ray& scattered,
	float3 random_in_unit_sphere,const RTDeviceData* rt_data)
{
	switch (Type)
	{
	case lambertian:
	{
		const int texid = data[0];
		// const auto albedo = make_float3(
		// 	tex2DLayered<float>(rt_data->Textures[texid], rec.uv.x, 1 - rec.uv.y, 0),
		// 	tex2DLayered<float>(rt_data->Textures[texid], rec.uv.x, 1 - rec.uv.y, 1),
		// 	tex2DLayered<float>(rt_data->Textures[texid], rec.uv.x, 1 - rec.uv.y, 2));
			const auto albedo = make_float3(0,0,0);
		
		//printf("%d\n", albedo);
		const auto target = rec.p + rec.normal + random_in_unit_sphere;
		scattered = Ray(rec.p, target - rec.p);
		attenuation = albedo;
		return true;
	}
	case metal:
	{
		const auto albedo = make_float3(data[0], data[1], data[2]);
		const auto fuzz = data[3];

		const auto reflected = Reflect(normalize(r_in.direction), rec.normal);
		scattered = Ray(rec.p, reflected + fuzz * random_in_unit_sphere);
		attenuation = albedo;
		//printf("%f %f %f \n", rec.normal[0], rec.normal[1], rec.normal[2]);
		return (dot(scattered.direction, rec.normal) > 0);
	}
	case dielectirc:
	{
		const auto ref_idx = data[0];

		float3 outward_normal;
		const auto reflected = Reflect(r_in.direction, rec.normal);
		float ni_over_nt;
		attenuation = make_float3(1.0, 1.0, 1.0);
		float3 refracted;
		float reflect_prob;
		float cosine; 
		if (dot(r_in.direction, rec.normal) > 0)
		{
			outward_normal = -(rec.normal);
			ni_over_nt = ref_idx;
			//         cosine = ref_idx * dot(r_in.direction, rec.normal) / r_in.direction.length();
			cosine = dot(r_in.direction, rec.normal) / length(r_in.direction);
			cosine = sqrt(1 - ref_idx * ref_idx * (1 - cosine * cosine));
		}
		else
		{
			outward_normal = rec.normal;
			ni_over_nt = 1.0 / ref_idx;
			cosine = -dot(r_in.direction, rec.normal) / length(r_in.direction);
		}
		if (Refract(r_in.direction, outward_normal, ni_over_nt, refracted))
			reflect_prob = Schlick(cosine, ref_idx);
		else
			reflect_prob = 1.0;
		if (rt_data->GetRandom() < reflect_prob)
			scattered = Ray(rec.p, reflected);
		else
			scattered = Ray(rec.p, refracted);
		return true;
	}
	}

}