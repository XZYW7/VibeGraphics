#include <VibeVulkanApp.h>
#include <iostream>

// Lesson 04: Depth Buffering
// This lesson demonstrates depth testing in Vulkan

using namespace Vibe;

class DepthBufferingApp : public VibeVulkanApp {
public:
    DepthBufferingApp(HINSTANCE hInstance) : VibeVulkanApp(hInstance) {
        m_MainWndCaption = L"Lesson 04: Depth Buffering (Vulkan)";
    }

    virtual bool Initialize() override {
        if (!VibeVulkanApp::Initialize()) return false;
        
        std::cout << "Lesson 04: Depth Buffering" << std::endl;
        std::cout << "This lesson will demonstrate depth testing." << std::endl;
        std::cout << "Implementation: To be completed" << std::endl;
        
        // TODO: Implement depth buffer creation and usage
        // - Modify render pass to include depth attachment
        // - Enable depth testing in pipeline
        // - Render overlapping geometry to demonstrate depth testing
        
        return true;
    }

    virtual void OnUpdate() override {}
    virtual void OnRender() override {
        // TODO: Implement rendering with depth testing
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        DepthBufferingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
