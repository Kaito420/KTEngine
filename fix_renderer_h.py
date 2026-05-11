import codecs

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'r', 'utf-8-sig') as f:
    r_h = f.read()

r_h = r_h.replace('''#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;

namespace Renderer {
    ID3D12Device* GetDeviceDX12();
    ID3D12DescriptorHeap* GetSrvHeapDX12();
    int GetFrameCountDX12();
    ID3D12GraphicsCommandList* GetCommandListDX12();
    ID3D12CommandQueue* GetCommandQueueDX12();
}

#endif''', '''#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;

namespace Renderer {
    ID3D12Device* GetDeviceDX12();
    ID3D12DescriptorHeap* GetSrvHeapDX12();
    int GetFrameCountDX12();
    ID3D12GraphicsCommandList* GetCommandListDX12();
    ID3D12CommandQueue* GetCommandQueueDX12();
}''')

duplicated = '''struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;

namespace Renderer {
    ID3D12Device* GetDeviceDX12();
    ID3D12DescriptorHeap* GetSrvHeapDX12();
    int GetFrameCountDX12();
    ID3D12GraphicsCommandList* GetCommandListDX12();
    ID3D12CommandQueue* GetCommandQueueDX12();
}
'''
r_h = r_h.replace(duplicated, '')

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'w', 'utf-8-sig') as f:
    f.write(r_h)

print('Fixed Renderer.h')
