#pragma once

namespace MyEngine {
class DirectXCommon;

class ModelCommon {
public:
    ModelCommon() = default;
    void Initialize(DirectXCommon* dxCommon) { dxCommon_ = dxCommon; }
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
};

} // namespace MyEngine
