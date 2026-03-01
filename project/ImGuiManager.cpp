#include "ImGuiManager.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <d3d12.h>

#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/2d/TextureManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/particle/ParticleEmitter.h"
#include "engine/particle/ParticleManager.h"
#include <unordered_set>

using namespace MyEngine;

/// <summary>
/// ImGui::NewFrame とバックエンドの NewFrame を呼び出して新しいフレームを開始する。
/// </summary>
void ImGuiManager::NewFrame()
{
    // バックエンドの NewFrame を呼び出す
    ImGui_ImplWin32_NewFrame(); // Win32プラットフォームの新しいフレームを開始
    ImGui_ImplDX12_NewFrame(); // DX12レンダラーの新しいフレームを開始
    ImGui::NewFrame(); // ImGuiの新しいフレームを開始
}

/// <summary>
/// ImGuiとバックエンドの初期化
/// </summary>
void ImGuiManager::Initialize(void* hwnd, SrvManager* srvManager)
{

    // なかったらImGuiコンテキストを作成する（通常はアプリケーションで1回だけ呼び出される想定なので、すでに存在している場合は再利用する）
    if (ImGui::GetCurrentContext() == nullptr) {
        ImGui::CreateContext();
    }

    // バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // SrvManagerが提供されている場合は、ImGuiの初期化もSrvManagerに任せる（SRVヒープの設定などを行うため）。提供されていない場合は、ImGuiの初期化は行わない。
    if (srvManager) {
        srvManager->InitImGui(); // SrvManagerにImGuiの初期化を任せる
    }
}

/// <summary>
/// ImGuiとバックエンドのシャットダウン
/// </summary>
void ImGuiManager::Shutdown()
{
    // バックエンドのシャットダウン
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown(); // DX12レンダラーのシャットダウン
        ImGui_ImplWin32_Shutdown(); // Win32プラットフォームのシャットダウン
        ImGui::DestroyContext(); // ImGuiコンテキストの破棄
    }
}

