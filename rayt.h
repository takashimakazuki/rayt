#include <memory>
#include <iostream>
#include <random>
#include <float.h> // FLT_MIN, FLT_MAX
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "vectormath/scalar/cpp/vectormath_aos.h"
using namespace Vectormath::Aos;
typedef Vector3 vec3;
typedef Vector3 col3;

#define PI 3.14159265359f
#define PI2 6.28318530718f
#define RECIP_PI 0.31830988618f
#define RECIP_PI2 0.15915494f
#define LOG2 1.442695f
#define EPSILON 1e-6f
#define GAMMA_FACTOR 2.2f

#define NUM_THREAD 8
#define MAX_DEPTH 50

inline float pow2(float x)
{
    return x * x;
}
inline float pow3(float x) { return x * x * x; }
inline float pow4(float x) { return x * x * x * x; }
inline float pow5(float x) { return x * x * x * x * x; }
inline float clamp(float x, float a, float b) { return x < a ? a : x > b ? b
                                                                         : x; }
inline float saturate(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f
                                                                : x; }
inline float recip(float x) { return 1.f / x; }
inline float mix(float a, float b, float t) { return a * (1.f - t) + b * t; /* return a + (b-a) * t; */ }
inline float step(float edge, float x) { return (x < edge) ? 0.f : 1.f; }
inline float smoothstep(float a, float b, float t)
{
    if (a >= b)
        return 0.f;
    float x = saturate((t - a) / (b - a));
    return x * x * (3.f - 2.f * t);
}
inline float radians(float deg) { return (deg / 180.f) * PI; }
inline float degrees(float rad) { return (rad / PI) * 180.f; }

inline vec3 random_vector()
{
    return vec3(drand48(), drand48(), drand48());
}

inline vec3 random_in_unit_sphere()
{
    vec3 p;
    do
    {
        // adujust [0, 1] to [-1, 1]
        p = 2.f * random_vector() - vec3(1.f);
    } while (lengthSqr(p) >= 1.f);
    return p;
}

inline vec3 linear_to_gamma(const vec3 &v, float gammaFactor)
{
    float recipGammaFactor = recip(gammaFactor);
    return vec3(
        powf(v.getX(), recipGammaFactor),
        powf(v.getY(), recipGammaFactor),
        powf(v.getZ(), recipGammaFactor));
}

inline vec3 gamma_to_linear(const vec3 &v, float gammaFactor)
{
    return vec3(
        powf(v.getX(), gammaFactor),
        powf(v.getY(), gammaFactor),
        powf(v.getZ(), gammaFactor));
}

inline vec3 reflect(const vec3 &v, const vec3 &n)
{
    return v - 2.f * dot(v, n) * n;
}

inline bool refract(const vec3 &v, const vec3 &n, float ni_over_nt, vec3 &refracted)
{
    vec3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.f - pow2(ni_over_nt) * (1.f - pow2(dt));
    if (D > 0.f)
    {
        refracted = -ni_over_nt * (uv - n * dt) - n * sqrt(D);
        return true;
    }
    else
    {
        return false;
    }
}

inline float schlick(float cosine, float ri)
{
    float r0 = pow2((1.f - ri) / (1.f + ri));
    return r0 + (1.f - r0) * pow5(1.f - cosine);
}

inline void get_sphere_uv(const vec3 &p, float &u, float &v)
{
    float phi = atan2(p.getZ(), p.getX());
    float theta = asin(p.getY());

    u = 1.f - (phi + PI) / (2.f * PI);
    v = (theta + PI / 2.f) / PI;
}

namespace rayt
{
    class Texture;
    typedef std::shared_ptr<Texture> TexturePtr;
    class ColorTexture;
    class CheckerTexture;

    class Material;
    typedef std::shared_ptr<Material> MaterialPtr;

    class Shape;
    typedef std::shared_ptr<Shape> ShapePtr;

    class Texture
    {
    public:
        virtual vec3 value(float u, float v, const vec3 &p) const = 0;
    };

    class ColorTexture : public Texture
    {
    public:
        ColorTexture(const vec3 &c) : m_color(c) {}

