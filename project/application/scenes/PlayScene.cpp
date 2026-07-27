#include "PlayScene.h"
#include "ImGuiManager.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/PrimitiveFactory.h"
#include "../../engine/3d/SkyBox.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/SrvManager.h"
#include "../../engine/base/WinApp.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/utility/mathUtility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr Vector2 kCenteredSpriteAnchor = { 0.5f, 0.5f }; // 中心基準で表示するスプライトのアンカー
constexpr Vector2 kTemporalSpriteDefaultSize = { 1.0f, 1.0f }; // 時間演出スプライトの初期サイズ
constexpr Vector4 kHiddenSpriteColor = { 0.0f, 0.0f, 0.0f, 0.0f }; // 非表示状態にするスプライト色
constexpr int kMaximumTemporalAfterimageCount = 8; // 時空破砕で保持できる最大残像数
constexpr int kMaximumTimeReversalParticleCount = 128; // 時間逆流で保持できる最大粒子数
constexpr int kMaximumTimeReversalAfterimageCount = 3; // 1粒子ごとに保持する最大残像数
constexpr std::array<float, 4> kSceneRenderTargetClearColor = { 0.53f, 0.71f, 0.82f, 1.0f }; // シーン描画RTのクリア色
constexpr std::array<float, 4> kTransparentRenderTargetClearColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // 中間RTと最終RTのクリア色
constexpr bool kUseDepthBuffer = true; // RenderTargetに深度バッファを作成する
constexpr bool kNoDepthBuffer = false; // RenderTargetに深度バッファを作成しない
constexpr bool kCreateDepthSrv = true; // 深度SRVを作成する
constexpr bool kNoDepthSrv = false; // 深度SRVを作成しない
constexpr float kParticleRingOuterRadius = 1.0f; // パーティクルリングの外径
constexpr float kParticleRingInnerRadius = 0.2f; // パーティクルリングの内径
constexpr float kParticleCylinderTopRadius = 1.0f; // パーティクル円柱の上面半径
constexpr float kParticleCylinderBottomRadius = 1.0f; // パーティクル円柱の下面半径
constexpr float kParticleCylinderHeight = 1.0f; // パーティクル円柱の高さ
constexpr uint32_t kParticleCylinderDivide = 32; // パーティクル円柱の分割数
constexpr bool kUseAlphaCutoutSampler = true; // アルファ抜き用サンプラーを使用する
constexpr bool kNoAlphaCutoutSampler = false; // アルファ抜き用サンプラーを使用しない
constexpr Vector3 kEmitterDefaultPosition = { 0.0f, 0.0f, 0.0f }; // エミッターの初期位置
constexpr int kHitEmitterParticleCount = 8; // ヒット演出で発生させる粒子数
constexpr int kSingleEffectEmitterCount = 1; // 単発演出で発生させる粒子数
constexpr float kEmitterDefaultFrequency = 0.25f; // エフェクト確認時に短寿命粒子が途切れにくい初期発生間隔
constexpr float kStoppedDeltaTime = 0.0f; // ヒットストップや時間停止中に使用する停止時間
constexpr float kHitStopFinishedThreshold = 0.0f; // ヒットストップが終了したとみなす残り時間
constexpr float kCubeEnvironmentCoefficient = 0.85f; // cubeに適用する環境マップ反射率
constexpr Vector3 kCubeInitialTranslate = { 3.0f, 0.0f, 0.0f }; // cubeの初期配置
constexpr Vector3 kSkinningPreviewScale = { 3.0f, 3.0f, 3.0f }; // Skinning確認モデルの初期スケール
constexpr Vector3 kSkinningPreviewTranslate = { 0.0f, 0.0f, 0.0f }; // Skinning確認モデルの初期位置
constexpr bool kLoadEnvironmentMapOnStartup = false; // 遷移直後に環境マップを読み込むか
constexpr const char* kFenceModelKeyword = "fence"; // アルファ抜き設定を適用するモデル判定キーワード
constexpr const char* kCubeModelKeywordLower = "cube"; // cubeモデル判定用の小文字キーワード
constexpr const char* kAnimatedCubeModelFileName = "AnimatedCube/AnimatedCube.gltf";
constexpr const char* kSimpleSkinModelFileName = "simpleSkin/simpleSkin.gltf"; // Skinning確認用simpleSkinモデル
constexpr const char* kHumanSneakWalkModelFileName = "human/sneakWalk.gltf"; // Skinning確認用sneakWalkモデル
constexpr const char* kHumanWalkModelFileName = "human/walk.gltf"; // Skinning確認用walkモデル
constexpr const char* kCubeModelKeywordUpper = "Cube"; // cubeモデル判定用の大文字キーワード

struct SceneModelLoadDesc {
    const char* fileName; // モデルファイル名
    bool loadOnStartup; // PlayScene遷移直後に読み込むか
};

