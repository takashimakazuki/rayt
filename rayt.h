#include <memory>
#include <iostream>
#include <random>
#include <float.h> // FLT_MIN, FLT_MAX

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

namespace rayt
{
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
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        void *pixels() const { return m_pixels.get(); }

        void write(int x, int y, float r, float g, float b)
        {
            int index = m_width * y + x;
            m_pixels[index].r = static_cast<unsigned char>(r * 255.99f);
            m_pixels[index].g = static_cast<unsigned char>(g * 255.99f);
            m_pixels[index].b = static_cast<unsigned char>(b * 255.99f);
        }

    private:
        int m_width;
        int m_height;
        std::unique_ptr<rgb[]> m_pixels;
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

    class Scene
    {
    public:
        Scene(int width, int height)
            : m_image(new Image(width, height)), m_backColor(0.2f)
        {
        }

        void build()
        {
            vec3 w(-2.0f, -1.0f, -1.0f);
            vec3 u(4.0f, 0.0f, 0.0f);
            vec3 v(0.0f, 2.0f, 0.0f);
            m_camera = std::unique_ptr<Camera>(new Camera(u, v, w));
        }

        float hit_sphere(const vec3 &center, float radius, const Ray &r) const
        {
            vec3 oc = r.origin() - center;
            float a = dot(r.direction(), r.direction());
            float b = 2.0f * dot(r.direction(), oc);
            float c = dot(oc, oc) - pow2(radius);
            // Discriminant that distinguish if the simultaneous equation between ray equation and sphere equation has 2 answers.
            // → conflict detection with ray and sphere
            float D = b * b - 4 * a * c;
            if (D < 0)
            {
                return -1.0f;
            }
            else
            {
                return (-b - sqrtf(D)) / (2.0f * a);
            }
        }

        vec3 color(const Ray &r)
        {
            vec3 c(0, 0, -1);
            float t = hit_sphere(c, 0.5f, r);
            if (t > 0.0f)
            {
                vec3 N = normalize(r.at(t) - c);
                return 0.5f * (N + vec3(1.0f));
            }
            return backgroundSky(r.direction());
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
            for (int j = 0; j < ny; ++j)
            {
                std::cerr << "Rendering (y = " << j << ") " << (100.0 * j / (ny - 1)) << "%" << std::endl;
                for (int i = 0; i < nx; ++i)
                {

                    float u = float(i + drand48()) / float(nx);
                    float v = float(j + drand48()) / float(ny);
                    Ray r = m_camera->getRay(u, v);
                    vec3 c = color(r);
                    m_image->write(i, (ny - j - 1), c.getX(), c.getY(), c.getZ());
                }
            }

            stbi_write_bmp("render.bmp", nx, ny, sizeof(Image::rgb), m_image->pixels());
        }

    private:
        std::unique_ptr<Camera> m_camera;
        std::unique_ptr<Image> m_image;
        vec3 m_backColor;
    };
};
