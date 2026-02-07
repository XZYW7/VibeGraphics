#include <VibeVulkanApp.h>
#include <iostream>

using namespace Vibe;

class MaterialsApp : public VibeVulkanApp {
public:
    MaterialsApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 07-1: Materials (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        std::cout << "Lesson 07-1: Materials - To be implemented" << std::endl;
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        MaterialsApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