constexpr std::array<SceneModelLoadDesc, 1> kSceneModelLoadDescs = {
    SceneModelLoadDesc { "sphere/sphere.gltf", true },
}; // シーンで扱うモデルと起動時ロード設定
constexpr std::array<const char*, 10> kSceneObjectCreateModelNames = {
    "plane/plane.gltf",
    "bunny/bunny.obj",
    "teapot/teapot.obj",
    "fence/fence.obj",
    "sphere/sphere.gltf",
    "terrain/terrain.obj",
    kAnimatedCubeModelFileName,
    kSimpleSkinModelFileName,
    kHumanSneakWalkModelFileName,
    kHumanWalkModelFileName,
}; // ImGuiから生成できる3Dモデル名
constexpr std::array<const char*, 10> kSceneObjectCreateModelDisplayNames = {
    "plane.gltf",
    "bunny.obj",
    "teapot.obj",
    "fence.obj",
    "sphere.gltf",
    "terrain.obj",
    "AnimatedCube.gltf",
    "simpleSkin.gltf",
    "sneakWalk.gltf",
    "walk.gltf",
}; // ImGuiに表示する3Dモデル名
constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
constexpr const char* kCircleTextureName = "circle.png"; // 円形パーティクルに使用するテクスチャ名
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名
constexpr const char* kGradationLineTextureName = "gradationLine.png"; // リングと円柱に使用するテクスチャ名
constexpr const char* kUvCheckerTextureName = "uvChecker.png"; // 確認用UVテクスチャ名
constexpr const char* kMonsterBallTextureName = "monsterBall.png"; // 確認用ボールテクスチャ名
constexpr std::array<const char*, 5> kSceneSpriteCreateTextureNames = {
    kUvCheckerTextureName,
    kMonsterBallTextureName,
    kCircleTextureName,
    kCircleFlashTextureName,
    kGradationLineTextureName,
}; // ImGuiから生成できるスプライト用テクスチャ名
constexpr const char* kDissolveMaskTextureName = "noise0.png"; // Dissolveに使用するノイズマスク名
constexpr std::array<const char*, 5> kSceneTextureNames = {
    kUvCheckerTextureName,
    kMonsterBallTextureName,
    kCircleTextureName,
    kGradationLineTextureName,
    kEnvironmentMapTextureName,
}; // シーン初期化時に読み込むテクスチャ名

constexpr const char* kCircleParticleGroupName = "Circle"; // 円形パーティクルグループ名
constexpr const char* kCheckerParticleGroupName = "Checker"; // チェッカーパーティクルグループ名
constexpr const char* kBallParticleGroupName = "Ball"; // ボールパーティクルグループ名
constexpr const char* kHitParticleGroupName = "Hit"; // ヒット演出パーティクルグループ名
constexpr const char* kRingParticleGroupName = "Ring"; // リング演出パーティクルグループ名
constexpr const char* kCylinderParticleGroupName = "Cylinder"; // 円柱演出パーティクルグループ名

/// <summary>
/// パーティクルエミッターで使用する演出種別
/// </summary>
enum class ParticleEmitterEffectType {
    Hit,
    Ring,
    Cylinder,
};

/// <summary>
/// パス文字列から表示用のファイル名部分だけを取得する。
/// </summary>
std::string GetDisplayFileName(const std::string& path)
{
    const size_t separatorPosition = path.find_last_of("/\\"); // 最後に見つかったパス区切り位置
    if (separatorPosition == std::string::npos) {
        return path;
    }

    return path.substr(separatorPosition + 1);
}

/// <summary>
/// パーティクルエミッターの共通設定を適用する
/// </summary>
void ConfigureParticleEmitter(
    ParticleEmitter& emitter,
    const char* groupName,
    int particleCount,
    ParticleEmitterEffectType effectType)
{
    emitter.groupName = groupName;
    emitter.transform.translate = kEmitterDefaultPosition;
    emitter.count = particleCount;
    emitter.frequency = kEmitterDefaultFrequency;
    emitter.useHitEffect = false;
    emitter.useRingEffect = false;
    emitter.useCylinderEffect = false;

    switch (effectType) {
    case ParticleEmitterEffectType::Hit:
        emitter.useHitEffect = true;
        break;
    case ParticleEmitterEffectType::Ring:
        emitter.useRingEffect = true;
        break;
    case ParticleEmitterEffectType::Cylinder:
        emitter.useCylinderEffect = true;
        break;
    }

    emitter.Emit();
}

/// <summary>
/// ポストプロセス用のRenderTarget設定を作成する
/// </summary>
RenderTargetDesc CreatePostProcessRenderTargetDesc(
    DXGI_FORMAT format,
    bool useDepth,
    bool createDepthSrv,
    const std::array<float, 4>& clearColor)
{
    RenderTargetDesc desc {}; // 作成するRenderTarget設定
    desc.width = WinApp::kWindowWidth;
    desc.height = WinApp::kWindowHeight;
    desc.format = format;
    desc.useDepth = useDepth;
    desc.createColorSrv = true;
    desc.createDepthSrv = createDepthSrv;
    desc.resizeWithWindow = true;
    desc.clearColor = clearColor;
    return desc;
}

/// <summary>
/// 非表示状態の時間演出スプライトを作成する
/// </summary>
std::unique_ptr<Sprite> CreateHiddenTemporalSprite(const SceneContext& ctx, const std::string& textureName)
{
    auto sprite = std::make_unique<Sprite>(); // 作成する時間演出用スプライト
    sprite->Initialize(ctx.spriteCommon, textureName, ctx.imguiManager);
    sprite->SetAnchorPoint(kCenteredSpriteAnchor);
    sprite->SetSize(kTemporalSpriteDefaultSize);
    sprite->SetColor(kHiddenSpriteColor);
    sprite->Update();
    return sprite;
}

