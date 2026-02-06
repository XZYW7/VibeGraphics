#include "VibeComponents.h"
#include <iostream>

// Lesson 01 的目标是创建一个窗口并初始化 DX12
// 这里先放一个简单的框架用于测试编译

const uint32_t CLIENT_WIDTH = 1280;
const uint32_t CLIENT_HEIGHT = 720;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. 注册窗口类
    const wchar_t* CLASS_NAME = L"VibeDX12Window";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // 2. 创建窗口
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Lesson 01: Hello Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CLIENT_WIDTH, CLIENT_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    std::cout << "Lesson 01 target compiled successfully!" << std::endl;

    // 3. 消息循环
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
