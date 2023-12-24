#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Forward hwnd on because we can get messages (e.g., WM_CREATE)
    // before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
    return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
    : mhAppInst(hInstance)
{
    // Only one D3DApp can be constructed.
    assert(mApp == nullptr);
    mApp = this;
}

D3DApp::~D3DApp()
{
    if (mDevice != nullptr)
        FlushCommandQueue();
}

int D3DApp::Run()
{
    //消息循环
    //定义消息结构体
    MSG msg = { 0 };
    //如果GetMessage函数不等于0，说明没有接受到WM_QUIT
    while (msg.message != WM_QUIT)
    {
        //如果有窗口消息就进行处理
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) //PeekMessage函数会自动填充msg结构体元素
        {
            TranslateMessage(&msg);	//键盘按键转换，将虚拟键消息转换为字符消息
            DispatchMessage(&msg);	//把消息分派给相应的窗口过程
        }
        //否则就执行动画和游戏逻辑
        else
        {
            Draw();
        }
    }
    return (int)msg.wParam;
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::Initialize()
{
    if (!InitMainWindow())
        return false;

    if (!InitDirect3D())
        return false;

    // Do the initial resize code.
    //OnResize();

    return true;
}

bool D3DApp::InitMainWindow()
{
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = mhAppInst;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"MainWnd";

    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
    if (!mhMainWnd)
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }

    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);

    return true;
}

bool D3DApp::InitDirect3D()
{
    //开启D3D调试层
#if defined(DEBUG) || defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
    }
#endif

    CreateDevice();
    CreateFence();
    Set4xMsaa();
    CreateCommandObject();

    CreateSwapChain();
    CreateDescriptorHeap();
    CreateRTV();
    CreateDSV();
    SetViewportAndScissor();

    return true;
}

void D3DApp::CreateDevice()
{
    // 创建 DXGI 工厂
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

    // 创建 Direct3D 12 设备
    HRESULT h = D3D12CreateDevice(
        nullptr,                    //显示设备设置，nullptr表示设置为主设备
        D3D_FEATURE_LEVEL_11_0,     //DirectX最低支持级别，设置为D3D11
        IID_PPV_ARGS(&mDevice)
    );

    // 获取描述符堆的大小
    mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::CreateFence()
{
    // 创建 Fence 对象
    ThrowIfFailed(
        mDevice->CreateFence(
            0,                              // 初始的 Fence 值
            D3D12_FENCE_FLAG_NONE,          // Fence 标志，这里选择无特殊标志
            IID_PPV_ARGS(&mFence)           // 用于接收创建的 Fence 对象的 ComPtr 指针
        )
    );
}

void D3DApp::Set4xMsaa()
{
    // 结构体用于存储多重采样质量级别信息
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
    msaaQualityLevels.Format = mBackBufferFormat;
    msaaQualityLevels.SampleCount = 4; // 设置为4倍多重采样
    msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msaaQualityLevels.NumQualityLevels = 0;

    // 检查硬件是否支持指定的多重采样配置
    ThrowIfFailed(
        mDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msaaQualityLevels,
            sizeof(msaaQualityLevels))
    );

    // 确保存在有效的多重采样质量级别
    assert(msaaQualityLevels.NumQualityLevels > 0);

    // 将多重采样质量级别存储以备后用
    m4xMsaaQuality = msaaQualityLevels.NumQualityLevels;
}