/// <summary>
/// パーティクル描画用の3Dオブジェクトを作成する
/// </summary>
std::unique_ptr<Object3d> CreateParticleDrawObject(
    const SceneContext& ctx,
    const std::vector<Object3d::VertexData>& meshVertices,
    const char* textureName,
    bool useAlphaCutoutSampler)
{
    auto object3d = std::make_unique<Object3d>(); // 作成するパーティクル描画用オブジェクト
    object3d->Initialize(ctx.object3dCommon, ctx.imguiManager);
    object3d->SetMesh(meshVertices);
    object3d->SetTexture(textureName);
    object3d->SetEnableLighting(false);
    object3d->SetUseAlphaCutoutSampler(useAlphaCutoutSampler);
    return object3d;
}

/// <summary>
/// ヒットストップが残っているか判定する
/// </summary>
bool HasActiveHitStop(float hitStopRemainingTime)
{
    return hitStopRemainingTime > kHitStopFinishedThreshold;
}

/// <summary>
/// 指定したモデルファイル名に判定キーワードが含まれるか調べる
/// </summary>
bool ContainsModelKeyword(const std::string& modelFileName, const char* keyword)
{
    return modelFileName.find(keyword) != std::string::npos;
}

/// <summary>
/// アルファ抜き設定が必要なモデルか判定する
/// </summary>
bool IsFenceModelFile(const std::string& modelFileName)
{
    return ContainsModelKeyword(modelFileName, kFenceModelKeyword);
}

/// <summary>
/// 環境マップ確認用のcubeモデルか判定する
/// </summary>
bool IsCubeModelFile(const std::string& modelFileName)
{
    return ContainsModelKeyword(modelFileName, kCubeModelKeywordLower)
        || ContainsModelKeyword(modelFileName, kCubeModelKeywordUpper);
}

/// <summary>
/// 初期化時にアニメーションも設定するモデルか判定する
/// </summary>
bool IsAnimationModelFile(const std::string& modelFileName)
{
    return modelFileName == kAnimatedCubeModelFileName
        || modelFileName == kHumanSneakWalkModelFileName
        || modelFileName == kHumanWalkModelFileName;
}

/// <summary>
/// Skinning確認用に表示サイズを調整するモデルか判定する
/// </summary>
bool IsSkinningPreviewModelFile(const std::string& modelFileName)
{
    return modelFileName == kSimpleSkinModelFileName
        || modelFileName == kHumanSneakWalkModelFileName
        || modelFileName == kHumanWalkModelFileName;
}

/// <summary>
/// 指定したテクスチャをPlayScene遷移直後に読み込むか判定する。
/// </summary>
bool ShouldLoadTextureOnStartup(const char* textureName)
{
    if (!textureName) {
        return false;
    }

    if (!kLoadEnvironmentMapOnStartup && std::string(textureName) == kEnvironmentMapTextureName) {
        return false;
    }

    return true;
}
}

/// <summary>
/// コンストラクタ
/// </summary>
PlayScene::PlayScene() { }

/// <summary>
/// デストラクタ
/// </summary>
PlayScene::~PlayScene() { }

/// <summary>
/// パーティクル描画用オブジェクトを初期化する
/// </summary>
void PlayScene::InitializeParticleObjects()
{
    const std::vector<Object3d::VertexData> planeMesh = PrimitiveFactory::CreatePlane(); // 平面パーティクル用メッシュ
    particlePlane_ = CreateParticleDrawObject(
        ctx_,
        planeMesh,
        kCircleTextureName,
        kNoAlphaCutoutSampler);

    const std::vector<Object3d::VertexData> ringMesh = PrimitiveFactory::CreateRing(
        kParticleRingOuterRadius,
        kParticleRingInnerRadius); // リングパーティクル用メッシュ
    particleRing_ = CreateParticleDrawObject(
        ctx_,
        ringMesh,
        kGradationLineTextureName,
        kUseAlphaCutoutSampler);

    const std::vector<Object3d::VertexData> cylinderMesh = PrimitiveFactory::CreateCylinder(
        kParticleCylinderTopRadius,
        kParticleCylinderBottomRadius,
        kParticleCylinderHeight,
        kParticleCylinderDivide); // 円柱パーティクル用メッシュ
    particleCylinder_ = CreateParticleDrawObject(
        ctx_,
        cylinderMesh,
        kGradationLineTextureName,
        kUseAlphaCutoutSampler);
}

/// <summary>
/// ParticleManagerに使用するグループと描画オブジェクトを登録する。
/// </summary>
void PlayScene::InitializeParticleManager()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル全体を管理するインスタンス
    if (!particleManager) {
        return;
    }

    particleManager->Initialize(ctx_.directXCommon, ctx_.object3dCommon, ctx_.srvManager, ctx_.textureManager, ctx_.imguiManager);
    particleManager->SetParticlePlane(particlePlane_.get());
    particleManager->CreateParticleGroup(kCircleParticleGroupName, kCircleTextureName);
    particleManager->CreateParticleGroup(kCheckerParticleGroupName, kUvCheckerTextureName);
    particleManager->CreateParticleGroup(kBallParticleGroupName, kMonsterBallTextureName);
    particleManager->CreateParticleGroup(kHitParticleGroupName, kCircleFlashTextureName);
    particleManager->CreateParticleGroup(kRingParticleGroupName, kGradationLineTextureName);
    particleManager->CreateParticleGroup(kCylinderParticleGroupName, kGradationLineTextureName);
    particleManager->SetParticleObject(kHitParticleGroupName, particlePlane_.get());
    particleManager->SetParticleObject(kRingParticleGroupName, particleRing_.get());
    particleManager->SetParticleObject(kCylinderParticleGroupName, particleCylinder_.get());
    particleManager->SetGroupBillboard(kCylinderParticleGroupName, false);
}

