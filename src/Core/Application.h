#pragma once

struct GLFWwindow;

namespace Nova {

    class Application {
    public:
        Application(int width, int height, const char* title);
        ~Application();

        void Run();

    private:
        void Init();
        void Shutdown();
        void ProcessInput();

        GLFWwindow* m_Window = nullptr;
        int m_Width, m_Height;
        const char* m_Title;
    };

} // namespace Nova