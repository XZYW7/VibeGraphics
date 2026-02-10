#include <VibeVulkanApp.h>
#include <iostream>

// Lesson 05: Textures
// This lesson demonstrates texture loading and mapping in Vulkan

using namespace Vibe;

class TexturesApp : public VibeVulkanApp {
public:
    TexturesApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 05: Textures (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        
        std::cout << "Lesson 05: Textures" << std::endl;
        std::cout << "This lesson will demonstrate texture mapping." << std::endl;
        std::cout << "Implementation: To be completed" << std::endl;
        
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        TexturesApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
