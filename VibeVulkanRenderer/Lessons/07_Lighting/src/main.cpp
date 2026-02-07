#include <VibeVulkanApp.h>
#include <iostream>

// Lesson 07: Lighting
// This lesson demonstrates basic lighting models in Vulkan

using namespace Vibe;

class LightingApp : public VibeVulkanApp {
public:
    LightingApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 07: Lighting (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        
        std::cout << "Lesson 07: Lighting" << std::endl;
        std::cout << "This lesson will demonstrate basic lighting." << std::endl;
        std::cout << "Implementation: To be completed" << std::endl;
        
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        LightingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
