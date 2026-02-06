#pragma once

#include "VibeComponents.h"

namespace Vibe {
    class VibeApp {
    public:
        VibeApp(HINSTANCE hInstance);
        virtual ~VibeApp();

        int Run();

        virtual bool Initialize();
        virtual void OnUpdate() = 0;
        virtual void OnRender() = 0;

        // 让全局消息处理函数可以访问 protected 成员
        static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    protected:
        virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HINSTANCE m_hInstance = nullptr;
        HWND      m_hMainWnd = nullptr;
        
        static VibeApp* m_pApp;

        // 窗口属性
        int m_ClientWidth = 1280;
        int m_ClientHeight = 720;
        std::wstring m_MainWndCaption = L"Vibe DX12 App";

        bool InitMainWindow();
    };
}
