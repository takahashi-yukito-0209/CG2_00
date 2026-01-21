#pragma once

// Forward declare global D3D12 command list type to avoid including d3d12.h here
struct ID3D12GraphicsCommandList;

// Forward declarations for runtime types used by UI context
class ParticleEmitter;
namespace MyEngine { class Object3dCommon; class Object3d; class ParticleManager; }

#include <vector>

namespace MyEngine {

class SrvManager;

class ImGuiManager {
public:
    // Initialize ImGui context and backends. Takes hwnd for Win32 init and a SrvManager for DX12 init.
    void Initialize(void* hwnd, class SrvManager* srvManager);

    // Shutdown ImGui and backends. This will call SrvManager::ShutdownImGui()
    void Shutdown();

    // Start a new ImGui frame (calls platform/renderer new-frame helpers and ImGui::NewFrame)
    void NewFrame();

    // Context passed from application to allow UI to edit runtime objects
    struct Context {
        ::ParticleEmitter* particleEmitter = nullptr; // global-scope ParticleEmitter
        MyEngine::Object3dCommon* object3dCommon = nullptr;
        std::vector<MyEngine::Object3d*>* objects3d = nullptr; // pointers to objects for per-object UI
        std::vector<class Sprite*>* sprites = nullptr; // pointers to sprites for sprite UI
        class SpriteCommon* spriteCommon = nullptr; // sprite common for blend mode
        int* selectedDrawType = nullptr; // pointer to main's selectedDrawType
        bool* useBillboard = nullptr; // pointer to main's useBillboard flag
        class ParticleManager* particleManager = nullptr; // pointer to particle manager for applying settings
        float dt = 0.0f; // delta time for updates
    };

    // Build UI elements (all ImGui:: calls moved here). This updates bound objects directly.
    void BuildUI(Context& ctx);

    // Render ImGui draw data using the provided D3D12 command list
    void Render(ID3D12GraphicsCommandList* commandList);
};
} // namespace MyEngine
