#include "D3DApp.h"
#include <DirectXColors.h>

using namespace DirectX;

class DirectX12_Init : public D3DApp
{
public:
	DirectX12_Init(HINSTANCE hInstance);
	~DirectX12_Init();

	virtual bool Initialize()override;

private:

	virtual void Draw() override;

};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		DirectX12_Init theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

DirectX12_Init::DirectX12_Init(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

DirectX12_Init::~DirectX12_Init()
{
}

bool DirectX12_Init::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	return true;
}

void DirectX12_Init::Draw()
{
    // 重置命令分配器
    ThrowIfFailed(mCommandAllocator->Reset());

    // 重置命令列表
    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    // 切换渲染目标状态为渲染目标
    mCommandList->ResourceBarrier(
        1,
        &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
    );

    // 设置视口和剪裁矩形
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 获取当前后台缓冲的RTV和DSV句柄
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize
    );
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

    // 清除渲染目标和深度模板缓冲
    mCommandList->ClearRenderTargetView(
        rtvHandle,
        Colors::Sienna,
        0,
        nullptr
    );
    mCommandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f,
        0,
        0,
        nullptr
    );

    // 将渲染目标和深度模板缓冲绑定到管线
    mCommandList->OMSetRenderTargets(
        1,
        &rtvHandle,
        true,
        &dsvHandle
    );

    // 切换渲染目标状态为呈现目标
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 关闭命令列表
    ThrowIfFailed(mCommandList->Close());

    // 执行命令列表
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 呈现当前帧
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // 刷新命令队列，确保命令执行完成
    FlushCommandQueue();
}