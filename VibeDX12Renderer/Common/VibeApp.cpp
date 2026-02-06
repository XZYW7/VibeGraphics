#include "VibeApp.h"
#include <iostream>

namespace Vibe {

    VibeApp* VibeApp::m_pApp = nullptr;

    VibeApp::VibeApp(HINSTANCE hInstance) : m_hInstance(hInstance) {
        assert(m_pApp == nullptr);
        m_pApp = this;
    }

    VibeApp::~VibeApp() {
    }

    LRESULT CALLBACK VibeApp::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // 将消息转发给成员函数
        return VibeApp::m_pApp->MsgProc(hwnd, msg, wParam, lParam);
    }

    bool VibeApp::Initialize() {
        if (!InitMainWindow())
            return false;
        
        return true;
    }

    int VibeApp::Run() {
        MSG msg = { 0 };
        while (msg.message != WM_QUIT) {
            // 如果有窗口消息，处理它
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // 否则，执行游戏循环
            else {
                OnUpdate();
                OnRender();
            }
        }
        return (int)msg.wParam;
    }

    bool VibeApp::InitMainWindow() {
        WNDCLASS wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MainWndProc; 
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = m_hInstance;
        wc.hIcon = LoadIcon(0, IDI_APPLICATION);
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszMenuName = 0;
        wc.lpszClassName = L"VibeWndClassName";

        if (!RegisterClass(&wc)) {
            MessageBox(0, L"RegisterClass Failed.", 0, 0);
            return false;
        }

        // 计算带边框的完整窗口大小
        RECT R = { 0, 0, m_ClientWidth, m_ClientHeight };
        AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
        int width = R.right - R.left;
        int height = R.bottom - R.top;

        m_hMainWnd = CreateWindow(L"VibeWndClassName", m_MainWndCaption.c_str(),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_hInstance, 0);

        if (!m_hMainWnd) {
            MessageBox(0, L"CreateWindow Failed.", 0, 0);
            return false;
        }

        ShowWindow(m_hMainWnd, SW_SHOW);
        UpdateWindow(m_hMainWnd);

        return true;
    }

    LRESULT VibeApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
