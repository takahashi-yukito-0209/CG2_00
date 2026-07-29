#pragma once

namespace Easing {

/// <summary>
/// 入力値をそのまま補間率として返す。
/// </summary>
float Linear(float t);

/// <summary>
/// ゆっくり開始するサイン補間を行う。
/// </summary>
float InSine(float t);

/// <summary>
/// ゆっくり終了するサイン補間を行う。
/// </summary>
float OutSine(float t);

/// <summary>
/// 開始と終了をなめらかにするサイン補間を行う。
/// </summary>
float InOutSine(float t);

/// <summary>
/// ゆっくり開始する二次補間を行う。
/// </summary>
float InQuad(float t);

/// <summary>
/// ゆっくり終了する二次補間を行う。
/// </summary>
float OutQuad(float t);

/// <summary>
/// 開始と終了をなめらかにする二次補間を行う。
/// </summary>
float InOutQuad(float t);

/// <summary>
/// ゆっくり開始する三次補間を行う。
/// </summary>
float InCubic(float t);

/// <summary>
/// ゆっくり終了する三次補間を行う。
/// </summary>
float OutCubic(float t);

/// <summary>
/// 開始と終了をなめらかにする三次補間を行う。
/// </summary>
float InOutCubic(float t);

/// <summary>
/// 少し戻ってから進む補間を行う。
/// </summary>
float InBack(float t);

/// <summary>
/// 目標を少し越えてから戻る補間を行う。
/// </summary>
float OutBack(float t);

/// <summary>
/// 開始と終了に戻りを含める補間を行う。
/// </summary>
float InOutBack(float t);

/// <summary>
/// 終了時に跳ねる補間を行う。
/// </summary>
float OutBounce(float t);

/// <summary>
/// 開始時に跳ねる補間を行う。
/// </summary>
float InBounce(float t);

/// <summary>
/// 開始と終了に跳ねを含める補間を行う。
/// </summary>
float InOutBounce(float t);

} // namespace Easing