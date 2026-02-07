#include <VibeVulkanApp.h>
#include <iostream>

using namespace Vibe;

class TexturedLightingApp : public VibeVulkanApp {
public:
    TexturedLightingApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 07-2: Textured Lighting (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        std::cout << "Lesson 07-2: Textured Lighting - To be implemented" << std::endl;
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        TexturedLightingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