/// <summary>
/// ヒット演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeHitParticleEmitter()
{
    ConfigureParticleEmitter(
        pmEmitter_,
        kHitParticleGroupName,
        kHitEmitterParticleCount,
        ParticleEmitterEffectType::Hit);
}

/// <summary>
/// リング演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeRingParticleEmitter()
{
    ConfigureParticleEmitter(
        ringEmitter_,
        kRingParticleGroupName,
        kSingleEffectEmitterCount,
        ParticleEmitterEffectType::Ring);
}

/// <summary>
/// 円柱演出用エミッターを初期化する。
/// </summary>
void PlayScene::InitializeCylinderParticleEmitter()
{
    ConfigureParticleEmitter(
        cylinderEmitter_,
        kCylinderParticleGroupName,
        kSingleEffectEmitterCount,
        ParticleEmitterEffectType::Cylinder);
}

/// <summary>
/// パーティクルエミッターを初期化する。
/// </summary>
void PlayScene::InitializeParticleEmitters()
{
    nextParticleEmitterId_ = 1;
    pmEmitter_.SetEmitterId(IssueParticleEmitterId());
    ringEmitter_.SetEmitterId(IssueParticleEmitterId());
    cylinderEmitter_.SetEmitterId(IssueParticleEmitterId());

    InitializeHitParticleEmitter();
    InitializeRingParticleEmitter();
    InitializeCylinderParticleEmitter();
    RebuildParticleEmitterPointerView();
}

/// <summary>
/// パーティクル管理とエミッターを初期化する。
/// </summary>
void PlayScene::InitializeParticleEffects()
{
    InitializeParticleManager();
    InitializeParticleEmitters();
}

/// <summary>
/// 時間演出用スプライトを初期化する
/// </summary>
void PlayScene::InitializeTemporalEffectSprites()
{
    temporalAfterimageSprites_.reserve(kMaximumTemporalAfterimageCount);
    timeReversalSprites_.reserve(kMaximumTimeReversalParticleCount);
    timeReversalAfterimageSprites_.reserve(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount);
}

/// <summary>
/// 時空破砕で使用する残像スプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTemporalRiftSprites()
{
    while (temporalAfterimageSprites_.size() < kMaximumTemporalAfterimageCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時空破砕の残像表示に使うスプライト
        temporalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }
}

/// <summary>
/// 時間逆行で使用するスプライトを必要になった時点で作成する。
/// </summary>
void PlayScene::EnsureTimeReversalSprites()
{
    while (timeReversalSprites_.size() < kMaximumTimeReversalParticleCount) {
        auto particleSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の粒子表示に使うスプライト
        timeReversalSprites_.push_back(std::move(particleSprite));
    }

    const size_t maximumRewindAfterimageSpriteCount = static_cast<size_t>(kMaximumTimeReversalParticleCount * kMaximumTimeReversalAfterimageCount); // 時間逆行の軌跡表示に必要な残像数
    while (timeReversalAfterimageSprites_.size() < maximumRewindAfterimageSpriteCount) {
        auto afterimageSprite = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName); // 時間逆行の軌跡表示に使うスプライト
        timeReversalAfterimageSprites_.push_back(std::move(afterimageSprite));
    }

    if (!timeReversalConvergenceSprite_) {
        timeReversalConvergenceSprite_ = CreateHiddenTemporalSprite(ctx_, kCircleFlashTextureName);
    }
}

/// <summary>
/// ポストプロセス用レンダーターゲットを初期化する
/// </summary>
void PlayScene::InitializePostProcessTargets()
{
    DirectXCommon* directXCommon = ctx_.directXCommon; // 初期化に使用するDirectX基盤
    if (!directXCommon) {
        return;
    }

    const DXGI_FORMAT renderTargetFormat = directXCommon->GetSwapChainFormat(); // 各RTで使用するカラーフォーマット
    const RenderTargetDesc sceneRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kUseDepthBuffer,
        kCreateDepthSrv,
        kSceneRenderTargetClearColor); // シーン描画用RT設定
    sceneRenderTarget_.Initialize(directXCommon, sceneRenderTargetDesc);

    const RenderTargetDesc intermediateTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // ポストプロセス中間RT設定
    postProcessIntermediateTarget_.Initialize(directXCommon, intermediateTargetDesc);

    const RenderTargetDesc finalRenderTargetDesc = CreatePostProcessRenderTargetDesc(
        renderTargetFormat,
        kNoDepthBuffer,
        kNoDepthSrv,
        kTransparentRenderTargetClearColor); // Scene View表示用RT設定
    finalRenderTarget_.Initialize(directXCommon, finalRenderTargetDesc);

    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(kDissolveMaskTextureName);
        dissolveMaskSrvIndex_ = ctx_.textureManager->GetSrvIndex(kDissolveMaskTextureName);
    }

    postProcess_.Initialize(directXCommon);
    postProcess_.SetEffectType(PostEffectType::Copy);
}

