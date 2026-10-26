#include "Texture.h"
#include "Manager.h"
#include "Renderer.h"
#include <cassert>
#include <vector>

std::unordered_map<std::string, std::unique_ptr<TEXTURE>> Texture::_texturePool;

namespace RendererDX12 {
    unsigned int CreateShaderResourceView(ID3D12Resource* resource);
    void UpdateShaderResourceView(ID3D12Resource* resource, unsigned int index);
}

#include <filesystem>
#include <algorithm>
#include <deque>

static std::deque<ComPtr<ID3D12Resource>> s_retiredTextureResources;
static void RetireTextureResource(ComPtr<ID3D12Resource> res) {
	if (!res) return;
	s_retiredTextureResources.push_back(res);
	while (s_retiredTextureResources.size() > 64) {
		s_retiredTextureResources.pop_front();
	}
}
#include <algorithm>

const TEXTURE* Texture::Load(const char* FileName) {
	if (!FileName) return nullptr;

	size_t len = 0;
	while (len < 512 && FileName[len] != '\0') len++;
	if (len == 0 || len >= 512) return nullptr;

	std::string resolvedPath(FileName, len);
	std::replace(resolvedPath.begin(), resolvedPath.end(), '\\', '/');

	if (std::filesystem::is_directory(resolvedPath)) {
		return nullptr;
	}

	if (_texturePool.count(resolvedPath) > 0)
	{
		return _texturePool[resolvedPath].get();
	}

	if (!std::filesystem::is_regular_file(resolvedPath)) {
		std::filesystem::path p(resolvedPath);
		std::string filename = p.filename().string();

		if (filename.empty() || filename == "." || filename == "..") {
			return nullptr;
		}

		std::vector<std::string> searchDirs = { "asset/model", "asset/texture", "asset" };
		bool found = false;
		for (const auto& dir : searchDirs) {
			std::string candidate = dir + "/" + filename;
			if (std::filesystem::is_regular_file(candidate)) {
				resolvedPath = candidate;
				found = true;
				break;
			}
		}
		if (!found) {
			return nullptr;
		}
	}

	if (_texturePool.count(resolvedPath) > 0)
	{
		return _texturePool[resolvedPath].get();
	}

	wchar_t wFileName[512];
	mbstowcs(wFileName, resolvedPath.c_str(), resolvedPath.length() + 1);

	TexMetadata metadata;
	ScratchImage image;
	HRESULT hr = LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(hr)) {
		hr = LoadFromTGAFile(wFileName, &metadata, image);
	}
	if (FAILED(hr)) {
		hr = LoadFromDDSFile(wFileName, DDS_FLAGS_NONE, &metadata, image);
	}

	if (FAILED(hr)) {
		OutputDebugStringA("Failed to load texture file: ");
		OutputDebugStringA(resolvedPath.c_str());
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

	const TEXTURE* retPtr = tex.get();
	_texturePool[resolvedPath] = std::move(tex);
	return retPtr;
}

const TEXTURE* Texture::LoadFromMemory(const std::string& key, const void* data, size_t size) {
	if (key.empty() || !data || size == 0) return nullptr;

	if (_texturePool.count(key) > 0) {
		return _texturePool[key].get();
	}

	TexMetadata metadata;
	ScratchImage image;
	HRESULT hr = LoadFromWICMemory((const uint8_t*)data, size, WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(hr)) {
		hr = LoadFromTGAMemory((const uint8_t*)data, size, &metadata, image);
	}
	if (FAILED(hr)) {
		hr = LoadFromDDSMemory((const uint8_t*)data, size, DDS_FLAGS_NONE, &metadata, image);
	}

	if (FAILED(hr)) {
		OutputDebugStringA("Failed to load texture from memory: ");
		OutputDebugStringA(key.c_str());
		OutputDebugStringA("\n");
		return nullptr;
	}

	ID3D12Device* device = Renderer::GetDeviceDX12();
	ComPtr<ID3D12Resource> res;
	hr = CreateTexture(device, metadata, &res);
	if (FAILED(hr)) {
		OutputDebugStringA("Failed to create D3D12 texture resource from memory: ");
		OutputDebugStringA(key.c_str());
		OutputDebugStringA("\n");
		return nullptr;
	}

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

	ComPtr<ID3D12CommandAllocator> tempAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
	assert(SUCCEEDED(hr));

	ComPtr<ID3D12GraphicsCommandList> tempCmdList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempCmdList));
	assert(SUCCEEDED(hr));

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = res.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tempCmdList->ResourceBarrier(1, &barrier);

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

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	tempCmdList->ResourceBarrier(1, &barrier);

	hr = tempCmdList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();
	ID3D12CommandList* cmdLists[] = { tempCmdList.Get() };
	queue->ExecuteCommandLists(1, cmdLists);

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

	unsigned int srvIndex = RendererDX12::CreateShaderResourceView(res.Get());

	auto tex = std::make_unique<TEXTURE>();
	tex->Resource = res;
	tex->SRVIndex = srvIndex;

	const TEXTURE* retPtr = tex.get();
	_texturePool[key] = std::move(tex);
	return retPtr;
}