        vec3 value(float u, float v, const vec3 &p) const override
        {
            return m_color;
        }

    private:
        vec3 m_color;
    };

    class CheckerTexture : public Texture
    {
    public:
        CheckerTexture(const TexturePtr &t0, const TexturePtr &t1, float freq)
            : m_odd(t0), m_even(t1), m_freq(freq) {}
        virtual vec3 value(float u, float v, const vec3 &p) const override
        {
            float sines = sinf(m_freq * p.getX()) * sinf(m_freq * p.getY()) * sinf(m_freq * p.getZ());
            if (sines < 0)
            {
                return m_odd->value(u, v, p);
            }
            else
            {
                return m_even->value(u, v, p);
            }
        }

    private:
        TexturePtr m_odd;
        TexturePtr m_even;
        float m_freq;
    };

    class ImageTexture : public Texture
    {
    public:
        ImageTexture(const char *name)
        {
            int nn;
            m_texels = stbi_load(name, &m_width, &m_height, &nn, 0);
        }

        virtual ~ImageTexture()
        {
            stbi_image_free(m_texels);
        }

        virtual vec3 value(float u, float v, const vec3 &p) const override
        {
            int i = (u)*m_width;
            int j = (1 - v) * m_height - 0.001;
            return sample(i, j);
        }

        vec3 sample(int u, int v) const
        {
            u = u < 0 ? 0 : u >= m_width ? m_width - 1
                                         : u;
            v = v < 0 ? 0 : v >= m_height ? m_height - 1
                                          : v;
            return vec3(
                int(m_texels[3 * u + 3 * m_width * v]) / 255.0,
                int(m_texels[3 * u + 3 * m_width * v + 1]) / 255.0,
                int(m_texels[3 * u + 3 * m_width * v + 2]) / 255.0);
        }

    private:
        int m_width;
        int m_height;
        unsigned char *m_texels;
    };
    class ImageFilter
    {
    public:
        virtual vec3 filter(const vec3 &c) const = 0;
    };

    class GammaFilter : public ImageFilter
    {
    public:
        GammaFilter(float factor) : m_factor(factor) {}
        virtual vec3 filter(const vec3 &c) const override
        {
            return linear_to_gamma(c, m_factor);
        }

    private:
        float m_factor;
    };

    class TonemapFilter : public ImageFilter
    {
    public:
        TonemapFilter() {}
        virtual vec3 filter(const vec3 &c) const override
        {
            return minPerElem(maxPerElem(c, Vector3(0.f)), Vector3(1.f));
        }
    };

    class Image
    {
    public:
        struct rgb
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        };

        Image() : m_pixels(nullptr) {}
        Image(int w, int h)
        {
            m_width = w;
            m_height = h;
            m_pixels.reset(new rgb[m_width * m_height]);

            m_filters.push_back(std::make_unique<GammaFilter>(GAMMA_FACTOR));
            m_filters.push_back(std::make_unique<TonemapFilter>());
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        void *pixels() const { return m_pixels.get(); }

        void write(int x, int y, float r, float g, float b)
        {
            vec3 c(r, g, b);
            for (auto &f : m_filters)
            {
                c = f->filter(c);
            }
            int index = m_width * y + x;
            m_pixels[index].r = static_cast<unsigned char>(c.getX() * 255.99f);
            m_pixels[index].g = static_cast<unsigned char>(c.getY() * 255.99f);
            m_pixels[index].b = static_cast<unsigned char>(c.getZ() * 255.99f);
        }

    private:
        int m_width;
        int m_height;
        std::unique_ptr<rgb[]> m_pixels;
        std::vector<std::unique_ptr<ImageFilter>> m_filters;
    };

    class Ray
    {
    public:
        Ray() {}
        Ray(const vec3 &o, const vec3 &dir)
            : m_origin(o), m_direction(dir)
        {
        }

        const vec3 &origin() const { return m_origin; }
        const vec3 &direction() const { return m_direction; }
        vec3 at(float t) const { return m_origin + t * m_direction; }

    private:
        vec3 m_origin;    // 始点
        vec3 m_direction; // 方向（非正規化）
    };

