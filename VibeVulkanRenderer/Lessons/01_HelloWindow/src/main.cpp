#include "VibeComponents.h"
#include <iostream>

// Lesson 01: Create a window using Win32 API
// This lesson focuses on window creation before introducing Vulkan

const uint32_t CLIENT_WIDTH = 1280;
const uint32_t CLIENT_HEIGHT = 720;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. Register window class
    const wchar_t* CLASS_NAME = L"VibeVulkanWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // 2. Create window
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Lesson 01: Hello Window (Vulkan)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CLIENT_WIDTH, CLIENT_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, L"Failed to create window", L"Error", MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    std::cout << "Lesson 01: Hello Window" << std::endl;
    std::cout << "Window created successfully!" << std::endl;
    std::cout << "Press ESC to close the window." << std::endl;

    // 3. Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
