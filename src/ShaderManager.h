#ifndef _SHADERMANAGER_H
#define _SHADERMANAGER_H

#include <map>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct PipelineStateKey {
    std::string vsId;
    std::string psId;
    int blendMode;       // 0: Opaque, 1: AlphaBlend, 2: Additive
    int cullMode;        // D3D12_CULL_MODE
    bool depthEnable;
    bool depthWrite;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;

    bool operator<(const PipelineStateKey& other) const {
        if (vsId != other.vsId) return vsId < other.vsId;
        if (psId != other.psId) return psId < other.psId;
        if (blendMode != other.blendMode) return blendMode < other.blendMode;
        if (cullMode != other.cullMode) return cullMode < other.cullMode;
        if (depthEnable != other.depthEnable) return depthEnable < other.depthEnable;
        if (depthWrite != other.depthWrite) return depthWrite < other.depthWrite;
        return topologyType < other.topologyType;
    }
};

class ShaderManager {
private:
    std::map<std::string, std::vector<unsigned char>> _vertexShaderBinaries;
    std::map<std::string, std::vector<unsigned char>> _pixelShaderBinaries;
    std::map<PipelineStateKey, ComPtr<ID3D12PipelineState>> _pipelineStates;

    ShaderManager() = default;
    ~ShaderManager() = default;

public:
    static ShaderManager& Instance() {
        static ShaderManager instance;
        return instance;
    }

    void LoadVertexShader(const std::string& id, const char* fileName);
    void LoadPixelShader(const std::string& id, const char* fileName);

    ID3D12PipelineState* GetPipelineState(
        const std::string& vsId,
        const std::string& psId,
        int blendMode, // 0: Opaque, 1: AlphaBlend, 2: Additive
        int cullMode,  // D3D12_CULL_MODE
        bool depthEnable,
        bool depthWrite,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
    );
};

#endif // !_SHADERMANAGER_H
