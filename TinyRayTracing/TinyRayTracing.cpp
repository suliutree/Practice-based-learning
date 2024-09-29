#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>

struct vec3 {
    float x = 0, y = 0, z = 0;
    vec3() = default;
    vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float& operator[] (const int i) { return i == 0 ? x : i == 1 ? y : z; }
    const float& operator[] (const int i) const { return i == 0 ? x : i == 1 ? y : z; }
    vec3  operator*(const float v) const { return vec3(x * v, y * v, z * v); }
    float operator*(const vec3& v) const { return x*v.x + y*v.y + z*v.z; }
    vec3  operator+(const vec3& v) const { return vec3(x + v.x, y + v.y, z + v.z); }
    vec3  operator-(const vec3& v) const { return vec3(x - v.x, y - v.y, z - v.z); }
    vec3  operator-()              const { return vec3(-x, -y, -z); }
    float norm() const { return std::sqrt(x*x + y*y + z*z);}
    vec3 normalized() const { return (*this)*(1.f/norm()); }
};

vec3 cross(const vec3 v1, const vec3 v2) {
    return vec3( v1.y*v2.z - v1.z*v2.y, v1.z*v2.x - v1.x*v2.z, v1.x*v2.y - v1.y*v2.x );
}

vec3 reflect(const vec3 &I, const vec3 &N) {
    return I - N * 2.f * (I * N);
}

vec3 refract(const vec3 &I, const vec3 &N, const float &refractive_index) {
    float cosi = -std::max(-1.f, std::min(1.f, I * N)); // 计算入射光线与法线的点积，负号用于调整方向，使内积为负时表示光线在物体内部。
    float etai = 1, etat = refractive_index;           // etai为入射介质的折射率（空气通常为1），etat为目标介质的折射率。
    vec3 n = N;
    if (cosi < 0) {
        cosi = -cosi;
        std::swap(etai, etat); n = -N;                 // 交换折射率，因为光线从目标介质进入入射介质，同时反转法线的方向。
    }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);       // 这里 k 用于判断是否发生全反射。
    return k < 0 ? vec3(0, 0, 0) : I * eta + n * (eta * cosi - sqrtf(k)); 
}

struct light {
    light(const vec3 &p, const float &i) : position(p), intensity(i) {}
    vec3 position;
    float intensity;
};

struct material {
    material(const float &r, const vec3 &a, const float &a3, const vec3 &color, const float &spec) 
        : refractive_index(r)
        , albedo(a)
        , albedo3(a3)
        , diffuse_color(color)
        , specular_exponent(spec) {}
    material() : refractive_index(1), albedo(1, 0, 0), albedo3(0), diffuse_color(), specular_exponent() {}
    float refractive_index;
    vec3 albedo;
    float albedo3;
    vec3 diffuse_color;
    float specular_exponent;
};


struct sphere {
    vec3  center;
    float radius;
    material mate;
    
    sphere(const vec3 &c, const float &r, const material &m) : center(c), radius(r), mate(m) {}

    /**
     * orig：光线的起点（原点）。
     * dir：光线的方向（单位向量）。
     * t0：用于存储相交点距离光线起点的距离。
     */
    bool ray_intersect(const vec3 &orig, const vec3 &dir, float &t0) const {
        vec3 L = center - orig;                     // 起点到球心的向量
        float tca = L * dir;                        // L 在光线方向 dir 上的投影长度
        float d2 = L*L - tca*tca;                   // 球心到光线方向的垂直距离
        if (d2 > radius * radius)                   // 判断光线是否与球相交
            return false;
        float thc = sqrtf(radius * radius - d2);    // 与球的两个交点之间距离的一半
        t0 = tca - thc;                             // 第一个交点距离原点的距离
        float t1 = tca + thc;                       // 第二个交点距离原点的距离
        if (t0 < 0) t0 = t1;                        // 处理光线起点在球体的内部的情况，如果第一个交点 t0 小于 0，说明第一个交点在光线的起点后面。
        if (t0 < 0) return false;                   // 如果更新后的 t0 仍然小于 0，说明两个交点都在光线的起点后面，光线与球体相交但在起点后方。此时认为没有有效的交点，返回 false。
        return true;
    }
};

bool scene_intersect(const vec3 &orig, const vec3 &dir, const std::vector<sphere> &spheres, vec3 &hit, vec3 &N, material &material) {
    float sphere_dist = std::numeric_limits<float>::max();
    for (size_t i = 0; i < spheres.size(); ++i) {
        float dist_i;
        if (spheres[i].ray_intersect(orig, dir, dist_i) && dist_i < sphere_dist) {
            sphere_dist = dist_i;
            hit = orig + dir*dist_i;
            N = (hit - spheres[i].center).normalized();
            material = spheres[i].mate;
        }
    }

    float checkerboard_dist = std::numeric_limits<float>::max();
    if (fabs(dir.y) > 1e-3) {
        float d = -(orig.y + 4) / dir.y;
        vec3 pt = orig + dir * d;
        if (d > 0 && fabs(pt.x) < 10 && pt.z < -10 && pt.z > -30 && d < sphere_dist) {
            checkerboard_dist = d;
            hit = pt;
            N = vec3(0, 1, 0);
            material.diffuse_color = (int(.5 * hit.x + 1000) + int(.5 * hit.z)) & 1 ? vec3(1, 1, 1) : vec3(1, .7, .3);
            material.diffuse_color = material.diffuse_color * 0.3;
        }
    }

    return std::min(sphere_dist, checkerboard_dist) < 1000;
}