void D3DApp::CreateCommandObject()
{
    // 创建命令队列描述符
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 指定为直接命令列表
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // 无特殊标志

    // 创建命令队列
    ThrowIfFailed(
        mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))
    );

    // 创建命令分配器（为了分配命令列表内存）
    ThrowIfFailed(
        mDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, // 与命令队列类型相同
            IID_PPV_ARGS(&mCommandAllocator)
        )
    );

    // 创建命令列表
    ThrowIfFailed(
        mDevice->CreateCommandList(
            0,                              // 通常为0，表示单线程提交命令列表
            D3D12_COMMAND_LIST_TYPE_DIRECT, // 与命令队列类型相同
            mCommandAllocator.Get(),        // 关联命令分配器
            nullptr,                        // 初始流水线状态,暂时设置为空
            IID_PPV_ARGS(&mCommandList)
        )
    );

    // 关闭命令列表，准备好接收新的命令
    mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    // 重置已存在的交换链
    mSwapChain.Reset();

    // DXGI_SWAP_CHAIN_DESC 结构体用于描述交换链属性
    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;                                     // 交换链缓冲区的宽度
    sd.BufferDesc.Height = mClientHeight;                                   // 交换链缓冲区的高度
    sd.BufferDesc.RefreshRate.Numerator = 60;                               // 刷新率的分子
    sd.BufferDesc.RefreshRate.Denominator = 1;                              // 刷新率的分母
    sd.BufferDesc.Format = mBackBufferFormat;                               // 缓冲区的格式
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;  // 扫描线的排序方式
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;                  // 缓冲区的缩放方式
    sd.SampleDesc.Count = 1;                                                // 多重采样的数量
    sd.SampleDesc.Quality = 0;                                              // 多重采样的质量
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                       // 缓冲区的使用方式
    sd.BufferCount = SwapChainBufferCount;                                   // 缓冲区的数量
    sd.OutputWindow = mhMainWnd;                                            // 渲染窗口的句柄
    sd.Windowed = true;                                                     // 是否窗口化
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                          // 交换链的效果
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;                      // 允许模式切换

    // 创建交换链
    ThrowIfFailed(
        mFactory->CreateSwapChain(
            mCommandQueue.Get(), // 命令队列
            &sd,                 // 交换链属性
            &mSwapChain          // 输出的交换链对象
        )
    );
}

void D3DApp::CreateDescriptorHeap()
{
    // 创建渲染目标视图 (RTV) 的描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeap;
    rtvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;             // 描述符堆的标志，此处无特殊标志
    rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;              // 描述符堆的类型为渲染目标视图 (RTV) 类型
    rtvHeap.NodeMask = 0;                                       // 节点遮罩，此处表示在单节点系统上使用
    rtvHeap.NumDescriptors = SwapChainBufferCount;              // 描述符堆中的描述符数量，与交换链后台缓冲区数量一致
    ThrowIfFailed(
        mDevice->CreateDescriptorHeap(
            &rtvHeap,
            IID_PPV_ARGS(&mRtvHeap)
        )
    );

    // 创建深度模板视图 (DSV) 的描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeap;
    dsvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;             // 描述符堆的标志，此处无特殊标志
    dsvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;              // 描述符堆的类型为深度模板视图 (DSV) 类型
    dsvHeap.NodeMask = 0;                                       // 节点遮罩，此处表示在单节点系统上使用
    dsvHeap.NumDescriptors = 1;                                // 描述符堆中的描述符数量，此处为深度模板视图 (DSV) 的数量
    ThrowIfFailed(
        mDevice->CreateDescriptorHeap(
            &dsvHeap,
            IID_PPV_ARGS(&mDsvHeap)
        )
    );
}

void D3DApp::CreateRTV()
{
    // 获取渲染目标视图堆的CPU句柄
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // 遍历交换链中的后台缓冲，为每个后台缓冲创建渲染目标视图
    for (int i = 0; i < SwapChainBufferCount; i++)
    {
        // 获取交换链中的后台缓冲
        ThrowIfFailed(
            mSwapChain->GetBuffer(
                i,
                IID_PPV_ARGS(&mSwapChainBuffer[i])
            )
        );

        // 使用设备接口创建渲染目标视图，将其关联到渲染目标视图堆的当前句柄位置
        mDevice->CreateRenderTargetView(
            mSwapChainBuffer[i].Get(),  // 后台缓冲资源
            nullptr,                    // 渲染目标视图的描述符
            rtvHeapHandle               // 当前渲染目标视图堆句柄位置
        );

        // 偏移至下一个渲染目标视图堆句柄位置
        rtvHeapHandle.Offset(mRtvDescriptorSize);
    }
}

