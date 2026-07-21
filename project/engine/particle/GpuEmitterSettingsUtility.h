#pragma once

#include <string>
#include <vector>

namespace MyEngine::GpuEmitterSettingsUtility {

/// <summary>
/// GPU Emitter設定名をファイル名として使える文字だけに整える。
/// </summary>
std::string SanitizeName(const std::string& name);

/// <summary>
/// GPU Emitter設定ファイルの保存先パスを作成する。
/// </summary>
std::string BuildSettingsPath(const std::string& name);

/// <summary>
/// GPU Emitter設定として読み込めるJSONファイルを収集する。
/// </summary>
std::vector<std::string> CollectSettingsFiles();

/// <summary>
/// 設定名に対応する読み込み対象のJSONパスを取得する。
/// </summary>
std::string ResolveSettingsPath(const std::string& name, const std::vector<std::string>& files);

/// <summary>
/// プリセット名に対応するGPU Emitter設定ファイルを取得する。
/// </summary>
std::string ResolvePresetPath(const std::string& presetName);

} // namespace MyEngine::GpuEmitterSettingsUtility