    class Camera
    {
    public:
        Camera() {}
        Camera(const vec3 &u, const vec3 &v, const vec3 &w)
        {
            m_origin = vec3(0);
            m_uvw[0] = u;
            m_uvw[1] = v;
            m_uvw[2] = w;
        }
        Camera(
            const vec3 &lookfrom,
            const vec3 &lookat,
            const vec3 &vup,
            float vfov,
            float aspect)
        {
            vec3 u, v, w;
            float halfH = tanf(radians(vfov) / 2.0f);
            float halfW = aspect * halfH;
            m_origin = lookfrom;
            w = normalize(lookfrom - lookat);
            u = normalize(cross(vup, w));
            v = cross(w, u);
            m_uvw[2] = m_origin - halfW * u - halfH * v - w;
            m_uvw[0] = 2.0f * halfW * u;
            m_uvw[1] = 2.0f * halfH * v;
        }

        Ray getRay(float u, float v) const
        {
            return Ray(m_origin, m_uvw[2] + m_uvw[0] * u + m_uvw[1] * v - m_origin);
        }

    private:
        vec3 m_origin; // position of the camera
        vec3 m_uvw[3]; // orthonormal basis vector
    };

    class HitRec
    {
    public:
        float t;
        float u;
        float v;
        vec3 p;
        vec3 n;
        MaterialPtr mat;
    };

    class ScatterRec
    {
    public:
        Ray ray;
        vec3 albedo;
    };

    class Material
    {
    public:
        virtual bool scatter(const Ray &r, const HitRec &hrec, ScatterRec &srec) const = 0;
        virtual vec3 emitted(const Ray &r, const HitRec &hrec) const { return vec3(0); }
    };

    class Lambertian : public Material
    {
    public:
        Lambertian(const TexturePtr &a) : m_albedo(a)
        {
        }
        virtual bool scatter(const Ray &r, const HitRec &hrec, ScatterRec &srec) const override
        {
            vec3 target = hrec.p + hrec.n + random_in_unit_sphere();
            srec.ray = Ray(hrec.p, target - hrec.p);
            srec.albedo = m_albedo->value(hrec.u, hrec.v, hrec.p);
            return true;
        };

    private:
        TexturePtr m_albedo;
    };

    class Metal : public Material
    {
    public:
        Metal(const TexturePtr &a, float fuzz)
            : m_albedo(a),
              m_fuzz(fuzz)
        {
        }

        virtual bool scatter(const Ray &r, const HitRec &hrec, ScatterRec &srec) const override
        {
            vec3 reflected = reflect(normalize(r.direction()), hrec.n);
            reflected += m_fuzz * random_in_unit_sphere();
            srec.ray = Ray(hrec.p, reflected);
            srec.albedo = m_albedo->value(hrec.u, hrec.v, hrec.p);
            return dot(srec.ray.direction(), hrec.n) > 0;
        }

    private:
        TexturePtr m_albedo;
        float m_fuzz;
    };

    class Dielectric : public Material
    {
    public:
        Dielectric(float ri)
            : m_ri(ri)
        {
        }

        virtual bool scatter(const Ray &r, const HitRec &hrec, ScatterRec &srec) const override
        {
            vec3 outward_normal;
            vec3 reflected = reflect(r.direction(), hrec.n);
            float ni_over_nt;
            float reflect_prob;
            float cosine;
            if (dot(r.direction(), hrec.n) > 0)
            {
                outward_normal = -hrec.n;
                ni_over_nt = m_ri;
                cosine = m_ri * dot(r.direction(), hrec.n) / length(r.direction());
            }
            else
            {
                outward_normal = hrec.n;
                ni_over_nt = recip(m_ri);
                cosine = -dot(r.direction(), hrec.n) / length(r.direction());
            }

            srec.albedo = vec3(1);

            vec3 refracted;
            if (refract(-r.direction(), outward_normal, ni_over_nt, refracted))
            {
                reflect_prob = schlick(cosine, m_ri);
            }
            else
            {
                reflect_prob = 1;
            }

            if (drand48() < reflect_prob)
            {
                srec.ray = Ray(hrec.p, reflected);
            }
            else
            {
                srec.ray = Ray(hrec.p, refracted);
            }

            return true;
        }

