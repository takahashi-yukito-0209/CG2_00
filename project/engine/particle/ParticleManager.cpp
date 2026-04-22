#include "ParticleManager.h"
#include "engine/utility/Logger.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/Camera.h"
#include <algorithm>
#include <vector>
#include "ImGuiManager.h"

using namespace Math;
using namespace MyEngine;

/// <summary>
/// 初期化
/// </summary>
void ParticleManager::Initialize(DirectXCommon* dx, Object3dCommon* objCommon, SrvManager* srv, TextureManager* texMgr, ImGuiManager* imguiManager)
{
    // 参照の保存
    dxCommon_ = dx;
    object3dCommon_ = objCommon;
    srvManager_ = srv;
    texManager_ = texMgr;
#ifdef USE_IMGUI
    if (imguiManager) {
        imguiManager_ = imguiManager;
    }
#else
    (void)imguiManager;
#endif
}

void ParticleManager::Finalize()
{
    // ImGuiコールバックの登録解除
    particleGroups_.clear();
}

/// <summary>
/// 既存グループにテクスチャを割り当て
/// </summary>
void ParticleManager::SetGroupTexture(const std::string& name, const std::string& textureFilePath)
{
    // グループ名で検索
    auto it = particleGroups_.find(name);
    // 見つからなければ終了
    if (it == particleGroups_.end()) return;
    // テクスチャパスを更新
    it->second.texturePath = textureFilePath;

    // テクスチャを確実にロードしてSRVインデックスを記録
    if (texManager_) {
        // すでにロードされているか確認
        uint32_t idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        // ロードされていなければロードしてインデックスを取得
        if (idx == UINT32_MAX) {
            // ロードしてリソース転送を実行
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        // SRVインデックスをグループデータに保存
        it->second.srvIndex = (idx == UINT32_MAX) ? 0u : idx;
    }
}

/// <summary>
/// 描画に使用するプレーン（Object3d）を設定
/// </summary>
void ParticleManager::SetParticlePlane(MyEngine::Object3d* plane)
{
    // プレーンモデルの参照を保存
    particlePlane_ = plane;
}

/// <summary>
/// 新しいパーティクルグループを作成
/// </summary>
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
    // グループ名が空なら作成しない
    if (name.empty()) { return; }
    
    // すでに存在するグループ名なら上書き
    auto& grp = particleGroups_[name];
    
    // テクスチャパスを保存
    grp.texturePath = textureFilePath;

    // テクスチャを確実にロードしてSRVインデックスを記録
    if (texManager_) {
        // すでにロードされているか確認
        uint32_t idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        // ロードされていなければロードしてインデックスを取得
        if (idx == UINT32_MAX) {
            // ロードしてリソース転送を実行
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        // SRVインデックスをグループデータに保存
        grp.srvIndex = (idx == UINT32_MAX) ? 0u : idx;
    }
}

/// <summary>
///  指定グループからパーティクルを生成
/// </summary>
void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    // グループ名で検索
    auto it = particleGroups_.find(name);

    // 見つからなければ終了
    if (it == particleGroups_.end()) { return; }

    // 生成するパーティクルのリストへの参照
    auto& list = it->second.particles;

    // ランダム分布の定義
    std::uniform_real_distribution<float> lifeDist(lifeMin_, lifeMax_); // 寿命分布
    std::uniform_real_distribution<float> rx(spawnPosMin_.x, spawnPosMax_.x); // 発生位置の分布
    std::uniform_real_distribution<float> ry(spawnPosMin_.y, spawnPosMax_.y); // 発生位置の分布
    std::uniform_real_distribution<float> rz(spawnPosMin_.z, spawnPosMax_.z); // 発生位置の分布
    std::uniform_real_distribution<float> rvx(velMin_.x, velMax_.x); // 初速の分布
    std::uniform_real_distribution<float> rvy(velMin_.y, velMax_.y); // 初速の分布
    std::uniform_real_distribution<float> rvz(velMin_.z, velMax_.z); // 初速の分布
    std::uniform_real_distribution<float> rsx(scaleMin_.x, scaleMax_.x); // スケールの分布
    std::uniform_real_distribution<float> rsy(scaleMin_.y, scaleMax_.y); // スケールの分布
    std::uniform_real_distribution<float> rsz(scaleMin_.z, scaleMax_.z); // スケールの分布
    std::uniform_real_distribution<float> rcx(colMin_.x, colMax_.x); // カラーの分布
    std::uniform_real_distribution<float> rcy(colMin_.y, colMax_.y); // カラーの分布
    std::uniform_real_distribution<float> rcz(colMin_.z, colMax_.z); // カラーの分布
    std::uniform_real_distribution<float> rca(colMin_.w, colMax_.w); // カラーの分布

    // パーティクルの生成
    for (uint32_t i = 0; i < count; ++i) {
        // パーティクルデータの生成
        PM_CpuParticle p{};
        // ランダムなスケール、回転（今回は固定）、位置、速度、カラーを設定
        p.transform.scale = { rsx(rng_), rsy(rng_), rsz(rng_) };
        p.transform.rotate = { 0.0f, 0.0f, 0.0f };
        p.transform.translate = { position.x + rx(rng_), position.y + ry(rng_), position.z + rz(rng_) };
        p.velocity = { rvx(rng_), rvy(rng_), rvz(rng_) };
        p.color = { rcx(rng_), rcy(rng_), rcz(rng_), rca(rng_) };

        // 寿命をランダムに設定
        float life = lifeDist(rng_);
        
        // 寿命が極端に短くならないように最低値を設定
        if (life < 0.01f) {
            life = 0.01f;
        }

        // パーティクルの寿命と生成時間を設定
        p.lifeTime = life;
        p.currentTime = 0.0f;
        p.spawnTime = globalTime_;

        // 生成したパーティクルをリストに追加
        list.push_back(p);
    }
}

