//=====================================================================================
// HotReloadManager.cpp
// Author:Kaito Aoki
// Date:2026/10/26
//=====================================================================================

#include "HotReloadManager.h"
#include "Manager.h"
#include "Renderer.h"
#include "ShaderManager.h"
#include "imgui.h"
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>
#include <algorithm>

HWND HotReloadManager::_hwnd = nullptr;
std::filesystem::path HotReloadManager::_projectRoot;
std::filesystem::path HotReloadManager::_shaderDir;
std::filesystem::path HotReloadManager::_srcDir;
std::filesystem::path HotReloadManager::_snapshotPath;

std::map<std::string, std::filesystem::file_time_type> HotReloadManager::_hlslTimestamps;
std::map<std::string, std::filesystem::file_time_type> HotReloadManager::_cppTimestamps;

bool HotReloadManager::_autoReloadOnFocus = true;
bool HotReloadManager::_focusCheckPending = false;
float HotReloadManager::_focusCheckDelayTimer = 0.0f;

std::atomic<bool> HotReloadManager::_isCompilingCpp{ false };
std::atomic<bool> HotReloadManager::_cppBuildFinished{ false };
std::atomic<bool> HotReloadManager::_cppBuildSucceeded{ false };
std::string HotReloadManager::_pendingNewExePath;
std::string HotReloadManager::_buildOutputLog;
std::mutex HotReloadManager::_logMutex;
std::thread HotReloadManager::_buildThread;
float HotReloadManager::_compileElapsedSeconds = 0.0f;

bool HotReloadManager::_syncOriginalExePending = false;
float HotReloadManager::_syncOriginalExeTimer = 0.0f;

HotReloadManager::ToastType HotReloadManager::_toastType = HotReloadManager::ToastType::None;
std::string HotReloadManager::_toastMessage;
float HotReloadManager::_toastTimer = 0.0f;
bool HotReloadManager::_showErrorModal = false;
std::string HotReloadManager::_errorModalTitle;
std::string HotReloadManager::_errorModalContent;

namespace {
    // Shift-JIS (CP932) 出力を UTF-8 に変換するヘルパー（MSBuild / FXC の日本語エラー文字化け防止）
    std::string ConvertSystemEncodingToUTF8(const std::string& input) {
        if (input.empty()) return "";
        int wlen = MultiByteToWideChar(CP_ACP, 0, input.c_str(), (int)input.size(), nullptr, 0);
        if (wlen <= 0) return input;
        std::wstring wstr(wlen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, input.c_str(), (int)input.size(), &wstr[0], wlen);

        int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) return input;
        std::string ustr(ulen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, &ustr[0], ulen, nullptr, nullptr);
        return ustr;
    }

    bool EndsWithIgnoreCase(const std::string& str, const std::string& suffix) {
        if (str.size() < suffix.size()) return false;
        return _stricmp(str.c_str() + str.size() - suffix.size(), suffix.c_str()) == 0;
    }
}

void HotReloadManager::LocateProjectRoot() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec) {
        candidates.push_back(cwd);
        candidates.push_back(cwd.parent_path());
        candidates.push_back(cwd.parent_path().parent_path());
    }

    char exePathBuf[MAX_PATH] = {};
    if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) > 0) {
        fs::path exeDir = fs::path(exePathBuf).parent_path();
        candidates.push_back(exeDir);
        candidates.push_back(exeDir.parent_path());
        candidates.push_back(exeDir.parent_path().parent_path());
        candidates.push_back(exeDir.parent_path().parent_path().parent_path());
    }

    for (const auto& dir : candidates) {
        if (!dir.empty() && fs::exists(dir / "KTEngine.vcxproj", ec)) {
            _projectRoot = fs::canonical(dir, ec);
            if (ec) _projectRoot = dir;
            break;
        }
    }

    if (_projectRoot.empty()) {
        _projectRoot = cwd;
    }

    _shaderDir = _projectRoot / "shader";
    _srcDir = _projectRoot / "src";
    _snapshotPath = _projectRoot / "hotreload_session.bin";
}

