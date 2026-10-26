//=====================================================================================
// HotReloadManager.h
// Author:Kaito Aoki
// Date:2026/10/26
//=====================================================================================

#ifndef _HOT_RELOAD_MANAGER_H
#define _HOT_RELOAD_MANAGER_H

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <thread>

class HotReloadManager {
private:
    static HWND _hwnd;
    static std::filesystem::path _projectRoot;
    static std::filesystem::path _shaderDir;
    static std::filesystem::path _srcDir;
    static std::filesystem::path _snapshotPath;

    static std::map<std::string, std::filesystem::file_time_type> _hlslTimestamps;
    static std::map<std::string, std::filesystem::file_time_type> _cppTimestamps;

    static bool _autoReloadOnFocus;
    static bool _focusCheckPending;
    static float _focusCheckDelayTimer;

    // バックグラウンドC++ビルド状態
    static std::atomic<bool> _isCompilingCpp;
    static std::atomic<bool> _cppBuildFinished;
    static std::atomic<bool> _cppBuildSucceeded;
    static std::string _pendingNewExePath;
    static std::string _buildOutputLog;
    static std::mutex _logMutex;
    static std::thread _buildThread;
    static float _compileElapsedSeconds;

    // 古いexeへの書き戻し同期用タイマー
    static bool _syncOriginalExePending;
    static float _syncOriginalExeTimer;

    // UI通知トースト・エラーモーダル用
    enum class ToastType {
        None,
        Compiling,
        Success,
        Error
    };
    static ToastType _toastType;
    static std::string _toastMessage;
    static float _toastTimer;
    static bool _showErrorModal;
    static std::string _errorModalTitle;
    static std::string _errorModalContent;

    static void LocateProjectRoot();
    static void ScanInitialTimestamps();
    static std::string FindMSBuildExe();
    static bool CompileSingleHLSL(const std::filesystem::path& hlslPath, std::string& outError);
    static void StartCppShadowBuild(const std::vector<std::string>& changedFiles);

public:
    static void Initialize(HWND hwnd);
    static void Shutdown();
    static void Update();
    static void RenderUI();
    static void RenderMenuItems();

    static void OnWindowActivate(bool active);

    static void CheckAndReloadModified();
    static void ForceRecompileAllShaders();
    static void ForceRebuildCpp();

    static bool IsAutoReloadEnabled() { return _autoReloadOnFocus; }
    static void SetAutoReloadEnabled(bool enabled) { _autoReloadOnFocus = enabled; }
    static bool IsCompilingCpp() { return _isCompilingCpp.load(); }
    static void ShowToast(const std::string& message, bool isError = false, float duration = 4.0f);
};

#endif // !_HOT_RELOAD_MANAGER_H

