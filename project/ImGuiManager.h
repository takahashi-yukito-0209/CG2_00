#pragma once

namespace MyEngine {

class ImGuiManager {
public:
    // Start a new ImGui frame (calls platform/renderer new-frame helpers and ImGui::NewFrame)
    void NewFrame();

    // Render ImGui draw data using the provided D3D12 command list
    void Render(ID3D12GraphicsCommandList* commandList);
};
}