vec3 cast_ray(const vec3 &orig, const vec3 &dir, const std::vector<sphere> &spheres, const std::vector<light> &lights, size_t depth=0) {
    vec3 point, N;
    material mate;

    if (depth > 4 || !scene_intersect(orig, dir, spheres, point, N, mate)) {
        return vec3(0.2, 0.7, 0.8);
    }

    vec3 reflect_dir   = reflect(dir, N).normalized();
    vec3 reflect_orig  = reflect_dir * N < 0 ? point - N*1e-3 : point + N*1e-3;
    vec3 reflect_color = cast_ray(reflect_orig, reflect_dir, spheres, lights, depth+1);

    vec3 refract_dir   = refract(dir, N, mate.refractive_index).normalized();
    vec3 refract_orig  = refract_dir * N < 0 ? point - N*1e-3 : point + N*1e-3;
    vec3 refract_color = cast_ray(refract_orig, refract_dir, spheres, lights, depth+1);

    float diffuse_light_intensity  = 0;
    float specular_light_intensity = 0;
    for (size_t i = 0; i < lights.size(); ++i) {
        vec3 light_dir = (lights[i].position - point).normalized();
        float light_distance = (lights[i].position - point).norm();

        vec3 shadow_orig = light_dir * N < 0 ? point - N*1e-3 : point + N*1e-3;
        vec3 shadow_pt, shadow_N;
        material tmpmaterial;
        if (scene_intersect(shadow_orig, light_dir, spheres, shadow_pt, shadow_N, tmpmaterial) && (shadow_pt-shadow_orig).norm() < light_distance)
            continue;

        diffuse_light_intensity  += lights[i].intensity * std::max(light_dir * N, 0.f);
        specular_light_intensity += powf(std::max(-reflect(-light_dir, N) * dir, 0.f),  mate.specular_exponent) * lights[i].intensity;
    }

    return mate.diffuse_color * diffuse_light_intensity * mate.albedo[0]
           + vec3(1., 1., 1.) * specular_light_intensity * mate.albedo[1]
           + reflect_color * mate.albedo[2]
           + refract_color * mate.albedo3;
}

int render(const std::vector<sphere> &spheres, const std::vector<light> &lights) {
    const int width  = 1200;
    const int height = 800;
    const int channels = 3;
    const int fov = M_PI / 2;

    std::vector<unsigned char> image(width * height * channels, 0);

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int index = (i + j * width) * channels;

            float x =  (2*(i + 0.5)/(float)width  - 1)*tan(fov/2.)*width/(float)height;
            float y = -(2*(j + 0.5)/(float)height - 1)*tan(fov/2.);
            vec3 dir = vec3(x, y, -1).normalized();
            vec3 color = cast_ray(vec3(0, 0, 0), dir, spheres, lights);

            float max = std::max(color.x, std::max(color.y, color.z));
            if (max > 1.f) color = color * (1.f/max);

            image[index + 0] = static_cast<unsigned char>(color.x * 255);
            image[index + 1] = static_cast<unsigned char>(color.y * 255);
            image[index + 2] = static_cast<unsigned char>(color.z * 255);
        }
    }

    const char* filename = "out.png";

    if (stbi_write_png(filename, width, height, channels, image.data(), width * channels)) {
        std::cout << "success!" << std::endl;
        return 0;
    } else {
        std::cout << "failed!" << std::endl;
        return 1;
    }
}


int main()
{
    material      ivory(1.0, vec3(0.6, 0.3,  0.1), 0.0, vec3(0.4, 0.4, 0.3), 50.f);
    material      glass(1.5, vec3(0.0, 0.5,  0.1), 0.8, vec3(0.6, 0.7, 0.8), 50.f);
    material red_rubber(1.0, vec3(0.9, 0.1,  0.0), 0.0, vec3(0.3, 0.1, 0.1), 10.f);
    material     mirror(1.0, vec3(0.0, 10.0, 0.8), 0.0, vec3(1.0, 1.0, 1.0), 1425.f);

    std::vector<sphere> spheres;
    spheres.push_back(sphere(vec3(  -3,    0, -16), 2, ivory));
    spheres.push_back(sphere(vec3(-1.0, -1.5, -12), 2, glass));
    spheres.push_back(sphere(vec3( 1.5, -0.5, -18), 3, red_rubber));
    spheres.push_back(sphere(vec3(   7,    5, -18), 4, mirror));

    std::vector<light> lights;
    lights.push_back(light(vec3(-20, 20,  20), 1.5));
    lights.push_back(light(vec3( 30, 50, -25), 1.8));
    lights.push_back(light(vec3( 30, 20,  30), 1.7));

    return render(spheres, lights);
}