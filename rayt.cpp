#include "rayt.h"

int main()
{
    int nx = 200;
    int ny = 100;
    int ns = 100;
    std::unique_ptr<rayt::Scene> scene(new rayt::Scene(nx, ny, ns));
    scene->render();

    return 0;
}