/// <summary>
/// 指定したテクスチャ名からシーン用スプライトを生成する。
/// </summary>
void PlayScene::CreateSceneSprite(const std::string& textureName)
{
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(textureName);
    }

    auto sprite = std::make_unique<Sprite>(); // 生成するスプライト
    sprite->SetSpriteId(IssueSpriteId());
    sprite->Initialize(ctx_.spriteCommon, textureName, ctx_.imguiManager);
    sprite->Update();
    sprites_.push_back(std::move(sprite));
    RebuildSpritePointerView();
}

/// <summary>
/// 指定した番号のシーン用スプライトを削除する。
/// </summary>
void PlayScene::DeleteSceneSprite(size_t spriteIndex)
{
    if (sprites_.size() <= spriteIndex) {
        return;
    }

    if (ctx_.imguiManager) {
        const size_t remainingSpriteCount = sprites_.size() - 1; // 削除後に残るスプライト数
        ctx_.imguiManager->NotifySpriteDeleted(spriteIndex, remainingSpriteCount);
    }

    sprites_.erase(sprites_.begin() + spriteIndex);
    RebuildSpritePointerView();
}

/// <summary>
/// ImGuiでシーン内スプライトの生成と削除を行う。
/// </summary>
void PlayScene::DrawSceneSpriteEditImGui()
{
#ifdef USE_IMGUI
    static int selectedCreateTextureIndex = 0; // 生成に使用するテクスチャ番号
    static int selectedDeleteSpriteIndex = 0; // 削除対象のスプライト番号

    ImGui::SeparatorText("Create");
    const char* textureNames[kSceneSpriteCreateTextureNames.size()] = {}; // Combo表示用のテクスチャ名一覧
    for (size_t textureIndex = 0; textureIndex < kSceneSpriteCreateTextureNames.size(); ++textureIndex) {
        textureNames[textureIndex] = kSceneSpriteCreateTextureNames[textureIndex];
    }

    ImGui::Combo(
        "Texture",
        &selectedCreateTextureIndex,
        textureNames,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()));
    selectedCreateTextureIndex = (std::clamp)(
        selectedCreateTextureIndex,
        0,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()) - 1);
    if (ImGui::Button("Create Sprite")) {
        const std::string textureName = kSceneSpriteCreateTextureNames[static_cast<size_t>(selectedCreateTextureIndex)]; // 生成するスプライトのテクスチャ名
        CreateSceneSprite(textureName);
        selectedDeleteSpriteIndex = static_cast<int>(sprites_.size()) - 1;
    }

    ImGui::SeparatorText("Delete");
    if (!sprites_.empty()) {
        const int spriteCount = static_cast<int>(sprites_.size()); // 削除対象として選択できるスプライト数
        selectedDeleteSpriteIndex = (std::clamp)(selectedDeleteSpriteIndex, 0, spriteCount - 1);

        std::string preview = "Sprite " + std::to_string(selectedDeleteSpriteIndex); // Comboの現在表示名
        Sprite* previewSprite = sprites_[static_cast<size_t>(selectedDeleteSpriteIndex)].get(); // 現在選択中のスプライト
        if (previewSprite && !previewSprite->GetTextureFilePath().empty()) {
            preview += " : " + GetDisplayFileName(previewSprite->GetTextureFilePath());
        }

        if (ImGui::BeginCombo("Delete Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = sprites_[static_cast<size_t>(spriteIndex)].get(); // 表示名を作る対象のスプライト
                std::string label = "Sprite " + std::to_string(spriteIndex); // Comboに表示するスプライト名
                if (sprite && !sprite->GetTextureFilePath().empty()) {
                    label += " : " + GetDisplayFileName(sprite->GetTextureFilePath());
                }

                const bool isSelected = selectedDeleteSpriteIndex == spriteIndex; // 現在選択中かどうか
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedDeleteSpriteIndex = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Delete Sprite")) {
            DeleteSceneSprite(static_cast<size_t>(selectedDeleteSpriteIndex));
            selectedDeleteSpriteIndex = (std::min)(selectedDeleteSpriteIndex, static_cast<int>(sprites_.size()) - 1);
        }
    } else {
        ImGui::Text("No sprites.");
    }
#endif
}
void PlayScene::ApplySceneObjectInitialSettings(Object3d& object3d, const std::string& modelFileName)
{
    const bool isFenceModel = IsFenceModelFile(modelFileName); // アルファ抜き用サンプラーが必要なモデルか
    if (isFenceModel) {
        object3d.SetUseAlphaCutoutSampler(true);
    }

    const bool isCubeModel = IsCubeModelFile(modelFileName); // 環境マップ確認用モデルか
    if (isCubeModel) {
        object3d.SetEnvironmentCoefficient(kCubeEnvironmentCoefficient);
        object3d.SetTranslate(kCubeInitialTranslate);
    }

    const bool isSkinningPreviewModel = IsSkinningPreviewModelFile(modelFileName); // Skinning確認用モデルか
    if (isSkinningPreviewModel) {
        object3d.SetScale(kSkinningPreviewScale);
        object3d.SetTranslate(kSkinningPreviewTranslate);
    }
}

