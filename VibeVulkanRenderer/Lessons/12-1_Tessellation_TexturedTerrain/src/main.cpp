#include <VibeVulkanApp.h>
#include <iostream>

using namespace Vibe;

class TessellationTerrainApp : public VibeVulkanApp {
public:
    TessellationTerrainApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 12-1: Tessellation Textured Terrain (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        std::cout << "Lesson 12-1: Tessellation Textured Terrain - To be implemented" << std::endl;
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        TessellationTerrainApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
