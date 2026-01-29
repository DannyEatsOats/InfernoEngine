#pragma once
#include "pch.h"
#include "Events/Event.h"
#include "GLFW/glfw3.h"
#include "Renderer/RenderingContext.h"

namespace Inferno {
    struct WindowProperties {
        std::string Title;
        uint32_t Width, Height;

        WindowProperties(std::string title = "Engine", const uint32_t width = 1920, const uint32_t height = 1080)
            : Title(std::move(title)), Width(width), Height(height) {
        }
    };

    class Window {
    public:
        using EventCallbackFn = std::function<void(Event &)>;

        Window(const WindowProperties &properties);
        virtual ~Window();

        virtual void OnUpdate();

        virtual uint32_t GetWidth() const;

        virtual uint32_t GetHeight() const;

        virtual inline GLFWwindow* GetNativeWindow() const {return m_Window;}

        virtual void SetEventCallback(const EventCallbackFn &callback);

        virtual void SetVSync(bool enabled);

        virtual bool IsVSync() const;

        static Window *Create(const WindowProperties &properties = WindowProperties());

    private:
        void Init(const WindowProperties &windownProperties);

        void Shutdown();

    private:
        GLFWwindow *m_Window;
        RenderingContext* m_Context;

        struct WindowData {
            std::string Title;
            uint32_t Width, Height;
            bool VSync;

            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
    };
}