/// <summary>
/// 指定したモデルファイル名からシーン用3Dオブジェクトを生成する
/// </summary>
void PlayScene::CreateSceneObject(const std::string& modelFileName)
{
    auto object3d = std::make_unique<Object3d>(); // 生成する3Dオブジェクト
    object3d->SetObjectId(IssueObjectId());
    object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    object3d->SetModel(modelFileName);
    if (IsAnimationModelFile(modelFileName)) {
        object3d->SetAnimation(modelFileName);
    }
    ApplySceneObjectInitialSettings(*object3d, modelFileName);
    objects3d_.push_back(std::move(object3d));
    RebuildObjectPointerView();
}

/// <summary>
/// 指定した番号のシーン用3Dオブジェクトを削除する
/// </summary>
void PlayScene::DeleteSceneObject(size_t objectIndex)
{
    if (objects3d_.size() <= objectIndex) {
        return;
    }

    if (ctx_.imguiManager) {
        const size_t remainingObjectCount = objects3d_.size() - 1; // 削除後に残る3Dオブジェクト数
        ctx_.imguiManager->NotifyObjectDeleted(objectIndex, remainingObjectCount);
    }

    objects3d_.erase(objects3d_.begin() + objectIndex);
    RebuildObjectPointerView();
}

/// <summary>
/// ImGuiでシーン内3Dオブジェクトの生成と削除を行う
/// </summary>
void PlayScene::DrawSceneObjectEditImGui()
{
#ifdef USE_IMGUI
    static int selectedCreateModelIndex = 0; // 生成に使用するモデル番号
    static int selectedDeleteObjectIndex = 0; // 削除対象のオブジェクト番号

    ImGui::SeparatorText("Create");
    const char* modelNames[kSceneObjectCreateModelDisplayNames.size()] = {}; // Combo表示用のモデル名一覧
    for (size_t modelIndex = 0; modelIndex < kSceneObjectCreateModelDisplayNames.size(); ++modelIndex) {
        modelNames[modelIndex] = kSceneObjectCreateModelDisplayNames[modelIndex];
    }

    ImGui::Combo(
        "Model",
        &selectedCreateModelIndex,
        modelNames,
        static_cast<int>(kSceneObjectCreateModelDisplayNames.size()));
    selectedCreateModelIndex = (std::clamp)(
        selectedCreateModelIndex,
        0,
        static_cast<int>(kSceneObjectCreateModelDisplayNames.size()) - 1);
    if (ImGui::Button("Create Object")) {
        const std::string modelFileName = kSceneObjectCreateModelNames[static_cast<size_t>(selectedCreateModelIndex)]; // 生成するモデルファイル名
        CreateSceneObject(modelFileName);
        selectedDeleteObjectIndex = static_cast<int>(objects3d_.size()) - 1;
    }

    ImGui::SeparatorText("Delete");
    if (!objects3d_.empty()) {
        const int objectCount = static_cast<int>(objects3d_.size()); // 削除対象として選択できるオブジェクト数
        selectedDeleteObjectIndex = (std::clamp)(selectedDeleteObjectIndex, 0, objectCount - 1);

        std::string preview = "Object " + std::to_string(selectedDeleteObjectIndex); // Comboの現在表示名
        Object3d* previewObject = objects3d_[static_cast<size_t>(selectedDeleteObjectIndex)].get(); // 現在選択中のオブジェクト
        if (previewObject && !previewObject->GetDebugName().empty()) {
            preview += " : " + GetDisplayFileName(previewObject->GetDebugName());
        }

        if (ImGui::BeginCombo("Delete Target", preview.c_str())) {
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
                Object3d* object = objects3d_[static_cast<size_t>(objectIndex)].get(); // 表示名を作る対象のオブジェクト
                std::string label = "Object " + std::to_string(objectIndex); // Comboに表示するオブジェクト名
                if (object && !object->GetDebugName().empty()) {
                    label += " : " + GetDisplayFileName(object->GetDebugName());
                }

                const bool isSelected = selectedDeleteObjectIndex == objectIndex; // 現在選択中かどうか
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedDeleteObjectIndex = objectIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Delete Object")) {
            DeleteSceneObject(static_cast<size_t>(selectedDeleteObjectIndex));
            selectedDeleteObjectIndex = (std::min)(selectedDeleteObjectIndex, static_cast<int>(objects3d_.size()) - 1);
        }
    } else {
        ImGui::Text("No objects.");
    }
#endif
}
void PlayScene::InitializeSceneObjects()
{
    objects3d_.reserve(kSceneModelLoadDescs.size());
    for (const SceneModelLoadDesc& modelDesc : kSceneModelLoadDescs) {
        auto object3d = std::make_unique<Object3d>(); // 作成中の3Dオブジェクト
        object3d->SetObjectId(IssueObjectId());
        object3d->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
        object3d->SetModel(modelDesc.fileName);
        ApplySceneObjectInitialSettings(*object3d, modelDesc.fileName);
        objects3d_.push_back(std::move(object3d));
    }
    RebuildObjectPointerView();
}

/// <summary>
/// シーンで使用するテクスチャを読み込む。
/// </summary>
void PlayScene::LoadSceneTextures()
{
    if (!ctx_.textureManager) {
        return;
    }

    for (const char* textureName : kSceneTextureNames) {
        if (!ShouldLoadTextureOnStartup(textureName)) {
            continue;
        }
        ctx_.textureManager->LoadTexture(textureName);
    }
}

