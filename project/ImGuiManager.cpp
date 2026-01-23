#include "ImGuiManager.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <d3d12.h>
// include runtime types used by UI
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Camera.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/2d/TextureManager.h"
#include <unordered_set>

using namespace MyEngine;

void ImGuiManager::NewFrame()
{
    // Platform backend must update display size/time before renderer NewFrame.
    // Call Win32 NewFrame first so ImGui gets correct io.DisplaySize and timing,
    // then call DX12 NewFrame which may create device resources if needed.
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Initialize(void* hwnd, SrvManager* srvManager)
{
    // Create ImGui context if not existing
    if (ImGui::GetCurrentContext() == nullptr)
    {
        ImGui::CreateContext();
    }

    // Initialize Win32 platform backend
    ImGui_ImplWin32_Init(hwnd);

    // Initialize DX12 renderer via SrvManager which owns SRV heap
    if (srvManager)
    {
        srvManager->InitImGui();
    }
}

void ImGuiManager::Shutdown()
{
    // Call renderer/platform shutdown through impl functions.
    // Note: SrvManager is responsible for calling ImGui_ImplDX12_Shutdown/ImGui_ImplWin32_Shutdown/ImGui::DestroyContext
    // but in case it wasn't used, perform safe shutdown here.
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

void ImGuiManager::BuildUI(Context& ctx)
{
    // Note: Particle/Sprite/BlendMode/Camera UIs are folded into the Settings window below.

    // Settings window
    ImGui::Begin("Settings");

    
    if (ctx.selectedDrawType) {
        const char* drawOptions[] = {"Model","Particle","Sprite","Bunny","Fence","Checker","Sphere","All"};
        ImGui::Combo("Model", ctx.selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));
    }
    // per-object UI: only allow editing objects that are currently displayed
    if (ctx.objects3d) {
        ImGui::Separator();
        ImGui::Text("Objects");
        int sel = ctx.selectedDrawType ? *ctx.selectedDrawType : -1;
        auto isVisible = [&](int idx)->bool {
            // Mapping must match the DrawType order used by the app:
            // 0: Model (plane -> object index 0)
            // 1: Particle (no object edits)
            // 2: Sprite (no object edits)
            // 3: Bunny (object index 1)
            // 4: Fence (object index 3)
            // 5: Checker (object index 2)
            // 6: Sphere (object index 4)
            // 7: All
            if (sel == -1) return true; // no selection provided -> show all
            if (sel == 7) return true; // All
            switch (sel) {
            case 0: return idx == 0; // Model
            case 3: return idx == 1; // Bunny
            case 4: return idx == 3; // Fence
            case 5: return idx == 2; // Checker
            case 6: return idx == 4; // Sphere
            default: return false; // Particle, Sprite, or unknown -> no object UI
            }
        };

        int idx = 0;
        for (auto obj : *ctx.objects3d) {
            if (!obj) { ++idx; continue; }
            if (!isVisible(idx)) { ++idx; continue; }
            ImGui::PushID(idx);
            // Use index-specific header so multiple shown objects are distinguishable
            char header[64];
            sprintf_s(header, "Object %d", idx);
            if (ImGui::CollapsingHeader(header)) {
                obj->DrawImGui(idx);
            }
            ImGui::PopID();
            ++idx;
        }
    }

    // current selection (matches main.cpp DrawType enum)
    int sel = ctx.selectedDrawType ? *ctx.selectedDrawType : -1;
    // Particle section: show only when Particle mode or All is selected
    if ((sel == 1 || sel == 7) && (ctx.particleEmitter || ctx.particleManager)) {
        if (ImGui::CollapsingHeader("Particle")) {
            if (ctx.particleEmitter)
                ctx.particleEmitter->DrawImGui();
            ImGui::Separator();
            if (ctx.particleManager)
                ctx.particleManager->DrawImGui();

            // Allow switching particle group textures from loaded textures
            if (ctx.particleManager) {
                const auto& groups = ctx.particleManager->GetGroups();
                if (!groups.empty()) {
                    auto loaded = TextureManager::GetInstance()->GetLoadedTextureFilePaths();
                    if (!loaded.empty()) {
                        // deduplicate by basename (no directory part) and keep mapping to first full path
                        std::vector<std::string> basenames; basenames.reserve(loaded.size());
                        std::vector<std::string> fullPaths; fullPaths.reserve(loaded.size());
                        std::unordered_set<std::string> seen;
                        for (const auto& p : loaded) {
                            // extract filename only
                            size_t pos = p.find_last_of("/\\");
                            std::string name = (pos == std::string::npos) ? p : p.substr(pos + 1);
                            if (seen.find(name) != seen.end()) continue;
                            seen.insert(name);
                            basenames.push_back(name);
                            fullPaths.push_back(p);
                        }

                        if (!basenames.empty()) {
                            ImGui::Separator();
                            ImGui::Text("Particle Group Textures");
                            for (const auto& kv : groups) {
                                const std::string& gname = kv.first;
                                const ParticleGroup& grp = kv.second;
                                // build item list of c_str
                                std::vector<const char*> items; items.reserve(basenames.size());
                                for (const auto& b : basenames) items.push_back(b.c_str());
                                // find current by comparing basename of group's texturePath
                                int cur = 0;
                                std::string curName;
                                if (!grp.texturePath.empty()) {
                                    size_t pos = grp.texturePath.find_last_of("/\\");
                                    curName = (pos == std::string::npos) ? grp.texturePath : grp.texturePath.substr(pos + 1);
                                }
                                for (size_t i = 0; i < basenames.size(); ++i) {
                                    if (basenames[i] == curName) { cur = static_cast<int>(i); break; }
                                }
                                ImGui::PushID(gname.c_str());
                                ImGui::Text("%s", gname.c_str());
                                if (ImGui::Combo("Texture", &cur, items.data(), static_cast<int>(items.size()))) {
                                    // apply change using mapped full path
                                    ctx.particleManager->SetGroupTexture(gname, fullPaths[cur]);
                                }
                                ImGui::PopID();
                            }
                        }
                        }
                }
            }
        }
    }

    // Sprites section
    // Sprites section: show only when Sprite mode or All is selected
    if ((sel == 2 || sel == 7) && ctx.sprites && ctx.spriteCommon) {
        if (ImGui::CollapsingHeader("Sprites")) {
            int sidx = 0;
            for (auto s : *ctx.sprites) {
                if (!s) {
                    ++sidx;
                    continue;
                }
                ImGui::PushID(sidx);
                char header[64];
                sprintf_s(header, "Sprite %d", sidx);
                if (ImGui::CollapsingHeader(header)) {
                    Vector2 pos = s->GetPosition();
                    if (ImGui::DragFloat2("Position", &pos.x, 0.1f))
                        s->SetPosition(pos);
                    float rot = s->GetRotation();
                    if (ImGui::DragFloat("Rotation", &rot, 0.01f))
                        s->SetRotation(rot);
                    Vector4 col = s->GetColor();
                    if (ImGui::ColorEdit4("Color", &col.x))
                        s->SetColor(col);
                    Vector2 size = s->GetSize();
                    if (ImGui::DragFloat2("Size", &size.x, 0.1f))
                        s->SetSize(size);
                    Vector2 anchor = s->GetAnchorPoint();
                    if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f))
                        s->SetAnchorPoint(anchor);
                    bool fx = s->GetIsFlipX();
                    bool fy = s->GetIsFlipY();
                    if (ImGui::Checkbox("FlipX", &fx))
                        s->SetIsFlipX(fx);
                    ImGui::SameLine();
                    if (ImGui::Checkbox("FlipY", &fy))
                        s->SetIsFlipY(fy);
                    Vector2 texLT = s->GetTextureLeftTop();
                    Vector2 texSize = s->GetTextureSize();
                    if (ImGui::DragFloat2("Tex LeftTop", &texLT.x, 1.0f))
                        s->SetTextureLeftTop(texLT);
                    if (ImGui::DragFloat2("Tex Size", &texSize.x, 1.0f, 1.0f, 8192.0f))
                        s->SetTextureSize(texSize);
                    static char texBuf[256] = "";
                    ImGui::InputText("Texture Path", texBuf, sizeof(texBuf));
                    if (ImGui::Button("Apply Texture")) {
                        s->SetTexture(std::string(texBuf));
                    }
                }
                ImGui::PopID();
                ++sidx;
            }
        }
    }

    // ライト編集（Object3dCommonが所有する共有の平行光源）
    if (ctx.object3dCommon) {
        auto light = ctx.object3dCommon->GetDirectionalLightData();
        if (light) {
            if (ImGui::CollapsingHeader("Light")) {
                float color[4] = { light->color.x, light->color.y, light->color.z, light->color.w };
                if (ImGui::ColorEdit4("Color", color)) {
                    light->color.x = color[0];
                    light->color.y = color[1];
                    light->color.z = color[2];
                    light->color.w = color[3];
                }

                // 方向ベクトル: 編集後に正規化して反映する
                float dir[3] = { light->direction.x, light->direction.y, light->direction.z };
                if (ImGui::SliderFloat3("Direction", dir, -1.0f, 1.0f)) {
                    float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
                    if (len > 1e-6f) {
                        light->direction.x = dir[0] / len;
                        light->direction.y = dir[1] / len;
                        light->direction.z = dir[2] / len;
                    }
                }

                // 強度（輝度）をスライダーで制御
                ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 10.0f, "%.2f");
            }
        }
    }

    // Point lights UI
    if (ctx.object3dCommon) {
        if (ImGui::CollapsingHeader("Point Lights")) {
            // show existing lights and allow add/remove/update
            uint32_t maxPL = ctx.object3dCommon->GetMaxPointLights();
            for (uint32_t i = 0; i < maxPL; ++i) {
                ImGui::PushID(static_cast<int>(i));
                // Access raw array
                auto pls = ctx.object3dCommon->GetPointLightsData();
                if (!pls) { ImGui::Text("Point lights not available"); ImGui::PopID(); break; }
                auto& pl = pls[i];
                bool enabled = pl.enabled != 0;
                if (!enabled) {
                    ImGui::TextDisabled("Slot %d: empty", (int)i);
                    ImGui::SameLine();
                    if (ImGui::Button("Add")) {
                        Object3d::PointLight newPl = pl; // default copy
                        newPl.position = {0.0f, 1.0f, 0.0f, 10.0f};
                        newPl.color = {1.0f,1.0f,1.0f,1.0f};
                        newPl.enabled = 1;
                        ctx.object3dCommon->AddPointLight(newPl);
                    }
                    ImGui::PopID();
                    continue;
                }

                ImGui::Text("Point %d", (int)i);
                // Position (xyz)
                float pos3[3] = { pl.position.x, pl.position.y, pl.position.z };
                if (ImGui::DragFloat3("Position", pos3, 0.1f)) {
                    Object3d::PointLight upd = pl;
                    upd.position.x = pos3[0]; upd.position.y = pos3[1]; upd.position.z = pos3[2];
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                // Radius (range) and decay (falloff exponent)
                float radius = pl.radius;
                if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.01f, 10000.0f)) {
                    Object3d::PointLight upd = pl;
                    upd.radius = radius;
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }
                float decay = pl.decay;
                if (ImGui::DragFloat("Decay", &decay, 0.1f, 0.0f, 10.0f)) {
                    Object3d::PointLight upd = pl;
                    upd.decay = decay;
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                // Enabled toggle
                bool enabledBool = pl.enabled != 0;
                if (ImGui::Checkbox("Enabled", &enabledBool)) {
                    Object3d::PointLight upd = pl;
                    upd.enabled = enabledBool ? 1 : 0;
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                // Color + intensity
                float color[4] = { pl.color.x, pl.color.y, pl.color.z, pl.color.w };
                if (ImGui::ColorEdit4("Color/Intensity", color)) {
                    Object3d::PointLight upd = pl;
                    upd.color.x = color[0]; upd.color.y = color[1]; upd.color.z = color[2]; upd.color.w = color[3];
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                if (ImGui::Button("Remove")) {
                    ctx.object3dCommon->RemovePointLight(static_cast<int>(i));
                }
                ImGui::Separator();
                ImGui::PopID();
            }
        }
    }

    // Spot Light UI (top-level) - always available like Point Lights
    if (ctx.object3dCommon) {
        if (ImGui::CollapsingHeader("Spot Light")) {
            auto spot = ctx.object3dCommon->GetSpotLightData();
            if (!spot) {
                ImGui::TextDisabled("Spot light not available");
            } else {
                bool enabled = spot->enabled != 0;
                if (ImGui::Checkbox("Enabled##Spot", &enabled)) {
                    spot->enabled = enabled ? 1 : 0;
                }

                float spos[3] = { spot->position.x, spot->position.y, spot->position.z };
                if (ImGui::DragFloat3("Position##Spot", spos, 0.1f)) {
                    spot->position.x = spos[0]; spot->position.y = spos[1]; spot->position.z = spos[2];
                }

                float sdir[3] = { spot->direction.x, spot->direction.y, spot->direction.z };
                if (ImGui::DragFloat3("Direction##Spot", sdir, 0.01f, -1.0f, 1.0f)) {
                    float len = sqrtf(sdir[0]*sdir[0] + sdir[1]*sdir[1] + sdir[2]*sdir[2]);
                    if (len > 1e-6f) {
                        spot->direction.x = sdir[0] / len;
                        spot->direction.y = sdir[1] / len;
                        spot->direction.z = sdir[2] / len;
                    }
                }

                float distance = spot->distance;
                if (ImGui::DragFloat("Distance##Spot", &distance, 0.1f, 0.01f, 100000.0f)) {
                    spot->distance = distance;
                }
                float decay = spot->decay;
                if (ImGui::DragFloat("Decay##Spot", &decay, 0.01f, 0.0f, 10.0f)) {
                    spot->decay = decay;
                }

                const float toDeg = 180.0f / 3.14159265f;
                const float toRad = 3.14159265f / 180.0f;
                float cosInner = spot->cosAngle; if (cosInner > 1.0f) cosInner = 1.0f; if (cosInner < -1.0f) cosInner = -1.0f;
                float cosOuter = spot->cosFalloffStart; if (cosOuter > 1.0f) cosOuter = 1.0f; if (cosOuter < -1.0f) cosOuter = -1.0f;
                float innerDeg = acosf(cosInner) * toDeg;
                float outerDeg = acosf(cosOuter) * toDeg;
                if (ImGui::SliderFloat("Inner Angle (deg)##Spot", &innerDeg, 0.0f, 90.0f)) {
                    if (innerDeg > outerDeg) outerDeg = innerDeg;
                    spot->cosAngle = cosf(innerDeg * toRad);
                    spot->cosFalloffStart = cosf(outerDeg * toRad);
                }
                if (ImGui::SliderFloat("Outer Angle (deg)##Spot", &outerDeg, innerDeg, 90.0f)) {
                    spot->cosFalloffStart = cosf(outerDeg * toRad);
                    if (innerDeg > outerDeg) spot->cosAngle = spot->cosFalloffStart;
                }

                float scol[4] = { spot->color.x, spot->color.y, spot->color.z, spot->color.w };
                if (ImGui::ColorEdit4("Color/Intensity##Spot", scol)) {
                    spot->color.x = scol[0]; spot->color.y = scol[1]; spot->color.z = scol[2]; spot->color.w = scol[3];
                }
            }
        }
    }

    // Blend Mode section
    if (ImGui::CollapsingHeader("Blend Mode")) {
        const char* blendNames[] = { "None", "Alpha", "Add", "Multiply", "Screen" };
        int blendIdx = static_cast<int>(ctx.object3dCommon ? ctx.object3dCommon->GetBlendMode() : BlendMode::Alpha);
        if (ImGui::Combo("Object3D Blend", &blendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
            if (ctx.object3dCommon)
                ctx.object3dCommon->SetBlendMode(static_cast<BlendMode>(blendIdx));
        }
        int spriteBlendIdx = ctx.spriteCommon ? static_cast<int>(ctx.spriteCommon->GetBlendMode()) : blendIdx;
        if (ImGui::Combo("Sprite Blend", &spriteBlendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
            if (ctx.spriteCommon)
                ctx.spriteCommon->SetBlendMode(static_cast<BlendMode>(spriteBlendIdx));
        }
    }

    ImGui::End();

    // Camera window (default camera controls + GPU camera data)
    ImGui::Begin("Camera");
    if (ctx.object3dCommon) {
        // Edit GPU camera world position used for billboards/specular
        auto camData = ctx.object3dCommon->GetCameraData();
        if (camData) {
            ImGui::DragFloat3("Camera World Position", &camData->worldPosition.x, 0.1f);
        }

        // Edit default Camera if available
        auto defaultCam = ctx.object3dCommon->GetDefaultCamera();
        if (defaultCam) {
            Vector3 camPos = defaultCam->GetTranslate();
            if (ImGui::DragFloat3("Position", &camPos.x, 0.1f)) {
                defaultCam->SetTranslate(camPos);
            }

            Vector3 camRot = defaultCam->GetRotate();
            float rotX = camRot.x * 180.0f / 3.14159265f;
            float rotY = camRot.y * 180.0f / 3.14159265f;
            float rotZ = camRot.z * 180.0f / 3.14159265f;
            bool changed = false;
            changed |= ImGui::SliderAngle("Rotation X", &rotX);
            changed |= ImGui::SliderAngle("Rotation Y", &rotY);
            changed |= ImGui::SliderAngle("Rotation Z", &rotZ);
            if (changed) {
                camRot.x = rotX * 3.14159265f / 180.0f;
                camRot.y = rotY * 3.14159265f / 180.0f;
                camRot.z = rotZ * 3.14159265f / 180.0f;
                defaultCam->SetRotate(camRot);
            }

            // Projection params
            // Show FOV in degrees for clarity
            float fovDeg = defaultCam->GetProjectionMatrix().m[0][0]; // placeholder: cannot extract easily
            // Instead, provide an editable field that maps to SetFovY (radians). We'll read/write using GetProjectionMatrix is complex,
            // so store a temporary and only set when user edits via GetProjectionMatrix fallback not available.
            // Use heuristics: expose controls for FOV/Aspect/Near/Far via getters where available. Camera has no GetFov, so keep editable fields minimal.
            // Provide sliders for aspect/near/far using current camera projection by keeping local defaults.
            static float fovY_deg = 45.0f;
            static float aspect = 16.0f/9.0f;
            static float nearClip = 0.1f;
            static float farClip = 1000.0f;
            // Initialize statics from camera on first open
            static bool init = false;
            if (!init) {
                // attempt to approximate: use stored values in Camera by temporarily querying via private access is not possible.
                // Fallback to reasonable defaults.
                init = true;
            }
            if (ImGui::DragFloat("FOV (deg)", &fovY_deg, 0.1f, 1.0f, 179.0f)) {
                defaultCam->SetFovY(fovY_deg * 3.14159265f / 180.0f);
            }
            if (ImGui::DragFloat("Aspect", &aspect, 0.01f, 0.1f, 10.0f)) {
                defaultCam->SetAspectRatio(aspect);
            }
            if (ImGui::DragFloat("Near", &nearClip, 0.01f, 0.001f, 100.0f)) {
                defaultCam->SetNearClip(nearClip);
            }
            if (ImGui::DragFloat("Far", &farClip, 1.0f, 10.0f, 100000.0f)) {
                defaultCam->SetFarClip(farClip);
            }

            // Apply update if any camera parameter changed
            defaultCam->Update();

            // Mirror default camera world position into GPU camera data for other systems
            if (camData) {
                camData->worldPosition = defaultCam->GetTranslate();
            }
        }
    }
    ImGui::End();
}

void ImGuiManager::Render(::ID3D12GraphicsCommandList* commandList)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

