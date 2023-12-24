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
    //��Ϣѭ��
    //������Ϣ�ṹ��
    MSG msg = { 0 };
    //���GetMessage����������0��˵��û�н��ܵ�WM_QUIT
    while (msg.message != WM_QUIT)
    {
        //����д�����Ϣ�ͽ��д���
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) //PeekMessage�������Զ����msg�ṹ��Ԫ��
        {
            TranslateMessage(&msg);	//���̰���ת�������������Ϣת��Ϊ�ַ���Ϣ
            DispatchMessage(&msg);	//����Ϣ���ɸ���Ӧ�Ĵ��ڹ���
        }
        //�����ִ�ж�������Ϸ�߼�
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
    //����D3D���Բ�
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
    // ���� DXGI ����
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

    // ���� Direct3D 12 �豸
    HRESULT h = D3D12CreateDevice(
        nullptr,                    //��ʾ�豸���ã�nullptr��ʾ����Ϊ���豸
        D3D_FEATURE_LEVEL_11_0,     //DirectX���֧�ּ�������ΪD3D11
        IID_PPV_ARGS(&mDevice)
    );

    // ��ȡ�������ѵĴ�С
    mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::CreateFence()
{
    // ���� Fence ����
    ThrowIfFailed(
        mDevice->CreateFence(
            0,                              // ��ʼ�� Fence ֵ
            D3D12_FENCE_FLAG_NONE,          // Fence ��־������ѡ���������־
            IID_PPV_ARGS(&mFence)           // ���ڽ��մ����� Fence ����� ComPtr ָ��
        )
    );
}

void D3DApp::Set4xMsaa()
{
    // �ṹ�����ڴ洢���ز�������������Ϣ
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
    msaaQualityLevels.Format = mBackBufferFormat;
    msaaQualityLevels.SampleCount = 4; // ����Ϊ4�����ز���
    msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msaaQualityLevels.NumQualityLevels = 0;

    // ���Ӳ���Ƿ�֧��ָ���Ķ��ز�������
    ThrowIfFailed(
        mDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msaaQualityLevels,
            sizeof(msaaQualityLevels))
    );

    // ȷ��������Ч�Ķ��ز�����������
    assert(msaaQualityLevels.NumQualityLevels > 0);

    // �����ز�����������洢�Ա�����
    m4xMsaaQuality = msaaQualityLevels.NumQualityLevels;
}

void D3DApp::CreateCommandObject()
{
    // �����������������
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // ָ��Ϊֱ�������б�
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // �������־

    // �����������
    ThrowIfFailed(
        mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))
    );

    // ���������������Ϊ�˷��������б��ڴ棩
    ThrowIfFailed(
        mDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, // ���������������ͬ
            IID_PPV_ARGS(&mCommandAllocator)
        )
    );

    // ���������б�
    ThrowIfFailed(
        mDevice->CreateCommandList(
            0,                              // ͨ��Ϊ0����ʾ���߳��ύ�����б�
            D3D12_COMMAND_LIST_TYPE_DIRECT, // ���������������ͬ
            mCommandAllocator.Get(),        // �������������
            nullptr,                        // ��ʼ��ˮ��״̬,��ʱ����Ϊ��
            IID_PPV_ARGS(&mCommandList)
        )
    );

    // �ر������б�׼���ý����µ�����
    mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    // �����Ѵ��ڵĽ�����
    mSwapChain.Reset();

    // DXGI_SWAP_CHAIN_DESC �ṹ��������������������
    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;                                     // �������������Ŀ��
    sd.BufferDesc.Height = mClientHeight;                                   // �������������ĸ߶�
    sd.BufferDesc.RefreshRate.Numerator = 60;                               // ˢ���ʵķ���
    sd.BufferDesc.RefreshRate.Denominator = 1;                              // ˢ���ʵķ�ĸ
    sd.BufferDesc.Format = mBackBufferFormat;                               // �������ĸ�ʽ
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;  // ɨ���ߵ�����ʽ
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;                  // �����������ŷ�ʽ
    sd.SampleDesc.Count = 1;                                                // ���ز���������
    sd.SampleDesc.Quality = 0;                                              // ���ز���������
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                       // ��������ʹ�÷�ʽ
    sd.BufferCount = SwapChainBufferCount;                                   // ������������
    sd.OutputWindow = mhMainWnd;                                            // ��Ⱦ���ڵľ��
    sd.Windowed = true;                                                     // �Ƿ񴰿ڻ�
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                          // ��������Ч��
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;                      // ����ģʽ�л�

    // ����������
    ThrowIfFailed(
        mFactory->CreateSwapChain(
            mCommandQueue.Get(), // �������
            &sd,                 // ����������
            &mSwapChain          // ����Ľ���������
        )
    );
}

