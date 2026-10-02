#include <MyEngine/Engine.h>
#include <MyEngine/Scene/IScene.h>
#include <MyEngine/Camera/Camera.h>

// 起動確認用の空のシーン。
class GameScene : public IScene {
public:
    void Initialize() override { camera_.Initialize(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f); camera_.Update(); }
    void Update() override { camera_.Update(); }
    void Draw() override {}
    void Finalize() override {}
    Camera* GetCamera() override { return &camera_; }
private:
    Camera camera_;
};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    WindowConfig config;
    config.title = L"AL4_01";
    config.width = 1280;
    config.height = 720;
#ifdef _DEBUG
    config.isImGui = true;
#endif
    Engine::Initialize(config, [] { return std::make_unique<GameScene>(); });
    while (Engine::ProcessMessage()) {
        Engine::BeginFrame();
        Engine::GetWindowManager()->UpdateAll();
        Engine::GetWindowManager()->DrawAll();
        Engine::EndFrame();
    }
    Engine::Finalize();
    return 0;
}