/// <summary>
/// パーティクルを更新する（位置・寿命・物理簡易処理）
/// </summary>
void ParticleManager::Update(float dt)
{
    // 経過時間を加算
    globalTime_ += dt;

    // すべてのグループのすべてのパーティクルを更新
    for (auto& kv : particleGroups_) {
        // パーティクルのリストへの参照
        auto& plist = kv.second.particles;
        
        // イテレータを使ってリストを走査し、寿命切れのパーティクルを削除
        for (auto it = plist.begin(); it != plist.end(); ) {
            PM_CpuParticle& p = *it;
            // フィールド適用（AABB内）
            if (fieldEnabled_) {
                const Vector3& pos = p.transform.translate;
                if (pos.x >= fieldMin_.x && pos.y >= fieldMin_.y && pos.z >= fieldMin_.z &&
                    pos.x <= fieldMax_.x && pos.y <= fieldMax_.y && pos.z <= fieldMax_.z) {
                    p.velocity.x += fieldAccel_.x * dt;
                    p.velocity.y += fieldAccel_.y * dt;
                    p.velocity.z += fieldAccel_.z * dt;
                }
            }
            // 重力
            if (gravityEnabled_) {
                p.velocity.x += gravity_.x * dt;
                p.velocity.y += gravity_.y * dt;
                p.velocity.z += gravity_.z * dt;
            }
            // 減衰（一次減衰）
            if (damping_ > 0.0f) {
                float k = damping_ * dt;
                p.velocity.x *= (1.0f - k);
                p.velocity.y *= (1.0f - k);
                p.velocity.z *= (1.0f - k);
            }
            // 位置更新
            p.transform.translate.x += p.velocity.x * dt;
            p.transform.translate.y += p.velocity.y * dt;
            p.transform.translate.z += p.velocity.z * dt;

            // 経過時間
            p.currentTime += dt;
            if (p.currentTime >= p.lifeTime) {
                it = plist.erase(it);
            } else {
                ++it;
            }
        }
    }
}

