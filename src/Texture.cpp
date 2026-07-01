#include "Texture.h"
#include "Manager.h"
#include "Renderer.h"
#include <cassert>
#include <vector>

std::unordered_map<std::string, std::unique_ptr<TEXTURE>> Texture::_texturePool;

namespace RendererDX12 {
    unsigned int CreateShaderResourceView(ID3D12Resource* resource);
}

const TEXTURE* Texture::Load(const char* FileName) {

	if (_texturePool.count(FileName) > 0)
	{
		return _texturePool[FileName].get();
	}

	wchar_t wFileName[512];
	mbstowcs(wFileName, FileName, strlen(FileName) + 1);

	// テクスチャロード
	TexMetadata metadata;
	ScratchImage image;
	HRESULT hr = LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to load WIC texture file: ");
		OutputDebugStringA(FileName);
		OutputDebugStringA("\n");
		return nullptr;
	}

	// D3D12テクスチャリソースの作成
	ID3D12Device* device = Renderer::GetDeviceDX12();
	ComPtr<ID3D12Resource> res;
	hr = CreateTexture(device, metadata, &res);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create D3D12 texture resource: ");
		OutputDebugStringA(FileName);
		OutputDebugStringA("\n");
		return nullptr;
	}

	// アップロードバッファを作成してデータ転送を行う
	UINT64 uploadSize = 0;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(metadata.mipLevels);
	std::vector<UINT> numRows(metadata.mipLevels);
	std::vector<UINT64> rowSizes(metadata.mipLevels);
	
	D3D12_RESOURCE_DESC desc = res->GetDesc();
	device->GetCopyableFootprints(&desc, 0, (UINT)metadata.mipLevels, 0, layouts.data(), numRows.data(), rowSizes.data(), &uploadSize);

	ComPtr<ID3D12Resource> uploadBuffer;
	
	D3D12_HEAP_PROPERTIES uploadHeapProps = {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = uploadSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)
	);
	assert(SUCCEEDED(hr));

	// アップロードバッファにCPU側でデータをコピー
	void* mappedData = nullptr;
	hr = uploadBuffer->Map(0, nullptr, &mappedData);
	assert(SUCCEEDED(hr));

	for (size_t i = 0; i < metadata.mipLevels; i++) {
		const Image* img = image.GetImage(i, 0, 0);
		byte* dest = (byte*)mappedData + layouts[i].Offset;
		byte* src = img->pixels;
		for (UINT y = 0; y < numRows[i]; y++) {
			memcpy(dest + y * layouts[i].Footprint.RowPitch, src + y * img->rowPitch, rowSizes[i]);
		}
	}
	uploadBuffer->Unmap(0, nullptr);

	// 一時的なコマンドアロケータとコマンドリストを作成してGPU転送を実行
	ComPtr<ID3D12CommandAllocator> tempAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
	assert(SUCCEEDED(hr));

	ComPtr<ID3D12GraphicsCommandList> tempCmdList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempCmdList));
	assert(SUCCEEDED(hr));

	// コピー先にリソースバリアを張る (COMMON -> COPY_DEST)
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = res.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tempCmdList->ResourceBarrier(1, &barrier);

	// コピーコマンドの発行
	for (size_t i = 0; i < metadata.mipLevels; i++) {
		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource = res.Get();
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLoc.SubresourceIndex = (UINT)i;

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource = uploadBuffer.Get();
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLoc.PlacedFootprint = layouts[i];

		tempCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	}

	// リソースバリアを戻す (COPY_DEST -> PIXEL_SHADER_RESOURCE)
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	tempCmdList->ResourceBarrier(1, &barrier);

	hr = tempCmdList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();
	ID3D12CommandList* cmdLists[] = { tempCmdList.Get() };
	queue->ExecuteCommandLists(1, cmdLists);

	// 同期待機用のフェンス生成
	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	queue->Signal(fence.Get(), 1);

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);
	}
	CloseHandle(event);

	// SRV作成
	unsigned int srvIndex = RendererDX12::CreateShaderResourceView(res.Get());

	auto tex = std::make_unique<TEXTURE>();
	tex->Resource = res;
	tex->SRVIndex = srvIndex;

	_texturePool[FileName] = std::move(tex);

	return _texturePool[FileName].get();
}