/// <summary>
/// 環境マップ用のSkyBoxを初期化する。
/// </summary>
void PlayScene::InitializeSkyBox()
{
    if (!kLoadEnvironmentMapOnStartup) {
        return;
    }

    if (!ctx_.textureManager || !ctx_.srvManager || !ctx_.directXCommon) {
        return;
    }
    const uint32_t environmentMapSrvIndex = ctx_.textureManager->GetSrvIndex(kEnvironmentMapTextureName); // 環境マップのSRV番号
    if (environmentMapSrvIndex == UINT32_MAX) {
        return;
    }

    skybox_ = std::make_unique<SkyBox>();
    skybox_->Initialize(ctx_.directXCommon, ctx_.srvManager, environmentMapSrvIndex);
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetEnvironmentMapSrvIndex(environmentMapSrvIndex);
    }
}

void PlayScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;

    LoadSceneTextures();
    InitializeSkyBox();
    InitializeSceneObjects();
    InitializeParticleObjects();
    InitializeParticleEffects();
    InitializeTemporalEffectSprites();
    InitializePostProcessTargets();
}

/// <summary>
/// ポストプロセス用リソースを解放する。
/// </summary>
void PlayScene::FinalizePostProcessTargets()
{
    sceneRenderTarget_.Finalize();
    postProcessIntermediateTarget_.Finalize();
    finalRenderTarget_.Finalize();
    dissolveMaskSrvIndex_ = UINT32_MAX;
    postProcess_.Finalize();
}

/// <summary>
/// ParticleManagerを解放する。
/// </summary>
void PlayScene::FinalizeParticleManager()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // 解放対象のパーティクル管理
    if (particleManager) {
        particleManager->Finalize();
    }
}

/// <summary>
/// シーンが保持している表示用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseSceneObjects()
{
    sprites_.clear();
    spritePointerView_.clear();
    nextSpriteId_ = 1;
    objects3d_.clear();
    objectPointerView_.clear();
    nextObjectId_ = 1;
    particleEmitterPointerView_.clear();
    nextParticleEmitterId_ = 1;
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
}

/// <summary>
/// パーティクル描画用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseParticleObjects()
{
    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル描画オブジェクトの参照元
    if (particleManager) {
        particleManager->SetParticlePlane(nullptr);
    }

    particlePlane_.reset();
    particleRing_.reset();
    particleCylinder_.reset();
}

/// <summary>
/// SkyBoxを解放する。
/// </summary>
void PlayScene::ReleaseSkyBox()
{
    if (skybox_) {
        skybox_->Finalize();
        skybox_.reset();
    }
}

/// <summary>
/// 終了処理を行う。
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    StopCameraShake();

    FinalizePostProcessTargets();
    FinalizeParticleManager();
    ReleaseSceneObjects();
    timeReversalEffect_.ResetState();
    ReleaseParticleObjects();
    ReleaseSkyBox();
    temporalRiftEffect_.ResetState(ctx_.camera);
    timeStopEffect_.ResetState();
    ctx_ = {};
}

/// <summary>
/// エフェクト開始入力を処理する。
/// </summary>
void PlayScene::HandleEffectStartInput()
{
    InputManager* inputManager = InputManager::GetInstance(); // エフェクト開始入力を取得する入力管理
    if (inputManager
        && inputManager->IsKeyJustPressed(DIK_R)
        && !IsAnyEffectPlaying()) {
        StartSelectedEffect();
    }
}

/// <summary>
/// 時間演出とポストプロセスの状態を更新する。
/// </summary>
void PlayScene::UpdateTemporalEffects(float deltaTime)
{
    const float hitStopRemainingTime = temporalRiftEffect_.GetHitStopRemainingTime(); // 現在のヒットストップ残り時間
    const bool hasActiveHitStop = HasActiveHitStop(hitStopRemainingTime); // ヒットストップ中か
    const float effectDeltaTime = hasActiveHitStop ? kStoppedDeltaTime : deltaTime; // ヒットストップを反映した演出時間
    UpdateTimeReversalTransformHistory();
    UpdateTemporalRiftEffect(effectDeltaTime);
    UpdateTimeReversalEffect(effectDeltaTime);
    UpdateTimeStopEffect(deltaTime);
    UpdateImpactResponse(deltaTime);
    if (!hasActiveHitStop) {
        UpdateTemporalAfterimages();
    }
    postProcess_.Update(deltaTime);
}

/// <summary>
/// 再生中エフェクトに対応するポストエフェクト中心を計算する。
/// </summary>
Vector2 PlayScene::CalculatePostEffectCenter() const
{
    if (timeStopEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeStopEffect_.GetEffectPosition());
    }
    if (timeReversalEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeReversalEffect_.GetEffectPosition());
    }

    return temporalRiftEffect_.GetScreenUv();
}

/// <summary>
/// ポストエフェクトの中心座標を更新する。
/// </summary>
void PlayScene::UpdatePostEffectCenters()
{
    temporalRiftEffect_.SetScreenUv(CalculateTemporalRiftScreenUv());
    const Vector2 postEffectCenter = CalculatePostEffectCenter(); // 再生中エフェクトに対応する画面中心
    postProcess_.SetRadialBlurCenter(postEffectCenter);
    postProcess_.SetDistortionCenter(postEffectCenter);
}

/// <summary>
/// 更新処理
/// </summary>
void PlayScene::Update(float dt)
{
    HandleEffectStartInput();
    UpdateTemporalEffects(dt);

    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    UpdatePostEffectCenters();

    UpdateParticleSystems(dt);
    UpdateSceneObjects(dt);

    UpdateAfterimageSprites();
    UpdateTimeReversalSprites();
}