/// <summary>
/// パーティクルを描画
/// </summary>
void ParticleManager::Draw()
{
    // 必要な参照が揃っているか確認
    if (!dxCommon_ || !object3dCommon_ || !particlePlane_) { return; }

    // PSO/RS 切り替え（パーティクル用）
    object3dCommon_->SetInstancingDrawSetting();

    // カメラ取得（非ビルボード時のWVP計算用）
    Camera* cam = object3dCommon_->GetDefaultCamera();
    Matrix4x4 view = cam ? cam->GetViewMatrix() : Matrix4x4();
    Matrix4x4 proj = cam ? cam->GetProjectionMatrix() : Matrix4x4();

    // インスタンシングバッファ
    auto instData = object3dCommon_->GetInstancingData();
    const uint32_t instSlots = object3dCommon_->GetInstancingSlotCount();
    if (!instData || instSlots == 0) { return; }

    // すべてのグループのパーティクルを描画
    for (auto& kv : particleGroups_) {
        auto& grp = kv.second;
        // 描画個数を決定
        uint32_t count = static_cast<uint32_t>(grp.particles.size());
        // インスタンススロット数を超える場合は描画数を制限
        if (count == 0) { continue; }
        // 描画数をインスタンススロット数に制限
        count = std::min<uint32_t>(count, instSlots);

        // 安定ソート用に参照配列を作る
        std::vector<std::reference_wrapper<const PM_CpuParticle>> sorted;
        // 参照配列にパーティクルを追加
        sorted.reserve(grp.particles.size());
        // 生成時間で安定ソート（同一フレーム生成のパーティクルは元の順序を保つ）
        for (const auto& p : grp.particles) { sorted.emplace_back(std::cref(p)); }
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.get().spawnTime < b.get().spawnTime;
        });

        // パーティクルプレーンにテクスチャ適用
        if (!grp.texturePath.empty()) {
            particlePlane_->SetTexture(grp.texturePath);
        }

        // インスタンスデータ転送
        for (uint32_t i = 0; i < count; ++i) {
            const auto& pt = sorted[i].get();
            Transform tr = pt.transform;
            // ZオフセットでZ Fighting軽減
            tr.translate.z += static_cast<float>(i) * 1e-3f;
            // ワールド行列の作成
            Matrix4x4 world = MathUtil::MakeAffineMatrix(tr.scale, tr.rotate, tr.translate);
            // WVP行列の作成
            Matrix4x4 wvp = MathUtil::Multiply(world, MathUtil::Multiply(view, proj));
            // ワールドの逆行列の転置を作成（法線変換用）
            Matrix4x4 inv = MathUtil::Inverse(world);
            // 逆行列の転置を計算
            Matrix4x4 invT = MathUtil::Transpose(inv);

            // インスタンスデータに転送
            instData[i].World = world;
            instData[i].WVP = wvp;
            instData[i].WorldInverseTranspose = invT;
            instData[i].color = pt.color;
        }

        // インスタンス描画
        if (auto* model = particlePlane_->GetModel()) {
            model->DrawInstanced(particlePlane_, count);
        } else {
            // モデルが無い場合は通常描画で1枚だけ（安全策）
            particlePlane_->Draw();
        }
    }
}

/// <summary>
/// 寿命の範囲を設定
/// </summary>
void ParticleManager::SetLifetimeRange(float minL, float maxL)
{
    // 最小値と最大値を入れ替える必要があるか確認
    if (minL <= maxL) {
        lifeMin_ = minL;
        lifeMax_ = maxL;
    } else {
        lifeMin_ = maxL;
        lifeMax_ = minL;
    }
}

/// <summary>
/// フィールド（ランダム加速度）有効フラグを設定
/// </summary>
void ParticleManager::SetFieldEnabled(bool enabled)
{
    // フィールド有効フラグを設定
    fieldEnabled_ = enabled;
}

/// <summary>
/// フィールド加速度を設定
/// </summary>
void ParticleManager::SetFieldAccel(const Vector3& a)
{
    // フィールド加速度を設定
    fieldAccel_ = a;
}

