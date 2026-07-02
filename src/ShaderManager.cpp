#include "ShaderManager.h"
#include "Renderer.h"
#include <cassert>
#include <cstdio>

void ShaderManager::LoadVertexShader(const std::string& id, const char* fileName) {
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (!file) {
        // もしshader/フォルダの中にない場合は、実行フォルダパスや異なる階層を探すための代替処理
        // （KTEngineではshader/フォルダにHLSLがあり、csoはx64/Debugなどのビルド出力ディレクトリにあることが多い）
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

void ShaderManager::LoadPixelShader(const std::string& id, const char* fileName) {
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
        OutputDebugStringA("Pixel Shader file not found: ");
        OutputDebugStringA(fileName);
        OutputDebugStringA(" (Fallback to UnlitTexturePS)\n");
        if (_pixelShaderBinaries.count("UnlitTexture") > 0) {
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

    // ジオメトリパス中かつ不透明オブジェクトの場合、自動的にジオメトリパス用PSOに切り替える
    if (Renderer::IsGeometryPass() && blendMode == 0) {
        actualVsId = "Geometry";
        actualPsId = "Geometry";
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
            OutputDebugStringA("Requested VS/PS binary not found in map! Falling back to UnlitTexture.\n");
            if (_vertexShaderBinaries.count(actualVsId) == 0 && _vertexShaderBinaries.count("UnlitTexture") > 0) {
                _vertexShaderBinaries[actualVsId] = _vertexShaderBinaries["UnlitTexture"];
            }
            if (_pixelShaderBinaries.count(actualPsId) == 0 && _pixelShaderBinaries.count("UnlitTexture") > 0) {
                _pixelShaderBinaries[actualPsId] = _pixelShaderBinaries["UnlitTexture"];
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

    // 入力レイアウト定義 (POSITION, NORMAL, COLOR, TEXCOORD)
    static const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
        psoDesc.NumRenderTargets = 6;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Color
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Normal
        psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // Position (FP16 optimized)
        psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Metallic
        psoDesc.RTVFormats[4] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Specular
        psoDesc.RTVFormats[5] = DXGI_FORMAT_R8G8B8A8_UNORM;       // Roughness
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
        // 詳細なエラーログを出力
        FILE* fp = nullptr;
        fopen_s(&fp, "d3d12_log.txt", "a");
        if (fp) {
            fprintf(fp, "\n--- CreateGraphicsPipelineState Failed Details ---\n");
            fprintf(fp, "VS ID: %s, PS ID: %s\n", vsId.c_str(), psId.c_str());
            fprintf(fp, "HRESULT: 0x%08X\n", hr);
            fprintf(fp, "VS Size: %zu bytes, PS Size: %zu bytes\n", vsBin.size(), psBin.size());
            fprintf(fp, "BlendMode: %d, CullMode: %d, DepthEnable: %d, DepthWrite: %d, TopologyType: %d\n",
                blendMode, cullMode, depthEnable ? 1 : 0, depthWrite ? 1 : 0, (int)topologyType);
            fclose(fp);
        }
        // D3D12デバッグメッセージも強制出力
        Renderer::PrintDebugMessagesDX12();
        assert(false && "CreateGraphicsPipelineState failed!");
        return nullptr;
    }

    _pipelineStates[key] = pso;
    _lastKey = key;
    _lastPSO = pso.Get();
    return _lastPSO;
}