void D3DApp::CreateDSV()
{
    assert(mDevice);
    assert(mSwapChain);
    assert(mCommandAllocator);
    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    // 定义深度模板缓冲区资源描述符
    D3D12_RESOURCE_DESC rd;
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 指定维度为二维纹理
    rd.Width = mClientWidth;                            // 指定宽度
    rd.Height = mClientHeight;                          // 指定高度
    rd.DepthOrArraySize = 1;                            // 深度和纹理数组的数量
    rd.MipLevels = 1;                                   // Mipmap 层级数
    rd.Alignment = 0;                                   // 对齐方式
    rd.Format = mDepthStencilFormat;                    // 深度模板格式
    rd.SampleDesc.Count = 1;                            // 采样数
    rd.SampleDesc.Quality = 0;                          // 质量级别
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;           // 纹理布局
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 标志，表示用于深度和模板

    // 定义深度模板缓冲区的初始清除值
    D3D12_CLEAR_VALUE cv;
    cv.Format = mDepthStencilFormat;    // 深度模板格式
    cv.DepthStencil.Depth = 1.0f;       // 初始深度值
    cv.DepthStencil.Stencil = 0;        // 初始模板值

    // 创建深度模板缓冲区
    ThrowIfFailed(
        mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),  // 堆属性
            D3D12_HEAP_FLAG_NONE,                               // 堆标志
            &rd,                                                // 资源描述符
            D3D12_RESOURCE_STATE_COMMON,                        // 初始资源状态
            &cv,                                                // 清除值
            IID_PPV_ARGS(&mDepthStencilBuffer)                  // 输出深度模板缓冲区
        )
    );

    // 定义深度模板视图描述符
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvd;
    dsvd.Flags = D3D12_DSV_FLAG_NONE;                       // 标志，此处表示无特殊标志
    dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;     // 视图维度为二维纹理
    dsvd.Format = mDepthStencilFormat;                      // 深度模板格式
    dsvd.Texture2D.MipSlice = 0;                            // Mipmap 切片

    // 创建深度模板视图
    mDevice->CreateDepthStencilView(
        mDepthStencilBuffer.Get(),                          // 对应深度模板缓冲区
        &dsvd,                                              // 深度模板视图描述符
        DepthStencilView()                                  // 输出深度模板视图
    );

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Execute the resize commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize is complete.
    FlushCommandQueue();
}

void D3DApp::SetViewportAndScissor()
{
    // 设置屏幕视口
    mScreenViewport.TopLeftX = 0;                               // 视口左上角 X 坐标
    mScreenViewport.TopLeftY = 0;                               // 视口左上角 Y 坐标
    mScreenViewport.Width = static_cast<float>(mClientWidth);   // 视口宽度
    mScreenViewport.Height = static_cast<float>(mClientHeight); // 视口高度
    mScreenViewport.MinDepth = 0.0f;                            // 视口最小深度
    mScreenViewport.MaxDepth = 1.0f;                            // 视口最大深度

    // 设置剪裁矩形
    mScissorRect.top = 0;                       // 剪裁矩形顶部坐标
    mScissorRect.left = 0;                      // 剪裁矩形左侧坐标
    mScissorRect.right = mClientWidth / 2.0;    // 剪裁矩形右侧坐标（宽度的一半）
    mScissorRect.bottom = mClientHeight / 2.0;  // 剪裁矩形底部坐标（高度的一半）
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize);
}

ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
    return mSwapChainBuffer[mCurrBackBuffer].Get();
}

void D3DApp::FlushCommandQueue()
{
    mCurrentFence++;

    mCommandQueue->Signal(mFence.Get(), mCurrentFence);	

    if (mFence->GetCompletedValue() < mCurrentFence)	
    {
        HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");

        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);

        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}