/// <summary>
/// フィールドの影響範囲をAABBで設定
/// </summary>
void ParticleManager::SetFieldAABB(const Vector3& mn, const Vector3& mx)
{
    // フィールドの影響範囲をAABBで設定
    fieldMin_ = mn;
    fieldMax_ = mx;
}

// 発生時のランダム範囲設定
void ParticleManager::SetSpawnPosRange(const Vector3& mn, const Vector3& mx) { spawnPosMin_ = mn; spawnPosMax_ = mx; }
void ParticleManager::SetVelocityRange(const Vector3& mn, const Vector3& mx) { velMin_ = mn; velMax_ = mx; }
void ParticleManager::SetScaleRange(const Vector3& mn, const Vector3& mx) { scaleMin_ = mn; scaleMax_ = mx; }
void ParticleManager::SetColorRange(const Vector4& mn, const Vector4& mx) { colMin_ = mn; colMax_ = mx; }

// 重力関連の設定
void ParticleManager::SetGravityEnabled(bool enabled) { gravityEnabled_ = enabled; }
void ParticleManager::SetGravity(const Vector3& g) { gravity_ = g; }
void ParticleManager::SetDamping(float d) { damping_ = d < 0.0f ? 0.0f : d; }

/// <summary>
/// このマネージャ用の ImGui コントロールを描画
/// </summary>
void ParticleManager::DrawImGui()
{
#ifdef USE_IMGUI
    // グループ数を表示
    ImGui::Text("Groups: %zu", particleGroups_.size());
    // フィールド設定
    ImGui::Checkbox("Enable Field", &fieldEnabled_);
    ImGui::DragFloat3("Field Accel", &fieldAccel_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat3("Field Min", &fieldMin_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat3("Field Max", &fieldMax_.x, 0.1f, -100.0f, 100.0f);

    ImGui::Separator();
    // 寿命設定
    ImGui::Text("Lifetime");
    ImGui::DragFloatRange2("Life Min/Max", &lifeMin_, &lifeMax_, 0.01f, 0.1f, 100.0f);

    ImGui::Separator();
    // 重力と減衰設定
    ImGui::Text("Dynamics");
    ImGui::Checkbox("Enable Gravity", &gravityEnabled_);
    ImGui::DragFloat3("Gravity", &gravity_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat("Damping (1/s)", &damping_, 0.01f, 0.0f, 10.0f);

    ImGui::Separator();
    // スポーン範囲設定
    ImGui::Text("Spawn Ranges");
    ImGui::DragFloat3("Pos Min", &spawnPosMin_.x, 0.01f, -10.0f, 10.0f);
    ImGui::DragFloat3("Pos Max", &spawnPosMax_.x, 0.01f, -10.0f, 10.0f);
    ImGui::DragFloat3("Vel Min", &velMin_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Vel Max", &velMax_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Scale Min", &scaleMin_.x, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat3("Scale Max", &scaleMax_.x, 0.01f, 0.01f, 10.0f);

    // カラー範囲設定
    ImGui::ColorEdit4("Color Min", &colMin_.x);
    ImGui::ColorEdit4("Color Max", &colMax_.x);

    ImGui::Separator();
    // グループごとのパーティクル数を表示
    ImGui::Text("Groups");
    // グループごとにツリー表示
    for (auto& kv : particleGroups_) {
        // グループ名をツリーのラベルにして表示
        if (ImGui::TreeNode(kv.first.c_str())) {
            // パーティクル数を表示
            ImGui::Text("Count = %zu", kv.second.particles.size());
            ImGui::TreePop();
        }
    }
#else
    (void)particleGroups_;
    (void)fieldEnabled_;
    (void)fieldAccel_;
    (void)fieldMin_;
    (void)fieldMax_;
    (void)lifeMin_;
    (void)lifeMax_;
    (void)gravityEnabled_;
    (void)gravity_;
    (void)damping_;
    (void)spawnPosMin_;
    (void)spawnPosMax_;
    (void)velMin_;
    (void)velMax_;
    (void)scaleMin_;
    (void)scaleMax_;
    (void)colMin_;
    (void)colMax_;
#endif
}