void HotReloadManager::ScanInitialTimestamps() {
    namespace fs = std::filesystem;
    std::error_code ec;

    _hlslTimestamps.clear();
    if (fs::exists(_shaderDir, ec)) {
        for (const auto& entry : fs::directory_iterator(_shaderDir, ec)) {
            if (entry.is_regular_file(ec) && EndsWithIgnoreCase(entry.path().extension().string(), ".hlsl")) {
                _hlslTimestamps[entry.path().string()] = entry.last_write_time(ec);
            }
        }
    }

    _cppTimestamps.clear();
    if (fs::exists(_srcDir, ec)) {
        for (const auto& entry : fs::recursive_directory_iterator(_srcDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            if (EndsWithIgnoreCase(ext, ".cpp") || EndsWithIgnoreCase(ext, ".h") || EndsWithIgnoreCase(ext, ".hpp")) {
                _cppTimestamps[entry.path().string()] = entry.last_write_time(ec);
            }
        }
    }
}

void HotReloadManager::Initialize(HWND hwnd) {
    _hwnd = hwnd;
    LocateProjectRoot();
    ScanInitialTimestamps();

    // ホットリロード引き継ぎスナップショットが存在する場合は復元する
    std::error_code ec;
    if (std::filesystem::exists(_snapshotPath, ec)) {
        if (Manager::LoadHotReloadSnapshot(_snapshotPath.string(), _hwnd)) {
            ShowToast("C++ Hot-Reload Complete!", false, 4.5f);
            _syncOriginalExePending = true;
            _syncOriginalExeTimer = 0.8f;
        }
        std::filesystem::remove(_snapshotPath, ec);
    }
}

void HotReloadManager::Shutdown() {
    if (_buildThread.joinable()) {
        _buildThread.join();
    }
}

void HotReloadManager::OnWindowActivate(bool active) {
    if (active && _autoReloadOnFocus && !_isCompilingCpp.load()) {
        // エディタ側のファイル書き込み完了を待つため微小ディレイを入れる
        _focusCheckPending = true;
        _focusCheckDelayTimer = 0.15f;
    }
}

void HotReloadManager::ShowToast(const std::string& message, bool isError, float duration) {
    _toastType = isError ? ToastType::Error : ToastType::Success;
    _toastMessage = message;
    _toastTimer = duration;
}

bool HotReloadManager::CompileSingleHLSL(const std::filesystem::path& hlslPath, std::string& outError) {
    std::string stem = hlslPath.stem().string();
    bool isVS = false;
    std::string targetProfile;

    if (EndsWithIgnoreCase(stem, "VS")) {
        isVS = true;
        targetProfile = "vs_5_0";
    } else if (EndsWithIgnoreCase(stem, "PS")) {
        isVS = false;
        targetProfile = "ps_5_0";
    } else {
        // Common.hlsl 等のインクルード専用ファイルは単体コンパイル対象外
        return true;
    }

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    std::wstring wPath = hlslPath.wstring();
    HRESULT hr = D3DCompileFromFile(
        wPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        targetProfile.c_str(),
        compileFlags,
        0,
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr) || !shaderBlob) {
        if (errorBlob && errorBlob->GetBufferPointer()) {
            std::string rawErr((const char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
            outError = ConvertSystemEncodingToUTF8(rawErr);
        } else {
            char buf[128];
            sprintf_s(buf, "D3DCompileFromFile failed (HRESULT = 0x%08X)", (unsigned int)hr);
            outError = buf;
        }
        return false;
    }

    std::vector<unsigned char> binary(shaderBlob->GetBufferSize());
    memcpy(binary.data(), shaderBlob->GetBufferPointer(), binary.size());

    // 1. プロジェクトの shader/<stem>.cso を更新
    std::error_code ec;
    std::filesystem::path csoInShaderDir = _shaderDir / (stem + ".cso");
    {
        std::ofstream ofs(csoInShaderDir, std::ios::binary);
        if (ofs.is_open()) {
            ofs.write((const char*)binary.data(), binary.size());
        }
    }

    // 2. 実行ファイル側の shader/<stem>.cso も更新
    char exePathBuf[MAX_PATH] = {};
    if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) > 0) {
        std::filesystem::path exeShaderDir = std::filesystem::path(exePathBuf).parent_path() / "shader";
        if (std::filesystem::exists(exeShaderDir, ec)) {
            std::ofstream ofs(exeShaderDir / (stem + ".cso"), std::ios::binary);
            if (ofs.is_open()) {
                ofs.write((const char*)binary.data(), binary.size());
            }
        }
    }

    // 3. GPU待機の上、ShaderManagerのバイナリとPSOキャッシュを更新
    Renderer::FlushGPUDX12();
    ShaderManager::Instance().ReloadShaderFromBinary(stem, isVS, binary);

    // ShadowVS の場合は RendererDX12 側の専用シャドウPSOも再生成する
    if (_stricmp(stem.c_str(), "ShadowVS") == 0) {
        Renderer::RecreateShadowPSODX12();
    }

    return true;
}

