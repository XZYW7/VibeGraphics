#include <VibeVulkanApp.h>
#include <iostream>

using namespace Vibe;

class BlendingApp : public VibeVulkanApp {
public:
    BlendingApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 08: Blending (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        std::cout << "Lesson 08: Blending - To be implemented" << std::endl;
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        BlendingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
