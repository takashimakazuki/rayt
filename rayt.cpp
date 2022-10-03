#include "rayt.h"

float hit_sphere(const vec3 &center, float radius, const rayt::Ray &r)
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

vec3 color(const rayt::Ray &r)
{
    vec3 c(0, 0, -1);
    float t = hit_sphere(c, 0.5f, r);
    if (t > 0.0f)
    {
        vec3 N = normalize(r.at(t) - c);
        return 0.5f * (N + vec3(1.0f));
    }
    vec3 d = normalize(r.direction());
    t = 0.5f * (r.direction().getY() + 1.0f);
    return lerp(t, vec3(1), vec3(0.5f, 0.7f, 1.0f));
}

int main()
{
    int nx = 200;
    int ny = 100;
    std::unique_ptr<rayt::Image> image(new rayt::Image(nx, ny));

    vec3 x(4.0f, 0.0f, 0.0f);
    vec3 y(0.0f, 2.0f, 0.0f);
    vec3 z(-2.0f, -1.0f, -1.0f);
    std::unique_ptr<rayt::Camera> camera(new rayt::Camera(x, y, z));

    for (int j = 0; j < ny; ++j)
    {
        std::cerr << "Rendering (y = " << j << ") " << (100.0 * j / (ny - 1)) << "%" << std::endl;
        for (int i = 0; i < nx; ++i)
        {
            float u = float(i) / float(nx);
            float v = float(j) / float(ny);
            rayt::Ray r = camera->getRay(u, v);
            vec3 c = color(r);
            image->write(i, j, c.getX(), c.getY(), c.getZ());
        }
    }

    stbi_write_bmp("render.bmp", nx, ny, sizeof(rayt::Image::rgb), image->pixels());
    return 0;
}