/// <summary>
/// パーティクルエミッターとParticleManagerを更新する。
/// </summary>
void PlayScene::UpdateParticleSystems(float deltaTime)
{
    const bool hasActiveHitStop = HasActiveHitStop(temporalRiftEffect_.GetHitStopRemainingTime()); // ヒットストップ中か
    const float particleDeltaTime = (hasActiveHitStop || IsTimeStopped()) ? kStoppedDeltaTime : deltaTime; // ヒットストップと時間停止を反映したパーティクル時間
    pmEmitter_.Update(particleDeltaTime);
    ringEmitter_.Update(particleDeltaTime);
    cylinderEmitter_.Update(particleDeltaTime);

    ParticleManager* particleManager = ParticleManager::GetInstance(); // パーティクル全体を更新する管理クラス
    if (particleManager) {
        particleManager->Update(particleDeltaTime);
    }
}

/// <summary>
/// シーン内の3Dオブジェクトを更新する。
/// </summary>
void PlayScene::UpdateSceneObjects(float deltaTime)
{
    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // 3Dオブジェクト更新に使用するビュー行列
    const Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // 3Dオブジェクト更新に使用する射影行列
    for (auto& object3d : objects3d_) { // 更新対象の3Dオブジェクト
        if (object3d) {
            object3d->UpdateAnimation(deltaTime);
            object3d->Update(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// 描画処理を行う
/// </summary>
void PlayScene::Draw()
{
    if (DrawPostProcessedScene()) {
        return;
    }

    DrawSceneContent();
    DrawDebugLines3D();
    DrawSprites();
    DrawDebugLines2D();
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void PlayScene::OnEnter() { std::cout << "PlayScene OnEnter\n"; }

/// <summary>
/// シーンから出るときの処理
/// </summary>
void PlayScene::OnExit() { std::cout << "PlayScene OnExit\n"; }

/// <summary>
/// Scene View用のオフスクリーン描画だけにするか設定する
/// </summary>
void PlayScene::SetSceneViewOnly(bool enabled)
{
    sceneViewOnly_ = enabled;
}

uint32_t PlayScene::GetSceneViewSrvIndex() const
{
    if (finalRenderTarget_.HasColorSrv()) {
        return finalRenderTarget_.GetColorSrvIndex();
    }
    if (sceneRenderTarget_.HasColorSrv()) {
        return sceneRenderTarget_.GetColorSrvIndex();
    }
    return UINT32_MAX;
}

/// <summary>
/// シーンが使用しているポストプロセスを取得する
/// </summary>
PostProcess* PlayScene::GetPostProcess()
{
    return &postProcess_;
}

/// <summary>
/// 所有中のスプライトから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildSpritePointerView()
{
    spritePointerView_.clear();
    spritePointerView_.reserve(sprites_.size());
    for (auto& sprite : sprites_) {
        if (sprite) {
            spritePointerView_.push_back(sprite.get());
        }
    }
}

/// <summary>
/// 所有中の3Dオブジェクトから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildObjectPointerView()
{
    objectPointerView_.clear();
    objectPointerView_.reserve(objects3d_.size());
    for (auto& object : objects3d_) {
        if (object) {
            objectPointerView_.push_back(object.get());
        }
    }
}

/// <summary>
/// 所有中のパーティクルエミッターから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildParticleEmitterPointerView()
{
    particleEmitterPointerView_.clear();
    particleEmitterPointerView_.reserve(3);
    particleEmitterPointerView_.push_back(&pmEmitter_);
    particleEmitterPointerView_.push_back(&ringEmitter_);
    particleEmitterPointerView_.push_back(&cylinderEmitter_);
}

/// <summary>
/// 次に生成するスプライトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueSpriteId()
{
    const uint32_t issuedSpriteId = nextSpriteId_; // 今回割り当てるスプライトID
    nextSpriteId_++;
    return issuedSpriteId;
}

/// <summary>
/// 次に生成する3Dオブジェクトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueObjectId()
{
    const uint32_t issuedObjectId = nextObjectId_; // 今回割り当てる3DオブジェクトID
    nextObjectId_++;
    return issuedObjectId;
}

/// <summary>
/// 次に生成するパーティクルエミッターへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueParticleEmitterId()
{
    const uint32_t issuedEmitterId = nextParticleEmitterId_; // 今回割り当てるパーティクルエミッターID
    nextParticleEmitterId_++;
    return issuedEmitterId;
}

/// <summary>
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    RebuildObjectPointerView();
    out->clear();
    out->reserve(objectPointerView_.size());
    out->insert(out->end(), objectPointerView_.begin(), objectPointerView_.end());
}

/// <summary>
/// シーンが所有するパーティクルエミッターポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillParticleEmitterPointers(std::vector<::ParticleEmitter*>* out)
{
    if (!out) {
        return;
    }

    RebuildParticleEmitterPointerView();
    out->clear();
    out->reserve(particleEmitterPointerView_.size());
    out->insert(out->end(), particleEmitterPointerView_.begin(), particleEmitterPointerView_.end());
}

/// <summary>
/// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out) {
        return;
    }

    RebuildSpritePointerView();
    out->clear();
    out->reserve(spritePointerView_.size());
    out->insert(out->end(), spritePointerView_.begin(), spritePointerView_.end());
}
