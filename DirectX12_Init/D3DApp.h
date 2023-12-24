#pragma once

#include "d3dUtil.h"
#include "../Common/GameTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& temp) = delete;
	D3DApp& operator=(const D3DApp& temp) = delete;
	virtual ~D3DApp();

public:

	static D3DApp* GetApp();

	void Set4xMsaa();

	int Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

	virtual void Draw() = 0;

protected:
	bool InitMainWindow();
	bool InitDirect3D();

	void CreateDevice();
	void CreateFence();
	void CreateCommandObject();
	void CreateSwapChain();
	void CreateDescriptorHeap();
	void CreateRTV();
	void CreateDSV();
	void SetViewportAndScissor();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;

protected:

	static D3DApp* mApp;

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND mhMainWnd = nullptr; // main window handle
	bool mAppPaused = false;  // is the application paused?
	bool mMinimized = false;  // is the application minimized?
	bool mMaximized = false;  // is the application maximized?
	bool mResizing = false;   // are the resize bars being dragged?
	bool mFullscreenState = false;// fullscreen enabled

	UINT m4xMsaaQuality = 0;

	Microsoft::WRL::ComPtr <IDXGIFactory> mFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> mDsvHeap;

	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	std::wstring mMainWndCaption = L"d3d App";

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int mClientWidth = 800;
	int mClientHeight = 600;
};