const TEXTURE* Texture::CreateFromRGBA(const std::string& key, const unsigned char* rgbaData, unsigned int width, unsigned int height) {
	if (key.empty() || !rgbaData || width == 0 || height == 0) return nullptr;

	if (_texturePool.count(key) > 0) {
		return _texturePool[key].get();
	}

	ScratchImage image;
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	if (FAILED(hr)) {
		return nullptr;
	}

	const Image* img = image.GetImage(0, 0, 0);
	if (!img || !img->pixels) {
		return nullptr;
	}

	for (unsigned int y = 0; y < height; y++) {
		memcpy(img->pixels + y * img->rowPitch, rgbaData + y * width * 4, width * 4);
	}

	const TexMetadata& metadata = image.GetMetadata();

	ID3D12Device* device = Renderer::GetDeviceDX12();
	if (!device) return nullptr;

	ComPtr<ID3D12Resource> res;
	hr = CreateTexture(device, metadata, &res);
	if (FAILED(hr)) {
		return nullptr;
	}

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
	if (FAILED(hr)) return nullptr;

	void* mappedData = nullptr;
	hr = uploadBuffer->Map(0, nullptr, &mappedData);
	if (FAILED(hr)) return nullptr;

	for (size_t i = 0; i < metadata.mipLevels; i++) {
		const Image* subImg = image.GetImage(i, 0, 0);
		byte* dest = (byte*)mappedData + layouts[i].Offset;
		byte* src = subImg->pixels;
		for (UINT y = 0; y < numRows[i]; y++) {
			memcpy(dest + y * layouts[i].Footprint.RowPitch, src + y * subImg->rowPitch, rowSizes[i]);
		}
	}
	uploadBuffer->Unmap(0, nullptr);

	ComPtr<ID3D12CommandAllocator> tempAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
	if (FAILED(hr)) return nullptr;

	ComPtr<ID3D12GraphicsCommandList> tempCmdList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempCmdList));
	if (FAILED(hr)) return nullptr;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = res.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tempCmdList->ResourceBarrier(1, &barrier);

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

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	tempCmdList->ResourceBarrier(1, &barrier);

	hr = tempCmdList->Close();
	if (FAILED(hr)) return nullptr;

	ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();
	ID3D12CommandList* cmdLists[] = { tempCmdList.Get() };
	queue->ExecuteCommandLists(1, cmdLists);

	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(hr)) return nullptr;
	queue->Signal(fence.Get(), 1);

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);
	}
	CloseHandle(event);

	unsigned int srvIndex = RendererDX12::CreateShaderResourceView(res.Get());

	auto tex = std::make_unique<TEXTURE>();
	tex->Resource = res;
	tex->SRVIndex = srvIndex;

	const TEXTURE* retPtr = tex.get();
	_texturePool[key] = std::move(tex);
	return retPtr;
}


