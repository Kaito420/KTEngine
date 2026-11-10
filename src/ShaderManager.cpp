#include "ShaderManager.h"
#include "Renderer.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <d3dcompiler.h>

static std::string ExtractShaderStem(const char* fileName) {
    if (!fileName) return "";
    return std::filesystem::path(fileName).stem().string();
}

static void RegisterStemMapping(std::map<std::string, std::vector<std::string>>& map, const std::string& stem, const std::string& id) {
    if (stem.empty()) return;
    auto& vec = map[stem];
    if (std::find(vec.begin(), vec.end(), id) == vec.end()) {
        vec.push_back(id);
    }
}

static bool IsCustomGeometryPixelShaderID(const std::string& psId) {
    if (psId.empty() || psId == "Geometry" || psId == "DirectionalLight" ||
        psId == "VertexDirectionalLightingPS" || psId == "UnlitTexture" ||
        psId == "UnlitTexturePS" || psId == "UnlitColor" || psId == "UnlitColorPS" ||
        psId == "Toon" || psId == "ToonPS") {
        return false;
    }
    if (psId.size() >= 4 && _strnicmp(psId.c_str(), "Proc", 4) == 0) {
        return true;
    }
    return false;
}

void ShaderManager::LoadVertexShader(const std::string& id, const char* fileName) {
    RegisterStemMapping(_vsFileToIds, ExtractShaderStem(fileName), id);
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (!file) {
        char altPath[512];
        sprintf_s(altPath, "shader/%s", fileName);
        fopen_s(&file, altPath, "rb");
        if (!file) {
            sprintf_s(altPath, "../shader/%s", fileName);
            fopen_s(&file, altPath, "rb");
        }
    }

    if (!file) {
        OutputDebugStringA("Vertex Shader file not found: ");
        OutputDebugStringA(fileName);
        OutputDebugStringA(" (Fallback to UnlitTextureVS)\n");
        if (_vertexShaderBinaries.count("UnlitTexture") > 0) {
            _vertexShaderBinaries[id] = _vertexShaderBinaries["UnlitTexture"];
        }
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::vector<unsigned char> buffer(size);
    fread(buffer.data(), 1, size, file);
    fclose(file);

    _vertexShaderBinaries[id] = std::move(buffer);
}

bool ShaderManager::CompileAndRegisterPixelShaderHLSL(const std::string& psId, const std::filesystem::path& hlslPath, std::string& outError) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path resolvedPath = hlslPath;
    if (!fs::exists(resolvedPath, ec)) {
        fs::path alt1 = fs::path("shader") / hlslPath.filename();
        fs::path alt2 = fs::path("../shader") / hlslPath.filename();
        if (fs::exists(alt1, ec)) resolvedPath = alt1;
        else if (fs::exists(alt2, ec)) resolvedPath = alt2;
        else {
            outError = "HLSL file not found: " + hlslPath.string();
            return false;
        }
    }

    std::string stem = resolvedPath.stem().string();
    RegisterStemMapping(_psFileToIds, stem, psId);
    if (psId != stem) {
        RegisterStemMapping(_psFileToIds, stem, stem);
    }

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;
    std::wstring wPath = resolvedPath.wstring();
    HRESULT hr = D3DCompileFromFile(
        wPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr) || !shaderBlob) {
        if (errorBlob && errorBlob->GetBufferPointer()) {
            outError.assign((const char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
        } else {
            outError = "D3DCompileFromFile failed for " + resolvedPath.string();
        }
        return false;
    }

    std::vector<unsigned char> binary(shaderBlob->GetBufferSize());
    memcpy(binary.data(), shaderBlob->GetBufferPointer(), binary.size());

    // Save .cso next to .hlsl
    fs::path csoPath = resolvedPath.parent_path() / (stem + ".cso");
    {
        std::ofstream ofs(csoPath, std::ios::binary);
        if (ofs.is_open()) {
            ofs.write((const char*)binary.data(), binary.size());
        }
    }

    Renderer::FlushGPUDX12();
    _pixelShaderBinaries[psId] = binary;
    _pixelShaderBinaries[stem] = binary;
    ClearPSOCache();
    outError = "Compiled " + resolvedPath.filename().string() + " successfully (ps_5_0).";
    return true;
}

void ShaderManager::LoadPixelShader(const std::string& id, const char* fileName) {
    std::string stem = ExtractShaderStem(fileName);
    RegisterStemMapping(_psFileToIds, stem, id);
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (!file) {
        char altPath[512];
        sprintf_s(altPath, "shader/%s", fileName);
        fopen_s(&file, altPath, "rb");
        if (!file) {
            sprintf_s(altPath, "../shader/%s", fileName);
            fopen_s(&file, altPath, "rb");
        }
    }

    if (!file) {
        // Attempt on-the-fly compilation if shader/<stem>.hlsl exists
        std::filesystem::path hlslCandidate = std::filesystem::path("shader") / (stem + ".hlsl");
        std::error_code ec;
        if (std::filesystem::exists(hlslCandidate, ec)) {
            std::string compileErr;
            if (CompileAndRegisterPixelShaderHLSL(id, hlslCandidate, compileErr)) {
                return;
            }
        }

        OutputDebugStringA("Pixel Shader file not found: ");
        OutputDebugStringA(fileName);
        OutputDebugStringA(" (Fallback to Geometry / UnlitTexturePS)\n");
        if (IsCustomGeometryPixelShaderID(id) && _pixelShaderBinaries.count("Geometry") > 0) {
            _pixelShaderBinaries[id] = _pixelShaderBinaries["Geometry"];
        } else if (_pixelShaderBinaries.count("UnlitTexture") > 0) {
            _pixelShaderBinaries[id] = _pixelShaderBinaries["UnlitTexture"];
        }
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::vector<unsigned char> buffer(size);
    fread(buffer.data(), 1, size, file);
    fclose(file);

    _pixelShaderBinaries[id] = std::move(buffer);
}

ID3D12PipelineState* ShaderManager::GetPipelineState(
    const std::string& vsId,
    const std::string& psId,
    int blendMode,
    int cullMode,
    bool depthEnable,
    bool depthWrite,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType
) {
    // ジオメトリパス中は不透明(blendMode == 0)のみ描画
    if (Renderer::IsGeometryPass() && blendMode != 0) {
        return nullptr;
    }
    // フォワードパス中は透過(blendMode != 0)のみ描画
    if (!Renderer::IsGeometryPass() && blendMode == 0) {
        return nullptr;
    }

    std::string actualVsId = vsId;
    std::string actualPsId = psId;

    // ジオメトリパス中かつ不透明オブジェクトの場合、VSはGeometry固定、PSはカスタムG-Buffer PS指定があればそれを使用
    if (Renderer::IsGeometryPass() && blendMode == 0) {
        actualVsId = "Geometry";
        if (!IsCustomGeometryPixelShaderID(psId)) {
            actualPsId = "Geometry";
        }
    }

    PipelineStateKey key = { actualVsId, actualPsId, blendMode, cullMode, depthEnable, depthWrite, topologyType };
    if (_lastPSO != nullptr && key == _lastKey) {
        return _lastPSO;
    }
    if (_pipelineStates.count(key) > 0) {
        _lastKey = key;
        _lastPSO = _pipelineStates[key].Get();
        return _lastPSO;
    }

    // 各シェーダーバイナリ取得
    if (_vertexShaderBinaries.count(actualVsId) == 0 || _pixelShaderBinaries.count(actualPsId) == 0) {
        if (_vertexShaderBinaries.count(actualVsId) == 0) {
            std::string fileName = actualVsId + ".cso";
            LoadVertexShader(actualVsId, fileName.c_str());
        }
        if (_pixelShaderBinaries.count(actualPsId) == 0) {
            std::string fileName = actualPsId + ".cso";
            LoadPixelShader(actualPsId, fileName.c_str());
        }
        
        // 予備チェック
        if (_vertexShaderBinaries.count(actualVsId) == 0 || _pixelShaderBinaries.count(actualPsId) == 0) {
            OutputDebugStringA("Requested VS/PS binary not found in map! Falling back to Geometry/UnlitTexture.\n");
            if (_vertexShaderBinaries.count(actualVsId) == 0 && _vertexShaderBinaries.count("UnlitTexture") > 0) {
                _vertexShaderBinaries[actualVsId] = _vertexShaderBinaries["UnlitTexture"];
            }
            if (_pixelShaderBinaries.count(actualPsId) == 0) {
                if (actualVsId == "Geometry" && _pixelShaderBinaries.count("Geometry") > 0) {
                    _pixelShaderBinaries[actualPsId] = _pixelShaderBinaries["Geometry"];
                } else if (_pixelShaderBinaries.count("UnlitTexture") > 0) {
                    _pixelShaderBinaries[actualPsId] = _pixelShaderBinaries["UnlitTexture"];
                }
            }
        }
    }

    const auto& vsBin = _vertexShaderBinaries[actualVsId];
    const auto& psBin = _pixelShaderBinaries[actualPsId];

    // PSO作成のDESC定義
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = Renderer::GetRootSignatureDX12();
    assert(psoDesc.pRootSignature != nullptr && "Root Signature is null when creating Pipeline State!");

    psoDesc.VS.pShaderBytecode = vsBin.data();
    psoDesc.VS.BytecodeLength = vsBin.size();
    psoDesc.PS.pShaderBytecode = psBin.data();
    psoDesc.PS.BytecodeLength = psBin.size();

    // 入力レイアウト定義 (POSITION, NORMAL, COLOR, TEXCOORD, BLENDINDICES, BLENDWEIGHT)
    static const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    psoDesc.InputLayout.pInputElementDescs = inputElementDescs;
    psoDesc.InputLayout.NumElements = _countof(inputElementDescs);

    // ブレンドステートの設定
    D3D12_RENDER_TARGET_BLEND_DESC rtBlend = {};
    if (blendMode == 0) { // Opaque
        rtBlend.BlendEnable = FALSE;
        rtBlend.LogicOpEnable = FALSE;
        rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    } else if (blendMode == 1) { // AlphaBlend
        rtBlend.BlendEnable = TRUE;
        rtBlend.LogicOpEnable = FALSE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    } else if (blendMode == 2) { // Additive
        rtBlend.BlendEnable = TRUE;
        rtBlend.LogicOpEnable = FALSE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_ONE;
        rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    
    if (actualVsId == "Geometry" || actualPsId == "Geometry") {
        psoDesc.BlendState.IndependentBlendEnable = TRUE;
        for (int i = 0; i < 6; i++) {
            psoDesc.BlendState.RenderTarget[i] = rtBlend;
        }
    } else {
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0] = rtBlend;
    }

    // ラスタライザステート
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = (D3D12_CULL_MODE)cullMode;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // 深度ステンシルステート
    psoDesc.DepthStencilState.DepthEnable = depthEnable ? TRUE : FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    // プリミティブトポロジとレンダーターゲット設定
    psoDesc.PrimitiveTopologyType = topologyType;
    
    if (actualVsId == "Geometry" || actualPsId == "Geometry") {
        psoDesc.NumRenderTargets = 4;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Color
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Normal
        psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Position
        psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Material (ARM + ShadingModel)
    } else {
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    
    psoDesc.DSVFormat = depthEnable ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.SampleMask = UINT_MAX;

    ID3D12Device* device = Renderer::GetDeviceDX12();
    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        FILE* fp = nullptr;
        fopen_s(&fp, "d3d12_log.txt", "a");
        if (fp) {
            fprintf(fp, "\n--- CreateGraphicsPipelineState Failed Details ---\n");
            fprintf(fp, "VS ID: %s, PS ID: %s\n", actualVsId.c_str(), actualPsId.c_str());
            fprintf(fp, "HRESULT: 0x%08X\n", hr);
            fprintf(fp, "VS Size: %zu bytes, PS Size: %zu bytes\n", vsBin.size(), psBin.size());
            fprintf(fp, "BlendMode: %d, CullMode: %d, DepthEnable: %d, DepthWrite: %d, TopologyType: %d\n",
                blendMode, cullMode, depthEnable ? 1 : 0, depthWrite ? 1 : 0, (int)topologyType);
            fclose(fp);
        }
        Renderer::PrintDebugMessagesDX12();
        OutputDebugStringA("CreateGraphicsPipelineState failed!\n");
        return nullptr;
    }

    _pipelineStates[key] = pso;
    _lastKey = key;
    _lastPSO = pso.Get();
    return _lastPSO;
}

const std::vector<unsigned char>& ShaderManager::GetVertexShaderBinary(const std::string& id) const {
    auto it = _vertexShaderBinaries.find(id);
    if (it != _vertexShaderBinaries.end()) {
        return it->second;
    }
    static const std::vector<unsigned char> empty;
    return empty;
}

bool ShaderManager::ReloadShaderFromBinary(const std::string& stem, bool isVS, const std::vector<unsigned char>& binary) {
    if (binary.empty()) return false;
    bool updated = false;
    if (isVS) {
        auto it = _vsFileToIds.find(stem);
        if (it != _vsFileToIds.end()) {
            for (const auto& id : it->second) {
                _vertexShaderBinaries[id] = binary;
                updated = true;
            }
        }
        if (_vertexShaderBinaries.count(stem) > 0) {
            _vertexShaderBinaries[stem] = binary;
            updated = true;
        }
    } else {
        auto it = _psFileToIds.find(stem);
        if (it != _psFileToIds.end()) {
            for (const auto& id : it->second) {
                _pixelShaderBinaries[id] = binary;
                updated = true;
            }
        }
        if (_pixelShaderBinaries.count(stem) > 0) {
            _pixelShaderBinaries[stem] = binary;
            updated = true;
        }
    }
    if (updated) {
        ClearPSOCache();
    }
    return updated;
}

void ShaderManager::ClearPSOCache() {
    _pipelineStates.clear();
    _lastKey = {};
    _lastPSO = nullptr;
}
