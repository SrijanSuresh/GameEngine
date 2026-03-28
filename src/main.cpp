#include "Core/Application.h"
 
#include <cstdio>
#include <stdexcept>
 
int main() {
    try {
        Nova::Application app(1280, 720, "NovaEngine v0.1");
        app.Run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Fatal] %s\n", e.what());
        return 1;
    }
 
    return 0;
}