    private:
        float m_ri;
    };

    class DiffuseLight : public Material
    {
    public:
        DiffuseLight(const TexturePtr &emit)
            : m_emit(emit) {}

        virtual bool scatter(const Ray &r, const HitRec &hrec, ScatterRec &srec) const override
        {
            return false;
        }

        virtual vec3 emitted(const Ray &r, const HitRec &hrec) const override
        {
            return m_emit->value(hrec.u, hrec.v, hrec.p);
        }

    private:
        TexturePtr m_emit;
    };

    class Shape
    {
    public:
        virtual bool hit(const Ray &r, float t0, float t1, HitRec &hrec) const = 0;
    };

    class Sphere : public Shape
    {
    public:
        Sphere() {}
        Sphere(const vec3 &c, float r, const MaterialPtr &mat)
            : m_center(c),
              m_radius(r),
              m_material(mat)
        {
        }

        virtual bool hit(const Ray &r, float t0, float t1, HitRec &hrec) const override
        {
            vec3 oc = r.origin() - m_center;
            float a = dot(r.direction(), r.direction());
            float b = 2.0 * dot(oc, r.direction());
            float c = dot(oc, oc) - pow2(m_radius);
            float D = b * b - 4 * a * c;
            if (D > 0)
            {
                float root = sqrtf(D);
                float temp = (-b - root) / (2.0f * a);
                if (temp < t1 && temp > t0)
                {
                    hrec.t = temp;
                    hrec.p = r.at(hrec.t);
                    hrec.n = (hrec.p - m_center) / m_radius;
                    hrec.mat = m_material;
                    get_sphere_uv(hrec.n, hrec.u, hrec.v);
                    return true;
                }
                temp = (-b + root) / (2.0f * a);
                if (temp < t1 && temp > t0)
                {
                    hrec.t = temp;
                    hrec.p = r.at(hrec.t);
                    hrec.n = (hrec.p - m_center) / m_radius;
                    hrec.mat = m_material;
                    get_sphere_uv(hrec.n, hrec.u, hrec.v);
                    return true;
                }
            }

            return false;
        }

    private:
        vec3 m_center;
        float m_radius;
        MaterialPtr m_material;
    };

    class Rect : public Shape
    {
    public:
        enum AxisType
        {
            kXY = 0,
            kXZ,
            kYZ,
        };
        Rect() {}
        Rect(float x0, float x1, float y0, float y1, float k, AxisType axis, const MaterialPtr &m)
            : m_x0(x0), m_x1(x1), m_y0(y0), m_y1(y1), m_k(k), m_axis(axis), m_material(m) {}

        virtual bool hit(const Ray &r, float t0, float t1, HitRec &hrec) const override
        {
            int xi, yi, zi;
            vec3 axis;
            switch (m_axis)
            {
            case kXY:
            {
                xi = 0;
                yi = 1;
                zi = 2;
                axis = vec3::zAxis();
                break;
            };
            case kXZ:
            {
                xi = 0;
                yi = 2;
                zi = 1;
                axis = vec3::yAxis();
                break;
            };
            case kYZ:
            {
                xi = 1;
                yi = 2;
                zi = 0;
                axis = vec3::xAxis();
                break;
            };
            }
            float t = (m_k - r.origin()[zi]) / r.direction()[zi];
            if (t < t0 || t > t1)
            {
                return false;
            }

            float x = r.origin()[xi] + t * r.direction()[xi];
            float y = r.origin()[yi] + t * r.direction()[yi];
            if (x < m_x0 || x > m_x1 || y < m_y0 || y > m_y1)
            {
                return false;
            }

            hrec.u = (x - m_x0) / (m_x1 - m_x0);
            hrec.v = (y - m_y0) / (m_y1 - m_y0);
            hrec.t = t;
            hrec.mat = m_material;
            hrec.p = r.at(t);
            hrec.n = axis;
            return true;
        }

    private:
        float m_x0;
        float m_x1;
        float m_y0;
        float m_y1;
        float m_k;
        AxisType m_axis;
        MaterialPtr m_material;
    };