void HotReloadManager::ForceRecompileAllShaders() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(_shaderDir, ec)) {
        ShowToast("Shader directory not found!", true);
        return;
    }

    int successCount = 0;
    std::string combinedErrors;

    for (const auto& entry : fs::directory_iterator(_shaderDir, ec)) {
        if (!entry.is_regular_file(ec) || !EndsWithIgnoreCase(entry.path().extension().string(), ".hlsl"))
            continue;

        _hlslTimestamps[entry.path().string()] = entry.last_write_time(ec);
        std::string stem = entry.path().stem().string();
        if (!EndsWithIgnoreCase(stem, "VS") && !EndsWithIgnoreCase(stem, "PS"))
            continue;

        std::string err;
        if (CompileSingleHLSL(entry.path(), err)) {
            successCount++;
        } else {
            combinedErrors += "[" + entry.path().filename().string() + "]\n" + err + "\n";
        }
    }

    if (combinedErrors.empty()) {
        ShowToast("Recompiled all shaders (" + std::to_string(successCount) + " files)", false);
    } else {
        ShowToast("Shader compilation failed!", true, 6.0f);
        _errorModalTitle = "HLSL Shader Compile Error";
        _errorModalContent = combinedErrors;
        _showErrorModal = true;
    }
}

std::string HotReloadManager::FindMSBuildExe() {
    namespace fs = std::filesystem;
    std::error_code ec;

    const char* knownPaths[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe"
    };

    for (const char* path : knownPaths) {
        if (fs::exists(path, ec)) {
            return path;
        }
    }

    // vswhere.exe による動的検索
    const char* vswherePath = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (fs::exists(vswherePath, ec)) {
        std::string cmd = "\"" + std::string(vswherePath) + "\" -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char line[512] = {};
            if (fgets(line, sizeof(line), pipe)) {
                std::string found(line);
                while (!found.empty() && (found.back() == '\r' || found.back() == '\n')) {
                    found.pop_back();
                }
                _pclose(pipe);
                if (fs::exists(found, ec)) {
                    return found;
                }
            } else {
                _pclose(pipe);
            }
        }
    }

    return "";
}