/// <summary>
/// ImGui コントロールの構築（Context 構造体を引数にして、必要な情報を渡す）
/// </summary>
void ImGuiManager::BuildUI(Context& ctx)
{

    // メインウィンドウの開始
    ImGui::Begin("Settings");

    // 描画タイプ選択UI（例: Model, Particle, Sprite など)
    if (ctx.selectedDrawType) {
        const char* drawOptions[] = { "Model", "Particle", "Sprite", "Bunny", "Fence", "Checker", "Sphere", "All" };
        ImGui::Combo("Model", ctx.selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));
    }

    // オブジェクトごとのUI（Model, Bunny, Fence, Checker, Sphere のみ）
    if (ctx.objects3d) {
        ImGui::Separator();
        ImGui::Text("Objects");
        int sel = ctx.selectedDrawType ? *ctx.selectedDrawType : -1;

        // それぞれのオブジェクトが現在の選択に対して表示されるべきかを判定するラムダ関数
        auto isVisible = [&](int idx) -> bool {
            // sel == -1 (None) なら全て表示、sel == 7 (All) なら全て表示、それ以外は idx に対応するオブジェクトのみ表示
            if (sel == -1) {
                return true; // None なら全て表示
            }
            if (sel == 7) {
                return true; // All なら全て表示
            }

            // それ以外は idx に対応するオブジェクトのみ表示 (0=Model, 3=Bunny, 4=Fence, 5=Checker, 6=Sphere)
            switch (sel) {
            case 0:
                return idx == 0; // Model
            case 3:
                return idx == 1; // Bunny
            case 4:
                return idx == 3; // Fence
            case 5:
                return idx == 2; // Checker
            case 6:
                return idx == 4; // Sphere
            default:
                return false; // その他の選択肢ではオブジェクトは表示しない
            }
        };

        // オブジェクトごとにUIを表示
        int idx = 0;
        // それぞれのオブジェクトが現在の選択に対して表示されるべきかを判定するラムダ関数を使用して、表示すべきオブジェクトのみUIを構築する
        for (auto obj : *ctx.objects3d) {
            // obj が nullptr ならスキップ
            if (!obj) {
                ++idx;
                continue;
            }
            // 現在の選択に対してこのオブジェクトが表示されるべきかを判定
            if (!isVisible(idx)) {
                ++idx;
                continue;
            }

            ImGui::PushID(idx);
            // ヘッダにオブジェクトの種類とインデックスを表示 (例: "Object 0", "Object 1", ...)
            char header[64];
            sprintf_s(header, "Object %d", idx);
            // 現在のオブジェクトのUIを表示
            if (ImGui::CollapsingHeader(header)) {
                obj->DrawImGui(idx);
            }

            ImGui::PopID();
            ++idx;
        }
    }

    // 描画タイプ選択に基づいて、ParticleセクションとSpritesセクションの表示を制御
    int sel = ctx.selectedDrawType ? *ctx.selectedDrawType : -1;

    // Particleセクション
    if ((sel == 1 || sel == 7) && (ctx.particleEmitter || ctx.particleManager)) {

        // Particleセクション: ParticleモードまたはAllが選択されている場合に表示
        if (ImGui::CollapsingHeader("Particle")) {

            // パーティクルエミッタのUIを表示
            if (ctx.particleEmitter) {
                ctx.particleEmitter->DrawImGui();
            }

            ImGui::Separator();

            // パーティクルマネージャのUIを表示
            if (ctx.particleManager) {
                ctx.particleManager->DrawImGui();
            }

            // パーティクルグループのテクスチャ選択UIを表示
            if (ctx.particleManager) {
                const auto& groups = ctx.particleManager->GetGroups();
                // グループが存在する場合にテクスチャ選択UIを表示
                if (!groups.empty()) {
                    // ロードされたテクスチャのファイルパス一覧を取得
                    auto loaded = TextureManager::GetInstance()->GetLoadedTextureFilePaths();
                    // ロードされたテクスチャが存在する場合に、グループごとにテクスチャを選択するUIを表示
                    if (!loaded.empty()) {
                        // ロードされたテクスチャのファイルパスから、重複を排除したベースネーム（ファイル名のみ）とフルパスの対応リストを作成
                        std::vector<std::string> basenames;
                        basenames.reserve(loaded.size());
                        std::vector<std::string> fullPaths;
                        fullPaths.reserve(loaded.size());
                        std::unordered_set<std::string> seen;
                        // ロードされたテクスチャのファイルパスをループして、ベースネームとフルパスの対応リストを作成。重複するベースネームは1つだけリストに追加する。
                        for (const auto& p : loaded) {
                            // ファイルパスからベースネームを抽出
                            size_t pos = p.find_last_of("/\\");
                            std::string name = (pos == std::string::npos) ? p : p.substr(pos + 1);
                            if (seen.find(name) != seen.end())
                                continue;
                            seen.insert(name);
                            basenames.push_back(name);
                            fullPaths.push_back(p);
                        }

                        // グループごとにテクスチャを選択するUIを表示
                        if (!basenames.empty()) {
                            ImGui::Separator();
                            ImGui::Text("Particle Group Textures");
                            // グループごとにUIを表示
                            for (const auto& kv : groups) {
                                const std::string& gname = kv.first;
                                const ParticleGroup& grp = kv.second;
                                // ベースネームの配列から、ImGui::Comboで使用する const char* の配列を作成
                                std::vector<const char*> items;
                                items.reserve(basenames.size());

                                for (const auto& b : basenames) {
                                    items.push_back(b.c_str());
                                }
                                // 現在のテクスチャのベースネームに対応するインデックスを見つける。見つからない場合は0を使用する。
                                int cur = 0;
                                std::string curName;

                                // グループのテクスチャパスからベースネームを抽出して、ベースネームの配列からインデックスを見つける
                                if (!grp.texturePath.empty()) {
                                    size_t pos = grp.texturePath.find_last_of("/\\");
                                    curName = (pos == std::string::npos) ? grp.texturePath : grp.texturePath.substr(pos + 1);
                                }

                                // ベースネームの配列から、現在のテクスチャのベースネームに対応するインデックスを見つける
                                for (size_t i = 0; i < basenames.size(); ++i) {
                                    if (basenames[i] == curName) {
                                        cur = static_cast<int>(i);
                                        break;
                                    }
                                }

                                ImGui::PushID(gname.c_str());
                                ImGui::Text("%s", gname.c_str());

                                // ImGui::Comboでテクスチャを選択。選択が変更されたら、対応するフルパスを使用してグループのテクスチャを更新する。
                                if (ImGui::Combo("Texture", &cur, items.data(), static_cast<int>(items.size()))) {
                                    // 選択されたテクスチャのフルパスを取得
                                    ctx.particleManager->SetGroupTexture(gname, fullPaths[cur]);
                                }

                                ImGui::PopID();
                            }
                        }
                    }
                }
            }
        }

        ImGui::End();
    }

    // スプライトセクション
    if ((sel == 2 || sel == 7) && ctx.sprites && ctx.spriteCommon) {

        // Spriteセクション: SpriteモードまたはAllが選択されているときのみ表示
        if (ImGui::CollapsingHeader("Sprites")) {
            int sidx = 0;

            for (auto s : *ctx.sprites) {

                if (!s) {
                    ++sidx;
                    continue;
                }

                ImGui::PushID(sidx);
                char header[64];
                // ヘッダにスプライトのインデックスを表示 (例: "Sprite 0", "Sprite 1", ...)
                sprintf_s(header, "Sprite %d", sidx);
                // スプライトごとにUIを表示するためにIDをプッシュ
                if (ImGui::CollapsingHeader(header)) {

                    Vector2 pos = s->GetPosition(); // 座標を取得
                    // 座標の編集UIを表示
                    if (ImGui::DragFloat2("Position", &pos.x, 0.1f)) {
                        s->SetPosition(pos);
                    }

                    float rot = s->GetRotation(); // 回転を取得
                    // 回転の編集UIを表示
                    if (ImGui::DragFloat("Rotation", &rot, 0.01f)) {
                        s->SetRotation(rot);
                    }

                    Vector4 col = s->GetColor(); // 色の取得
                    // 色の編集UIを表示
                    if (ImGui::ColorEdit4("Color", &col.x)) {
                        s->SetColor(col);
                    }

                    Vector2 size = s->GetSize(); // 大きさの取得
                    // 大きさの編集UIを表示
                    if (ImGui::DragFloat2("Size", &size.x, 0.1f)) {
                        s->SetSize(size);
                    }

                    Vector2 anchor = s->GetAnchorPoint(); // アンカーポイントの取得
                    // アンカーポイントの編集UIを表示
                    if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
                        s->SetAnchorPoint(anchor);
                    }

                    bool fx = s->GetIsFlipX();//フリップ(X)の取得
                    bool fy = s->GetIsFlipY();//フリップ(Y)の取得
                    
                    //フリップ(X)の編集UIを表示
                    if (ImGui::Checkbox("FlipX", &fx)) {
                        s->SetIsFlipX(fx);
                    }

                    ImGui::SameLine();
                    // フリップ(Y)の編集UIを表示
                    if (ImGui::Checkbox("FlipY", &fy)) {
                        s->SetIsFlipY(fy);
                    }
                    
                    Vector2 texLT = s->GetTextureLeftTop();//テクスチャ左上座標の取得
                    Vector2 texSize = s->GetTextureSize();//テクスチャのサイズ取得
                    // テクスチャ左上座標の編集UIを表示
                    if (ImGui::DragFloat2("Tex LeftTop", &texLT.x, 1.0f)) {
                        s->SetTextureLeftTop(texLT);
                    }

                    //テクスチャサイズの編集UIを表示
                    if (ImGui::DragFloat2("Tex Size", &texSize.x, 1.0f, 1.0f, 8192.0f)) {
                        s->SetTextureSize(texSize);
                    }

                    static char texBuf[256] = "";
                    ImGui::InputText("Texture Path", texBuf, sizeof(texBuf));
                    // テクスチャサイズの変更をスプライトに反映
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
                    float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
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

    // ポイントライトのUIは平行光源の下に表示する
    if (ctx.object3dCommon) {
        if (ImGui::CollapsingHeader("Point Lights")) {
            // 最大数の点光源スロットに対して、スロットごとにUIを表示して編集できるようにする
            uint32_t maxPL = ctx.object3dCommon->GetMaxPointLights();

            for (uint32_t i = 0; i < maxPL; ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto pls = ctx.object3dCommon->GetPointLightsData();

                if (!pls) {
                    ImGui::Text("Point lights not available");
                    ImGui::PopID();
                    break;
                }

                auto& pl = pls[i];
                bool enabled = pl.enabled != 0;

                if (!enabled) {
                    ImGui::TextDisabled("Slot %d: empty", (int)i);
                    ImGui::SameLine();
                    if (ImGui::Button("Add")) {
                        Object3d::PointLight newPl = pl; // default copy
                        newPl.position = { 0.0f, 1.0f, 0.0f, 10.0f };
                        newPl.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                        newPl.enabled = 1;
                        ctx.object3dCommon->AddPointLight(newPl);
                    }
                    ImGui::PopID();
                    continue;
                }

                ImGui::Text("Point %d", (int)i);
                
                float pos3[3] = { pl.position.x, pl.position.y, pl.position.z };
                if (ImGui::DragFloat3("Position", pos3, 0.1f)) {
                    Object3d::PointLight upd = pl;
                    upd.position.x = pos3[0];
                    upd.position.y = pos3[1];
                    upd.position.z = pos3[2];
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                
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

                
                bool enabledBool = pl.enabled != 0;
                if (ImGui::Checkbox("Enabled", &enabledBool)) {
                    Object3d::PointLight upd = pl;
                    upd.enabled = enabledBool ? 1 : 0;
                    ctx.object3dCommon->UpdatePointLight(static_cast<int>(i), upd);
                }

                
                float color[4] = { pl.color.x, pl.color.y, pl.color.z, pl.color.w };
                if (ImGui::ColorEdit4("Color/Intensity", color)) {
                    Object3d::PointLight upd = pl;
                    upd.color.x = color[0];
                    upd.color.y = color[1];
                    upd.color.z = color[2];
                    upd.color.w = color[3];
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

    // スポットライトUIは点光源の下に表示する
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
                    spot->position.x = spos[0];
                    spot->position.y = spos[1];
                    spot->position.z = spos[2];
                }

                float sdir[3] = { spot->direction.x, spot->direction.y, spot->direction.z };
                if (ImGui::DragFloat3("Direction##Spot", sdir, 0.01f, -1.0f, 1.0f)) {
                    float len = sqrtf(sdir[0] * sdir[0] + sdir[1] * sdir[1] + sdir[2] * sdir[2]);
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
                float cosInner = spot->cosAngle;
                if (cosInner > 1.0f) {
                    cosInner = 1.0f;
                }

                if (cosInner < -1.0f) {
                    cosInner = -1.0f;
                }

                float cosOuter = spot->cosFalloffStart;
                if (cosOuter > 1.0f) {
                    cosOuter = 1.0f;
                }

                if (cosOuter < -1.0f) {
                    cosOuter = -1.0f;
                }

                float innerDeg = acosf(cosInner) * toDeg;
                float outerDeg = acosf(cosOuter) * toDeg;
                if (ImGui::SliderFloat("Inner Angle (deg)##Spot", &innerDeg, 0.0f, 90.0f)) {
                    
                    if (innerDeg > outerDeg) {
                        outerDeg = innerDeg;
                    }

                    spot->cosAngle = cosf(innerDeg * toRad);
                    spot->cosFalloffStart = cosf(outerDeg * toRad);
                }

                if (ImGui::SliderFloat("Outer Angle (deg)##Spot", &outerDeg, innerDeg, 90.0f)) {
                    
                    spot->cosFalloffStart = cosf(outerDeg * toRad);
                    
                    if (innerDeg > outerDeg) {
                        spot->cosAngle = spot->cosFalloffStart;
                    }
                }

                float scol[4] = { spot->color.x, spot->color.y, spot->color.z, spot->color.w };
                if (ImGui::ColorEdit4("Color/Intensity##Spot", scol)) {
                    spot->color.x = scol[0];
                    spot->color.y = scol[1];
                    spot->color.z = scol[2];
                    spot->color.w = scol[3];
                }
            }
        }
    }

    // ブレンドモードUIはライトセクションの下に表示する
    if (ImGui::CollapsingHeader("Blend Mode")) {
        // ブレンドモードの種類を列挙した配列
        const char* blendNames[] = { "None", "Alpha", "Add", "Multiply", "Screen" };

        // Object3D共通とSprite共通で別々にブレンドモードを設定できるようにするため、両方の現在のブレンドモードを取得してインデックスを初期化する
        int blendIdx = static_cast<int>(ctx.object3dCommon ? ctx.object3dCommon->GetBlendMode() : BlendMode::Alpha);

        // オブジェクト共通とスプライト共通で別々にブレンドモードを設定できるようにする
        if (ImGui::Combo("Object3D Blend", &blendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
            if (ctx.object3dCommon) {
                ctx.object3dCommon->SetBlendMode(static_cast<BlendMode>(blendIdx));
            }
        }
        
        int spriteBlendIdx = ctx.spriteCommon ? static_cast<int>(ctx.spriteCommon->GetBlendMode()) : blendIdx;
        if (ImGui::Combo("Sprite Blend", &spriteBlendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
            if (ctx.spriteCommon) {
                ctx.spriteCommon->SetBlendMode(static_cast<BlendMode>(spriteBlendIdx));
            }
        }
    }

    ImGui::End();

    // カメラのUIは常に表示する
    ImGui::Begin("Camera");
    // カメラウィンドウ（デフォルトのカメラコントロール + GPUカメラデータ）
    if (ctx.object3dCommon) {
        
        auto camData = ctx.object3dCommon->GetCameraData();
        // GPUカメラデータとデフォルトカメラの両方が存在する場合にUIを表示する
        if (camData) {
            ImGui::DragFloat3("Camera World Position", &camData->worldPosition.x, 0.1f);
        }

        
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

            
            float fovDeg = defaultCam->GetProjectionMatrix().m[0][0]; 

            static float fovY_deg = 45.0f;
            static float aspect = 16.0f / 9.0f;
            static float nearClip = 0.1f;
            static float farClip = 1000.0f;
            
            static bool init = false;
            if (!init) {
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
            
            defaultCam->Update();

            if (camData) {
                camData->worldPosition = defaultCam->GetTranslate();
            }
        }
    }
    ImGui::End();
}

/// <summary>
/// ImGui の描画コマンドを発行する。ImGui::Render とバックエンドの Render を呼び出す
/// </summary>
void ImGuiManager::Render(ID3D12GraphicsCommandList* commandList)
{
    // ImGui の描画コマンドを発行する。ImGui::Render とバックエンドの Render を呼び出す
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
