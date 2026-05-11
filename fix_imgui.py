import codecs
import re

# 1. Update RendererDX12.cpp
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\RendererDX12.cpp', 'r', 'cp932') as f:
    r12 = f.read()

# Remove ImGui_ImplDX12_Init from RendererDX12::Init
r12 = re.sub(r'// Init ImGui DX12\s*ImGui_ImplDX12_Init\([^)]+\);', '', r12)
# Remove ImGui_ImplDX12_Shutdown from Shutdown
r12 = r12.replace('ImGui_ImplDX12_Shutdown();', '')
# Remove ImGui_ImplDX12_NewFrame from BeginFrame
r12 = r12.replace('ImGui_ImplDX12_NewFrame();', '')

# Add getters
getters = '''
    ID3D12Device* GetDevice() { return g_pd3dDevice.Get(); }
    ID3D12DescriptorHeap* GetSrvHeap() { return g_pd3dSrvDescHeap.Get(); }
    int GetFrameCount() { return FrameCount; }
    ID3D12GraphicsCommandList* GetCommandList() { return g_pd3dCommandList.Get(); }
'''
if 'ID3D12Device* GetDevice()' not in r12:
    r12 = r12.replace('void BeginFrame()', getters + '\n    void BeginFrame()')

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\RendererDX12.cpp', 'w', 'cp932') as f:
    f.write(r12)

# 2. Update Renderer.cpp
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.cpp', 'r', 'cp932') as f:
    r_cpp = f.read()

if 'GetDeviceDX12' not in r_cpp:
    r_cpp += '''
namespace Renderer {
    ID3D12Device* GetDeviceDX12() { return RendererDX12::GetDevice(); }
    ID3D12DescriptorHeap* GetSrvHeapDX12() { return RendererDX12::GetSrvHeap(); }
    int GetFrameCountDX12() { return RendererDX12::GetFrameCount(); }
    ID3D12GraphicsCommandList* GetCommandListDX12() { return RendererDX12::GetCommandList(); }
}
'''
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.cpp', 'w', 'cp932') as f:
        f.write(r_cpp)

# 3. Update Renderer.h
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'r', 'cp932') as f:
    r_h = f.read()

r_h_add = '''
struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;

namespace Renderer {
    ID3D12Device* GetDeviceDX12();
    ID3D12DescriptorHeap* GetSrvHeapDX12();
    int GetFrameCountDX12();
    ID3D12GraphicsCommandList* GetCommandListDX12();
}
'''
if 'GetDeviceDX12' not in r_h:
    r_h = r_h.replace('#endif', r_h_add + '\n#endif')
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'w', 'cp932') as f:
        f.write(r_h)

# 4. Update ImGuiLayer.cpp
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\ImGuiLayer.cpp', 'r', 'cp932') as f:
    imgui_cpp = f.read()

dx12_init = '''
    } else if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX12) {
        ID3D12DescriptorHeap* srvHeap = Renderer::GetSrvHeapDX12();
        ImGui_ImplDX12_Init(Renderer::GetDeviceDX12(), Renderer::GetFrameCountDX12(),
            DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap,
            srvHeap->GetCPUDescriptorHandleForHeapStart(),
            srvHeap->GetGPUDescriptorHandleForHeapStart());
    }
'''
imgui_cpp = imgui_cpp.replace('    // DX12 Init is handled inside RendererDX12::Init for simplicity as it requires specific descriptors', dx12_init)

imgui_cpp = imgui_cpp.replace('    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {\n        ImGui_ImplDX11_NewFrame();\n    }', 
'''    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_NewFrame();
    } else if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX12) {
        ImGui_ImplDX12_NewFrame();
    }''')

imgui_cpp = imgui_cpp.replace('    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {\n        ImGui_ImplDX11_Shutdown();\n    }',
'''    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_Shutdown();
    } else if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX12) {
        ImGui_ImplDX12_Shutdown();
    }''')

if '#include <d3d12.h>' not in imgui_cpp:
    imgui_cpp = imgui_cpp.replace('#include "Renderer.h"', '#include "Renderer.h"\n#include <d3d12.h>')

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\ImGuiLayer.cpp', 'w', 'cp932') as f:
    f.write(imgui_cpp)

print('Updated ImGuiLayer and Renderer for DX12.')