void HotReloadManager::StartCppShadowBuild(const std::vector<std::string>& changedFiles) {
    if (_isCompilingCpp.load()) return;

    std::string msbuildExe = FindMSBuildExe();
    if (msbuildExe.empty()) {
        ShowToast("MSBuild.exe not found!", true, 5.0f);
        return;
    }

    if (_buildThread.joinable()) {
        _buildThread.join();
    }

    // 現在実行中のexe名からピンポン先のTargetNameを決定する（ファイルロック回避）
    char exePathBuf[MAX_PATH] = {};
    GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
    std::filesystem::path currentExePath(exePathBuf);
    std::string currentStem = currentExePath.stem().string();

    std::string nextTargetName = "KTEngine_HotReload_A";
    if (_stricmp(currentStem.c_str(), "KTEngine_HotReload_A") == 0) {
        nextTargetName = "KTEngine_HotReload_B";
    }

#ifdef _DEBUG
    std::string config = "Debug";
#else
    std::string config = "Release";
#endif

    // 出力先ディレクトリの特定
    std::filesystem::path outDir = _projectRoot / "x64" / config;
    std::error_code ec;
    if (!std::filesystem::exists(outDir, ec)) {
        outDir = currentExePath.parent_path();
    }
    _pendingNewExePath = (outDir / (nextTargetName + ".exe")).string();

    // 前回のピンポン先ファイルが残っていれば事前削除を試みる
    std::filesystem::remove(outDir / (nextTargetName + ".exe"), ec);
    std::filesystem::remove(outDir / (nextTargetName + ".pdb"), ec);

    _isCompilingCpp.store(true);
    _cppBuildFinished.store(false);
    _cppBuildSucceeded.store(false);
    _compileElapsedSeconds = 0.0f;
    _toastType = ToastType::Compiling;

    std::string fileLabel = changedFiles.empty() ? "C++ project" : std::filesystem::path(changedFiles[0]).filename().string();
    if (changedFiles.size() > 1) {
        fileLabel += " (+" + std::to_string(changedFiles.size() - 1) + " files)";
    }
    _toastMessage = "Compiling " + fileLabel + "...";

    std::filesystem::path vcxprojPath = _projectRoot / "KTEngine.vcxproj";

    _buildThread = std::thread([msbuildExe, vcxprojPath, config, nextTargetName]() {
        std::string cmdLine = "\"" + msbuildExe + "\" \"" + vcxprojPath.string() + "\""
            + " /p:Configuration=" + config
            + " /p:Platform=x64"
            + " /p:TargetName=" + nextTargetName
            + " /p:PostBuildEventUseInBuild=false"
            + " /m /nologo /v:minimal";

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hReadPipe = NULL;
        HANDLE hWritePipe = NULL;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            _cppBuildSucceeded.store(false);
            _isCompilingCpp.store(false);
            _cppBuildFinished.store(true);
            return;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(STARTUPINFOA);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        std::string workDir = vcxprojPath.parent_path().string();
        BOOL created = CreateProcessA(
            nullptr,
            cmdBuf.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            workDir.c_str(),
            &si,
            &pi
        );

        CloseHandle(hWritePipe);

        std::string rawOutput;
        if (created) {
            char buffer[1024];
            DWORD bytesRead = 0;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                rawOutput.append(buffer, bytesRead);
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 1;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);

            {
                std::lock_guard<std::mutex> lock(_logMutex);
                _buildOutputLog = ConvertSystemEncodingToUTF8(rawOutput);
            }

            _cppBuildSucceeded.store(exitCode == 0);
        } else {
            CloseHandle(hReadPipe);
            {
                std::lock_guard<std::mutex> lock(_logMutex);
                _buildOutputLog = "Failed to launch MSBuild process.";
            }
            _cppBuildSucceeded.store(false);
        }

        _isCompilingCpp.store(false);
        _cppBuildFinished.store(true);
    });
}

void HotReloadManager::ForceRebuildCpp() {
    StartCppShadowBuild({});
}