    class ShapeList : public Shape
    {
    public:
        ShapeList() {}

        void add(const ShapePtr &shape)
        {
            m_list.push_back(shape);
        }

        virtual bool hit(const Ray &r, float t0, float t1, HitRec &hrec) const override
        {
            HitRec temp_rec;
            bool hit_anything = false;
            float closest_so_far = t1;
            for (auto &p : m_list)
            {
                if (p->hit(r, t0, closest_so_far, temp_rec))
                {
                    hit_anything = true;
                    closest_so_far = temp_rec.t;
                    hrec = temp_rec;
                }
            }
            return hit_anything;
        }

    private:
        std::vector<ShapePtr> m_list;
    };

    class Scene
    {
    public:
        Scene(int width, int height, int samples)
            : m_image(new Image(width, height)), m_backColor(0.1f), m_samples(samples)
        {
        }

        void build()
        {
            // Camera

            vec3 lookfrom(13, 2, 3);
            vec3 lookat(0, 1, 0);
            vec3 vup(0, 1, 0);
            float aspect = float(m_image->width()) / float(m_image->height());
            m_camera = std::make_unique<Camera>(lookfrom, lookat, vup, 30, aspect);

            // Shapes

            ShapeList *world = new ShapeList();
            world->add(std::make_shared<Sphere>(
                vec3(0, 2, 0), 2,
                std::make_shared<Lambertian>(
                    std::make_shared<ColorTexture>(vec3(0.5f, 0.5f, 0.5f)))));
            world->add(std::make_shared<Sphere>(
                vec3(0, -1000, 0), 1000,
                std::make_shared<Lambertian>(
                    std::make_shared<ColorTexture>(vec3(0.8f, 0.8f, 0.8f)))));
            world->add(std::make_shared<Rect>(
                3, 5, 1, 3, -2, Rect::kXY,
                std::make_shared<DiffuseLight>(
                    std::make_shared<ColorTexture>(vec3(4)))));

            m_world.reset(world);
        }

        vec3 color(const rayt::Ray &r, const Shape *world, int depth) const
        {
            HitRec hrec;
            if (world->hit(r, 0.001f, FLT_MAX, hrec))
            {
                vec3 emitted = hrec.mat->emitted(r, hrec);
                ScatterRec srec;
                if (depth < MAX_DEPTH && hrec.mat->scatter(r, hrec, srec))
                {

                    return emitted + mulPerElem(srec.albedo, color(srec.ray, world, depth + 1));
                }
                else
                {
                    return emitted;
                }
            }
            return background(r.direction());
        }

        vec3 background(const vec3 &d) const
        {
            return m_backColor;
        }

        vec3 backgroundSky(const vec3 &d) const
        {
            vec3 v = normalize(d);
            float t = 0.5f * (v.getY() + 1.0f);
            return lerp(t, vec3(1), vec3(0.5f, 0.7f, 1.0f));
        }

        void render()
        {

            build();

            int nx = m_image->width();
            int ny = m_image->height();
#pragma omp parallel for schedule(dynamic, 1) num_threads(NUM_THREAD)
            for (int j = 0; j < ny; ++j)
            {
                std::cerr << "Rendering (y = " << j << ") " << (100.0 * j / (ny - 1)) << "%" << std::endl;
                for (int i = 0; i < nx; ++i)
                {
                    vec3 c(0);
                    for (int s = 0; s < m_samples; ++s)
                    {
                        float u = float(i + drand48()) / float(nx);
                        float v = float(j + drand48()) / float(ny);
                        Ray r = m_camera->getRay(u, v);
                        c += color(r, m_world.get(), 0);
                    }

                    c /= m_samples;
                    m_image->write(i, (ny - j - 1), c.getX(), c.getY(), c.getZ());
                }
            }

            stbi_write_bmp("render_rect_tonemap.bmp", nx, ny, sizeof(Image::rgb), m_image->pixels());
        }

    private:
        std::unique_ptr<Camera> m_camera;
        std::unique_ptr<Image> m_image;
        std::unique_ptr<Shape> m_world;
        vec3 m_backColor;
        int m_samples;
    };
}