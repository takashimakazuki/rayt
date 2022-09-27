#include <memory>
#include <iostream>

namespace rayt {
    class Image {
        public:
        struct rgb {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        };

        Image() : m_pixels(nullptr) {}
        Image(int w, int h) {
            m_width = w;
            m_height = h;
            m_pixels.reset(new rgb[m_width*m_height]);
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        void* pixels() const { return m_pixels.get(); }

        void write(int x, int y, float r, float g, float b) {
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
};