void D3DApp::CreateDescriptorHeap()
{
    // ������ȾĿ����ͼ (RTV) ����������
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeap;
    rtvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;             // �������ѵı�־���˴��������־
    rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;              // �������ѵ�����Ϊ��ȾĿ����ͼ (RTV) ����
    rtvHeap.NodeMask = 0;                                       // �ڵ����֣��˴���ʾ�ڵ��ڵ�ϵͳ��ʹ��
    rtvHeap.NumDescriptors = SwapChainBufferCount;              // ���������е��������������뽻������̨����������һ��
    ThrowIfFailed(
        mDevice->CreateDescriptorHeap(
            &rtvHeap,
            IID_PPV_ARGS(&mRtvHeap)
        )
    );

    // �������ģ����ͼ (DSV) ����������
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeap;
    dsvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;             // �������ѵı�־���˴��������־
    dsvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;              // �������ѵ�����Ϊ���ģ����ͼ (DSV) ����
    dsvHeap.NodeMask = 0;                                       // �ڵ����֣��˴���ʾ�ڵ��ڵ�ϵͳ��ʹ��
    dsvHeap.NumDescriptors = 1;                                // ���������е��������������˴�Ϊ���ģ����ͼ (DSV) ������
    ThrowIfFailed(
        mDevice->CreateDescriptorHeap(
            &dsvHeap,
            IID_PPV_ARGS(&mDsvHeap)
        )
    );
}

void D3DApp::CreateRTV()
{
    // ��ȡ��ȾĿ����ͼ�ѵ�CPU���
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // �����������еĺ�̨���壬Ϊÿ����̨���崴����ȾĿ����ͼ
    for (int i = 0; i < SwapChainBufferCount; i++)
    {
        // ��ȡ�������еĺ�̨����
        ThrowIfFailed(
            mSwapChain->GetBuffer(
                i,
                IID_PPV_ARGS(&mSwapChainBuffer[i])
            )
        );

        // ʹ���豸�ӿڴ�����ȾĿ����ͼ�������������ȾĿ����ͼ�ѵĵ�ǰ���λ��
        mDevice->CreateRenderTargetView(
            mSwapChainBuffer[i].Get(),  // ��̨������Դ
            nullptr,                    // ��ȾĿ����ͼ��������
            rtvHeapHandle               // ��ǰ��ȾĿ����ͼ�Ѿ��λ��
        );

        // ƫ������һ����ȾĿ����ͼ�Ѿ��λ��
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

    // �������ģ�建������Դ������
    D3D12_RESOURCE_DESC rd;
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // ָ��ά��Ϊ��ά����
    rd.Width = mClientWidth;                            // ָ�����
    rd.Height = mClientHeight;                          // ָ���߶�
    rd.DepthOrArraySize = 1;                            // ��Ⱥ��������������
    rd.MipLevels = 1;                                   // Mipmap �㼶��
    rd.Alignment = 0;                                   // ���뷽ʽ
    rd.Format = mDepthStencilFormat;                    // ���ģ���ʽ
    rd.SampleDesc.Count = 1;                            // ������
    rd.SampleDesc.Quality = 0;                          // ��������
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;           // ������
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // ��־����ʾ������Ⱥ�ģ��

    // �������ģ�建�����ĳ�ʼ���ֵ
    D3D12_CLEAR_VALUE cv;
    cv.Format = mDepthStencilFormat;    // ���ģ���ʽ
    cv.DepthStencil.Depth = 1.0f;       // ��ʼ���ֵ
    cv.DepthStencil.Stencil = 0;        // ��ʼģ��ֵ

    // �������ģ�建����
    ThrowIfFailed(
        mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),  // ������
            D3D12_HEAP_FLAG_NONE,                               // �ѱ�־
            &rd,                                                // ��Դ������
            D3D12_RESOURCE_STATE_COMMON,                        // ��ʼ��Դ״̬
            &cv,                                                // ���ֵ
            IID_PPV_ARGS(&mDepthStencilBuffer)                  // ������ģ�建����
        )
    );

    // �������ģ����ͼ������
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvd;
    dsvd.Flags = D3D12_DSV_FLAG_NONE;                       // ��־���˴���ʾ�������־
    dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;     // ��ͼά��Ϊ��ά����
    dsvd.Format = mDepthStencilFormat;                      // ���ģ���ʽ
    dsvd.Texture2D.MipSlice = 0;                            // Mipmap ��Ƭ

    // �������ģ����ͼ
    mDevice->CreateDepthStencilView(
        mDepthStencilBuffer.Get(),                          // ��Ӧ���ģ�建����
        &dsvd,                                              // ���ģ����ͼ������
        DepthStencilView()                                  // ������ģ����ͼ
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
    // ������Ļ�ӿ�
    mScreenViewport.TopLeftX = 0;                               // �ӿ����Ͻ� X ����
    mScreenViewport.TopLeftY = 0;                               // �ӿ����Ͻ� Y ����
    mScreenViewport.Width = static_cast<float>(mClientWidth);   // �ӿڿ��
    mScreenViewport.Height = static_cast<float>(mClientHeight); // �ӿڸ߶�
    mScreenViewport.MinDepth = 0.0f;                            // �ӿ���С���
    mScreenViewport.MaxDepth = 1.0f;                            // �ӿ�������

    // ���ü��þ���
    mScissorRect.top = 0;                       // ���þ��ζ�������
    mScissorRect.left = 0;                      // ���þ����������
    mScissorRect.right = mClientWidth / 2.0;    // ���þ����Ҳ����꣨��ȵ�һ�룩
    mScissorRect.bottom = mClientHeight / 2.0;  // ���þ��εײ����꣨�߶ȵ�һ�룩
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