void HotReloadManager::CheckAndReloadModified() {
    if (_isCompilingCpp.load()) return;

    namespace fs = std::filesystem;
    std::error_code ec;

    // 1. HLSLファイルの変更検知
    std::vector<fs::path> modifiedHlsl;
    bool commonHlslChanged = false;

    if (fs::exists(_shaderDir, ec)) {
        for (const auto& entry : fs::directory_iterator(_shaderDir, ec)) {
            if (!entry.is_regular_file(ec) || !EndsWithIgnoreCase(entry.path().extension().string(), ".hlsl"))
                continue;

            std::string key = entry.path().string();
            auto writeTime = entry.last_write_time(ec);
            auto it = _hlslTimestamps.find(key);
            if (it == _hlslTimestamps.end() || it->second != writeTime) {
                _hlslTimestamps[key] = writeTime;
                std::string stem = entry.path().stem().string();
                if (_stricmp(stem.c_str(), "Common") == 0) {
                    commonHlslChanged = true;
                } else {
                    modifiedHlsl.push_back(entry.path());
                }
            }
        }
    }

    if (commonHlslChanged) {
        ForceRecompileAllShaders();
    } else if (!modifiedHlsl.empty()) {
        std::string combinedErrors;
        std::vector<std::string> reloadedNames;
        for (const auto& path : modifiedHlsl) {
            std::string err;
            if (CompileSingleHLSL(path, err)) {
                reloadedNames.push_back(path.filename().string());
            } else {
                combinedErrors += "[" + path.filename().string() + "]\n" + err + "\n";
            }
        }

        if (combinedErrors.empty()) {
            std::string msg = "Hot-Reloaded Shader: " + reloadedNames[0];
            if (reloadedNames.size() > 1) {
                msg += " (+" + std::to_string(reloadedNames.size() - 1) + ")";
            }
            ShowToast(msg, false, 4.0f);
        } else {
            ShowToast("Shader Compile Error!", true, 6.0f);
            _errorModalTitle = "HLSL Shader Compile Error";
            _errorModalContent = combinedErrors;
            _showErrorModal = true;
        }
    }

    // 2. C++ (.cpp / .h / .hpp) ファイルの変更検知
    std::vector<std::string> modifiedCpp;
    if (fs::exists(_srcDir, ec)) {
        for (const auto& entry : fs::recursive_directory_iterator(_srcDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            if (!EndsWithIgnoreCase(ext, ".cpp") && !EndsWithIgnoreCase(ext, ".h") && !EndsWithIgnoreCase(ext, ".hpp"))
                continue;

            std::string key = entry.path().string();
            auto writeTime = entry.last_write_time(ec);
            auto it = _cppTimestamps.find(key);
            if (it == _cppTimestamps.end() || it->second != writeTime) {
                _cppTimestamps[key] = writeTime;
                modifiedCpp.push_back(key);
            }
        }
    }

    if (!modifiedCpp.empty()) {
        StartCppShadowBuild(modifiedCpp);
    }
}

void HotReloadManager::Update() {
    float dt = ImGui::GetIO().DeltaTime;

    // Ctrl+R ショートカット検知
    if (!ImGui::GetIO().WantTextInput && !_isCompilingCpp.load()) {
        bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrlDown && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            CheckAndReloadModified();
        }
    }

    // フォーカス復帰時のディレイチェック
    if (_focusCheckPending) {
        _focusCheckDelayTimer -= dt;
        if (_focusCheckDelayTimer <= 0.0f) {
            _focusCheckPending = false;
            CheckAndReloadModified();
        }
    }

    // 新プロセス起動後に旧プロセスが終了したタイミングで、KTEngine.exe 本体にも最新バイナリを同期コピーする
    if (_syncOriginalExePending) {
        _syncOriginalExeTimer -= dt;
        if (_syncOriginalExeTimer <= 0.0f) {
            _syncOriginalExePending = false;
            char exePathBuf[MAX_PATH] = {};
            if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) > 0) {
                std::filesystem::path currentExe(exePathBuf);
                std::string stem = currentExe.stem().string();
                std::error_code ec;
                if (_stricmp(stem.c_str(), "KTEngine") != 0) {
                    std::filesystem::path mainExe = currentExe.parent_path() / "KTEngine.exe";
                    std::filesystem::copy_file(currentExe, mainExe, std::filesystem::copy_options::overwrite_existing, ec);
                }
                // テスト用や反対側の古いシャドウexeがあれば掃除
                std::filesystem::remove(currentExe.parent_path() / "KTEngine_HotReload.exe", ec);
                std::filesystem::remove(currentExe.parent_path() / "KTEngine_HotReload.pdb", ec);
            }
        }
    }

    if (_isCompilingCpp.load()) {
        _compileElapsedSeconds += dt;
    }

    // バックグラウンドC++ビルド完了時の処理
    if (_cppBuildFinished.load()) {
        _cppBuildFinished.store(false);
        if (_buildThread.joinable()) {
            _buildThread.join();
        }

        if (_cppBuildSucceeded.load()) {
            // 1. 現在のエディタ・ランタイム・カメラ・UI状態をスナップショット保存
            if (Manager::SaveHotReloadSnapshot(_snapshotPath.string(), _hwnd)) {
                // 2. 新しい実行ファイルを起動して現在のプロセスを終了
                STARTUPINFOA si = {};
                si.cb = sizeof(STARTUPINFOA);
                PROCESS_INFORMATION pi = {};

                std::string cmdLine = "\"" + _pendingNewExePath + "\" --hotreload";
                std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
                cmdBuf.push_back('\0');

                std::error_code ec;
                std::string cwd = std::filesystem::current_path(ec).string();

                if (CreateProcessA(
                    _pendingNewExePath.c_str(),
                    cmdBuf.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    0,
                    nullptr,
                    cwd.empty() ? nullptr : cwd.c_str(),
                    &si,
                    &pi
                )) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    PostQuitMessage(0);
                    return;
                } else {
                    ShowToast("Failed to launch new executable!", true, 5.0f);
                }
            } else {
                ShowToast("Failed to save hot-reload session snapshot!", true, 5.0f);
            }
        } else {
            ShowToast("C++ Hot-Reload Build Failed!", true, 6.0f);
            {
                std::lock_guard<std::mutex> lock(_logMutex);
                _errorModalTitle = "C++ Build Error (MSBuild)";
                _errorModalContent = _buildOutputLog;
            }
            _showErrorModal = true;
        }
    }

    if (_toastType == ToastType::Success || _toastType == ToastType::Error) {
        _toastTimer -= dt;
        if (_toastTimer <= 0.0f) {
            _toastType = ToastType::None;
        }
    }
}

