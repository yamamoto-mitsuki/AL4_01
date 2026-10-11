#include <MyEngine/Engine.h>

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    WindowConfig config;
    config.title = L"AL4_01";
    config.width = 1280;
    config.height = 720;
#ifdef USE_IMGUI
    config.isImGui = true;
#endif
	Engine::Initialize(config, "Test");
    while (Engine::ProcessMessage()) {
        Engine::BeginFrame();
        Engine::GetWindowManager()->UpdateAll();
        Engine::GetWindowManager()->DrawAll();
        Engine::EndFrame();
    }
    Engine::Finalize();
    return 0;
}