const TEXTURE* Texture::UpdateOrCreateFromRGBA(const std::string& key, const unsigned char* rgbaData, unsigned int width, unsigned int height) {
	if (key.empty() || !rgbaData || width == 0 || height == 0) return nullptr;

	if (_texturePool.count(key) == 0) {
		return CreateFromRGBA(key, rgbaData, width, height);
	}

	ScratchImage image;
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	if (FAILED(hr)) return nullptr;

	const Image* img = image.GetImage(0, 0, 0);
	if (!img || !img->pixels) return nullptr;

	for (unsigned int y = 0; y < height; y++) {
		memcpy(img->pixels + y * img->rowPitch, rgbaData + y * width * 4, width * 4);
	}

	const TexMetadata& metadata = image.GetMetadata();
	ID3D12Device* device = Renderer::GetDeviceDX12();
	if (!device) return nullptr;

	ComPtr<ID3D12Resource> res;
	hr = CreateTexture(device, metadata, &res);
	if (FAILED(hr)) return nullptr;

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
	if (FAILED(hr)) return nullptr;

	void* mappedData = nullptr;
	hr = uploadBuffer->Map(0, nullptr, &mappedData);
	if (FAILED(hr)) return nullptr;

	for (size_t i = 0; i < metadata.mipLevels; i++) {
		const Image* subImg = image.GetImage(i, 0, 0);
		byte* dest = (byte*)mappedData + layouts[i].Offset;
		byte* src = subImg->pixels;
		for (UINT y = 0; y < numRows[i]; y++) {
			memcpy(dest + y * layouts[i].Footprint.RowPitch, src + y * subImg->rowPitch, rowSizes[i]);
		}
	}
	uploadBuffer->Unmap(0, nullptr);

	ComPtr<ID3D12CommandAllocator> tempAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
	if (FAILED(hr)) return nullptr;

	ComPtr<ID3D12GraphicsCommandList> tempCmdList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempCmdList));
	if (FAILED(hr)) return nullptr;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = res.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tempCmdList->ResourceBarrier(1, &barrier);

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

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	tempCmdList->ResourceBarrier(1, &barrier);

	hr = tempCmdList->Close();
	if (FAILED(hr)) return nullptr;

	ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();
	ID3D12CommandList* cmdLists[] = { tempCmdList.Get() };
	queue->ExecuteCommandLists(1, cmdLists);

	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(hr)) return nullptr;
	queue->Signal(fence.Get(), 1);

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);
	}
	CloseHandle(event);

	auto& existingTex = _texturePool[key];
	RetireTextureResource(existingTex->Resource);
	existingTex->Resource = res;
	RendererDX12::UpdateShaderResourceView(res.Get(), existingTex->SRVIndex);
	return existingTex.get();
}

const TEXTURE* Texture::Reload(const char* FileName) {
	if (!FileName) return nullptr;

	size_t len = 0;
	while (len < 512 && FileName[len] != '\0') len++;
	if (len == 0 || len >= 512) return nullptr;

	std::string resolvedPath(FileName, len);
	std::replace(resolvedPath.begin(), resolvedPath.end(), '\\', '/');

	if (_texturePool.count(resolvedPath) == 0) {
		return Load(resolvedPath.c_str());
	}

	wchar_t wFileName[512];
	mbstowcs(wFileName, resolvedPath.c_str(), resolvedPath.length() + 1);

	TexMetadata metadata;
	ScratchImage image;
	HRESULT hr = LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(hr)) {
		hr = LoadFromTGAFile(wFileName, &metadata, image);
	}
	if (FAILED(hr)) {
		hr = LoadFromDDSFile(wFileName, DDS_FLAGS_NONE, &metadata, image);
	}
	if (FAILED(hr)) {
		return _texturePool[resolvedPath].get();
	}

	ID3D12Device* device = Renderer::GetDeviceDX12();
	if (!device) return nullptr;

	ComPtr<ID3D12Resource> res;
	hr = CreateTexture(device, metadata, &res);
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

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
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

	void* mappedData = nullptr;
	hr = uploadBuffer->Map(0, nullptr, &mappedData);
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

	for (size_t i = 0; i < metadata.mipLevels; i++) {
		const Image* img = image.GetImage(i, 0, 0);
		byte* dest = (byte*)mappedData + layouts[i].Offset;
		byte* src = img->pixels;
		for (UINT y = 0; y < numRows[i]; y++) {
			memcpy(dest + y * layouts[i].Footprint.RowPitch, src + y * img->rowPitch, rowSizes[i]);
		}
	}
	uploadBuffer->Unmap(0, nullptr);

	ComPtr<ID3D12CommandAllocator> tempAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc));
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

	ComPtr<ID3D12GraphicsCommandList> tempCmdList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempCmdList));
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = res.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tempCmdList->ResourceBarrier(1, &barrier);

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

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	tempCmdList->ResourceBarrier(1, &barrier);

	hr = tempCmdList->Close();
	if (FAILED(hr)) return _texturePool[resolvedPath].get();

	ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();
	ID3D12CommandList* cmdLists[] = { tempCmdList.Get() };
	queue->ExecuteCommandLists(1, cmdLists);

	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(hr)) return _texturePool[resolvedPath].get();
	queue->Signal(fence.Get(), 1);

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);
	}
	CloseHandle(event);

	auto& existingTex = _texturePool[resolvedPath];
	RetireTextureResource(existingTex->Resource);
	existingTex->Resource = res;
	RendererDX12::UpdateShaderResourceView(res.Get(), existingTex->SRVIndex);
	return existingTex.get();
}