void HotReloadManager::RenderMenuItems() {
    if (ImGui::BeginMenu("Hot Reload")) {
        ImGui::MenuItem("Auto Reload on Window Focus", nullptr, &_autoReloadOnFocus);
        ImGui::Separator();
        if (ImGui::MenuItem("Reload Modified Files", "Ctrl+R", false, !_isCompilingCpp.load())) {
            CheckAndReloadModified();
        }
        if (ImGui::MenuItem("Recompile All Shaders (.hlsl)", nullptr, false, !_isCompilingCpp.load())) {
            ForceRecompileAllShaders();
        }
        if (ImGui::MenuItem("Force Rebuild C++ (.cpp/.h)", nullptr, false, !_isCompilingCpp.load())) {
            ForceRebuildCpp();
        }
        if (!_errorModalContent.empty()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Show Last Compile Output")) {
                _showErrorModal = true;
            }
        }
        ImGui::EndMenu();
    }
}

void HotReloadManager::RenderUI() {
    // 1. 右下トースト通知（Unity風コンパイルインジケータ / 完了通知）
    if (_toastType != ToastType::None) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;
        ImVec2 windowPos = ImVec2(workPos.x + workSize.x - 16.0f, workPos.y + workSize.y - 16.0f);

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.92f);

        ImVec4 borderCol = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        if (_toastType == ToastType::Compiling) {
            borderCol = ImVec4(0.95f, 0.65f, 0.15f, 1.0f);
        } else if (_toastType == ToastType::Success) {
            borderCol = ImVec4(0.20f, 0.80f, 0.35f, 1.0f);
        } else if (_toastType == ToastType::Error) {
            borderCol = ImVec4(0.90f, 0.25f, 0.25f, 1.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Border, borderCol);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("##HotReloadToast", nullptr, flags)) {
            if (_toastType == ToastType::Compiling) {
                const char* spinnerFrames[] = { "|", "/", "-", "\\" };
                int frame = (int)(ImGui::GetTime() * 10.0) % 4;
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "[%s] %s (%.1fs)", spinnerFrames[frame], _toastMessage.c_str(), _compileElapsedSeconds);
            } else if (_toastType == ToastType::Success) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.48f, 1.0f), "[OK] %s", _toastMessage.c_str());
            } else if (_toastType == ToastType::Error) {
                ImGui::TextColored(ImVec4(1.0f, 0.38f, 0.38f, 1.0f), "[ERROR] %s", _toastMessage.c_str());
                if (ImGui::SmallButton("View Error Log")) {
                    _showErrorModal = true;
                }
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    // 2. コンパイルエラー表示ウィンドウ
    if (_showErrorModal) {
        ImGui::SetNextWindowSize(ImVec2(780.0f, 420.0f), ImGuiCond_FirstUseEver);
        std::string winTitle = (_errorModalTitle.empty() ? "Compile Error" : _errorModalTitle) + "###HotReloadErrorWindow";
        if (ImGui::Begin(winTitle.c_str(), &_showErrorModal)) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Compilation failed. Fix the errors below and save (or press Ctrl+R) to retry:");
            ImGui::Separator();

            ImVec2 avail = ImGui::GetContentRegionAvail();
            avail.y -= 36.0f;
            if (avail.y < 100.0f) avail.y = 100.0f;

            ImGui::BeginChild("##ErrorLogScroll", avail, true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(_errorModalContent.c_str());
            ImGui::EndChild();

            if (ImGui::Button("Retry Reload (Ctrl+R)", ImVec2(180.0f, 28.0f))) {
                _showErrorModal = false;
                CheckAndReloadModified();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy to Clipboard", ImVec2(150.0f, 28.0f))) {
                ImGui::SetClipboardText(_errorModalContent.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(100.0f, 28.0f))) {
                _showErrorModal = false;
            }
        }
        ImGui::End();
    }
}

