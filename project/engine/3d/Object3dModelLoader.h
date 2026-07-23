#pragma once

#include "Object3d.h"

namespace MyEngine::Object3dModelLoader {

/// <summary>
/// .mtlファイルを読み取り、マテリアル情報を取得する。
/// </summary>
Object3d::MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

/// <summary>
/// モデルファイルを読み込み、Object3d用のモデルデータを作成する。
/// </summary>
Object3d::ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

/// <summary>
/// アニメーションファイルを読み込み、Object3d用のアニメーションデータを作成する。
/// </summary>
Object3d::Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

} // namespace MyEngine::Object3dModelLoader
