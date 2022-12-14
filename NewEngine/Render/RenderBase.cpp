#include "RenderBase.h"
#include "RenderWindow.h"
#include "Viewport.h"
#include "ScissorRectangle.h"
#include <cassert>
#include <string>
#include <d3dcompiler.h>
#include <d3dx12.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
using namespace Microsoft::WRL;
using namespace std;

unique_ptr<ScissorRectangle> scissorRectangle = move(make_unique<ScissorRectangle>());
unique_ptr<Viewport> viewport = move(make_unique<Viewport>());

float RenderBase::clearColor[4] = { 0.1f,0.25f,0.5f,1.0f };

void RenderBase::Init()
{
	renderWindow = RenderWindow::GetInstance().get();
	viewport = move(make_unique<Viewport>());

	DeviceInit();			// デバイスの初期化
	CommandInit();			// コマンド関連の初期化
	SwapChainInit();		// スワップチェンの初期化
	FenceInit();			// フェンスの初期化
	DepthBufferInit();		// 深度バッファの初期化
	SrvInit();				// シェーダーリソースビュー関連の初期化
	ShaderCompilerInit();	// シェーダーコンパイラーの初期化
	RootSignatureInit();	// ルードシグネチャーの初期化
	GraphicsPipelineInit();	// グラフィックスパイプラインの初期化
}

void RenderBase::PreDraw()
{
	//---------------------- リソースバリアの変更コマンド ----------------------//
	// バックバッファの番号を取得（2つなので0番か1番）
	UINT bbIndex = swapChain->GetCurrentBackBufferIndex();
	// １．リソースバリアで書き込み可能に変更
	barrierDesc.Transition.pResource = backBuffers[bbIndex].Get();	// バックバッファを指定
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;	// 表示状態から
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態へ
	commandList->ResourceBarrier(1, &barrierDesc);

	//--------------------------- 描画先指定コマンド ---------------------------//
	// ２．描画先の変更
	// レンダーターゲットビューのハンドルを取得
	rtvHandle = rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += bbIndex * device->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);

	// 深度ステンシルビュー用デスクリプタヒープのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

	// 画面クリア R G B A
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// 深度バッファクリア
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// ビューポートの処理
	viewport->SetViewport(
		{ 0,0 },
		{
			(float)renderWindow->GetWindowSize().x,
			(float)renderWindow->GetWindowSize().y
		});
	viewport->Update();

	// シザー矩形の処理
	scissorRectangle->Update();
}
void RenderBase::PostDraw()
{
	HRESULT result;

	//---------------------- リソースバリアの復帰コマンド ----------------------//
	// ５．リソースバリアを戻す
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態から
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // 表示状態へ
	commandList->ResourceBarrier(1, &barrierDesc);

	//-------------------------- コマンドのフラッシュ --------------------------//
	// 命令のクローズ
	result = commandList->Close();
	assert(SUCCEEDED(result));
	// コマンドリストの実行
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// 画面に表示するバッファをフリップ（裏表の入替え）
	result = swapChain->Present(1, 0);
	assert(SUCCEEDED(result));

	// コマンドの実行完了を待つ
	commandQueue->Signal(fence.Get(), ++fenceVal);
	//RenderBase::GetInstance()->PreIncreFenceVal());
	if (fence.Get()->GetCompletedValue() != fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		fence->SetEventOnCompletion(fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	// キューをクリア
	result = commandAllocator->Reset();
	assert(SUCCEEDED(result));
	// 再びコマンドリストを貯める準備
	result = commandList.Get()->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(result));
}
void RenderBase::CreateSrv(Texture& texture, const D3D12_RESOURCE_DESC& textureResourceDesc)
{
	// SRVヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = srvDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvDescHeap->GetGPUDescriptorHandleForHeapStart();

	UINT descriptorSize = RenderBase::GetInstance()->GetDevice()->
		GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	srvCpuHandle.ptr += descriptorSize * incrementIndex;
	srvGpuHandle.ptr += descriptorSize * incrementIndex;

	texture.SetCpuHandle(srvCpuHandle);
	texture.SetGpuHandle(srvGpuHandle);

	// シェーダーリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};	// srv設定構造体
	srvDesc.Format = textureResourceDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = textureResourceDesc.MipLevels;

	// ハンドルの指す位置にシェーダーリソースビュー作成
	RenderBase::GetInstance()->GetDevice()->
		CreateShaderResourceView(texture.buffer.Get(), &srvDesc, srvCpuHandle);

	incrementIndex++;
}

// 各初期化
void RenderBase::DeviceInit()
{
	HRESULT result;

	// DXGIファクトリーの生成
	result = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(result));

	// アダプターの列挙用
	std::vector<ComPtr<IDXGIAdapter4>> adapters;
	// ここに特定の名前を持つアダプターオブジェクトが入る
	ComPtr<IDXGIAdapter4> tmpAdapter;

	// パフォーマンスが高いものから順に、全てのアダプターを列挙する
	for (UINT i = 0;
		dxgiFactory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND;
		i++)
	{
		// 動的配列に追加する
		adapters.push_back(tmpAdapter);
	}

	// 妥当なアダプタを選別する
	for (size_t i = 0; i < adapters.size(); i++)
	{
		DXGI_ADAPTER_DESC3 adapterDesc;
		// アダプターの情報を取得する
		adapters[i]->GetDesc3(&adapterDesc);
		// ソフトウェアデバイスを回避
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			// デバイスを採用してループを抜ける
			tmpAdapter = adapters[i].Get();
			break;
		}
	}

	// 対応レベルの配列
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;
	for (size_t i = 0; i < _countof(levels); i++)
	{
		// 採用したアダプターでデバイスを生成
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i],
			IID_PPV_ARGS(device.GetAddressOf()));
		if (result == S_OK) {
			// デバイスを生成できた時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

}
void RenderBase::CommandInit()
{

	HRESULT result;

	// コマンドアロケータを生成
	result = device->CreateCommandAllocator
	(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);
	assert(SUCCEEDED(result));

	// コマンドリストを生成
	result = device->CreateCommandList
	(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&commandList)
	);
	assert(SUCCEEDED(result));

	//コマンドキューの設定
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	//コマンドキューを生成
	result = device.Get()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(result));
}
void RenderBase::SwapChainInit()
{
	HRESULT result;

	// リソースの設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = (UINT)renderWindow->GetWindowSize().x;
	swapChainDesc.Height = (UINT)renderWindow->GetWindowSize().y;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				 // 色情報の書式
	swapChainDesc.SampleDesc.Count = 1;								 // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;				 // バックバッファ用
	swapChainDesc.BufferCount = 2;									 // バッファ数を２つに設定
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		 // フリップ後は破棄
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// スワップチェーンの生成
	ComPtr<IDXGISwapChain1> swapChain1;
	result = dxgiFactory->CreateSwapChainForHwnd
	(
		commandQueue.Get(),
		renderWindow->GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	);
	swapChain1.As(&swapChain);
	assert(SUCCEEDED(result));

	// デスクリプタヒープの設定
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// レンダーターゲットビュー
	rtvHeapDesc.NumDescriptors = swapChainDesc.BufferCount; // 裏表の２つ

	// デスクリプタヒープの生成
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescHeap));

	// バックバッファ
	backBuffers.resize(swapChainDesc.BufferCount);

	// スワップチェーンの全てのバッファについて処理する
	for (size_t i = 0; i < backBuffers.size(); i++)
	{
		// スワップチェーンからバッファを取得
		swapChain->GetBuffer((UINT)i, IID_PPV_ARGS(&backBuffers[i]));
		// デスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
		// 裏か表かでアドレスがずれる
		rtvHandle.ptr += i * device.Get()->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
		// レンダーターゲットビューの設定
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		// シェーダーの計算結果をSRGBに変換して書き込む
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		// レンダーターゲットビューの生成
		device->CreateRenderTargetView(backBuffers[i].Get(), &rtvDesc, rtvHandle);
	}

}
void RenderBase::FenceInit()
{

	HRESULT result;

	// フェンスの生成
	result = device->CreateFence(
		fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));

}
void RenderBase::DepthBufferInit()
{
	HRESULT result;

	// リソースの設定
	D3D12_RESOURCE_DESC depthResourceDesc{};
	depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResourceDesc.Width = (UINT)renderWindow->GetWindowSize().x;		// 幅
	depthResourceDesc.Height = (UINT)renderWindow->GetWindowSize().y; // 高さ
	depthResourceDesc.DepthOrArraySize = 1;
	depthResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;	// 深度値デフォルト
	depthResourceDesc.SampleDesc.Count = 1;
	depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 深度用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp{};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;	// 深度値1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;	// 深度値フォーマット

	// リソースの生成
	result = device->CreateCommittedResource
	(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer)
	);
	assert(SUCCEEDED(result));

	// dsv用デスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;	// 深度ビューは一つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // デプスステンシルビュー
	result = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescHeap));
	assert(SUCCEEDED(result));

	// 深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
	dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;	// 深度値フォーマット
	dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView
	(
		depthBuffer.Get(),
		&dsvViewDesc,
		dsvDescHeap->GetCPUDescriptorHandleForHeapStart()
	);

}
void RenderBase::SrvInit()
{

	HRESULT result;

	// SRVの最大個数
	const size_t kMaxSRVCount = 2056;

	// デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;	// シェーダから見えるように
	srvHeapDesc.NumDescriptors = kMaxSRVCount;

	// 設定を元にSRV用デスクリプタヒープを生成
	result = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescHeap));
	assert(SUCCEEDED(result));

}
void RenderBase::ShaderCompilerInit()
{

	// 3Dオブジェクトのシェーダー
	basicShader = std::move(std::make_unique<ShaderObject>());
	basicShader->AddInputLayout("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	basicShader->AddInputLayout("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
	basicShader->AddInputLayout("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
	basicShader->CompileVertexShader("Shader/BasicVS.hlsl", "main");
	basicShader->CompilePixelShader("Shader/BasicPS.hlsl", "main");

	// スプライト用シェーダー
	spriteShader = std::move(std::make_unique<ShaderObject>());
	spriteShader->AddInputLayout("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	spriteShader->AddInputLayout("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
	spriteShader->CompileVertexShader("Shader/SpriteVS.hlsl", "main");
	spriteShader->CompilePixelShader("Shader/SpritePS.hlsl", "main");

	// レンダーテクスチャーのシェーダー
	renderTextureShader = std::move(std::make_unique<ShaderObject>());
	renderTextureShader->AddInputLayout("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	renderTextureShader->AddInputLayout("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
	renderTextureShader->CompileVertexShader("Shader/RenderTextureVS.hlsl", "main");
	renderTextureShader->CompilePixelShader("Shader/RenderTexturePS.hlsl", "main");

	// ロードしたモデルのシェーダー
	loadModelShader = std::move(std::make_unique<ShaderObject>());
	loadModelShader->AddInputLayout("POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	loadModelShader->AddInputLayout("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT);
	loadModelShader->AddInputLayout("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);
	loadModelShader->CompileVertexShader("Shader/ObjectBasicVS.hlsl", "main");
	loadModelShader->CompilePixelShader("Shader/ObjectBasicPS.hlsl", "main");

}
void RenderBase::RootSignatureInit()
{
	HRESULT result;

	// デスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.NumDescriptors = 1;			// 一度の描画に使うテクスチャが1枚なので１
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.BaseShaderRegister = 0;		// テクスチャレジスタ0番
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// テクスチャサンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;					// 横繰り返し(タイリング)
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;					// 縦繰り返し(タイリング)
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;					// 奥行繰り返し(タイリング)
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;	// ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;					// 全てのリニア補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;									// ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;												// ミップマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;			// ピクセルシェーダからのみ使用可能

	// ルートパラメーターの設定
	D3D12_ROOT_PARAMETER rootParams[6] = {};

	// テクスチャレジスタ0番（SRV）
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // 種類
	rootParams[0].DescriptorTable.pDescriptorRanges = &descriptorRange;		  // デスクリプタレンジ
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;					  // デスクリプタレンジ数
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;			  // 全てのシェーダから見える

	// 定数バッファ（トランスフォーム）
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 種類
	rootParams[1].Descriptor.ShaderRegister = 0;					// 定数バッファ番号
	rootParams[1].Descriptor.RegisterSpace = 0;						// デフォルト値
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 全てのシェーダから見える

	// 定数バッファ（マテリアル）
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 種類
	rootParams[2].Descriptor.ShaderRegister = 1;					// 定数バッファ番号
	rootParams[2].Descriptor.RegisterSpace = 0;						// デフォルト値
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 全てのシェーダから見える

	// 定数バッファ（マテリアル）
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 種類
	rootParams[3].Descriptor.ShaderRegister = 2;					// 定数バッファ番号
	rootParams[3].Descriptor.RegisterSpace = 0;						// デフォルト値
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 全てのシェーダから見える

	// 定数バッファ（平行光源、ディレクショナルライト）
	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 種類
	rootParams[4].Descriptor.ShaderRegister = 3;					// 定数バッファ番号
	rootParams[4].Descriptor.RegisterSpace = 0;						// デフォルト値
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 全てのシェーダから見える

	// 定数バッファ（点光源、スポットライト）
	rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// 種類
	rootParams[5].Descriptor.ShaderRegister = 4;					// 定数バッファ番号
	rootParams[5].Descriptor.RegisterSpace = 0;						// デフォルト値
	rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 全てのシェーダから見える

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParams;	// ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = _countof(rootParams);		// ルートパラメータ数
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;
	// ルートシグネチャのシリアライズ
	ComPtr<ID3DBlob> rootSigBlob;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);
	assert(SUCCEEDED(result));
	result = device->CreateRootSignature
	(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	assert(SUCCEEDED(result));

}
void RenderBase::GraphicsPipelineInit()
{
	// グラフィックスパイプライン3D用
	basicPipeline = std::move(std::make_unique<GraphicsPipeline>());
	basicPipeline->SetShaderObject(basicShader.get());
	basicPipeline->SetCullMode(CullMode::CullBack);
	basicPipeline->SetisDepthEnable(true);
	basicPipeline->SetTopologyType(TopologyType::TriangleTopology);
	basicPipeline->SetRootSignature(rootSignature.Get());
	basicPipeline->Create();

	// グラフィックスパイプライン2D用
	spritePipeline = std::move(std::make_unique<GraphicsPipeline>());
	spritePipeline->SetShaderObject(spriteShader.get());
	spritePipeline->SetCullMode(CullMode::None);
	spritePipeline->SetisDepthEnable(false);
	spritePipeline->SetTopologyType(TopologyType::TriangleTopology);
	spritePipeline->SetRootSignature(rootSignature.Get());
	spritePipeline->Create();

	// グラフィックスパイプラインLine用
	linePipeline = std::move(std::make_unique<GraphicsPipeline>());
	linePipeline->SetShaderObject(basicShader.get());
	linePipeline->SetCullMode(CullMode::CullBack);
	linePipeline->SetisDepthEnable(true);
	linePipeline->SetTopologyType(TopologyType::LineTopology);
	linePipeline->SetRootSignature(rootSignature.Get());
	linePipeline->Create();

	// レンダーテクスチャ用
	renderTexturePipeline = std::move(std::make_unique<GraphicsPipeline>());
	renderTexturePipeline->SetShaderObject(renderTextureShader.get());
	renderTexturePipeline->SetCullMode(CullMode::CullBack);
	renderTexturePipeline->SetisDepthEnable(true);
	renderTexturePipeline->SetTopologyType(TopologyType::TriangleTopology);
	renderTexturePipeline->SetRootSignature(rootSignature.Get());
	renderTexturePipeline->Create();

	// 読み込みオブジェクト用
	loadModelPipeline = std::move(std::make_unique<GraphicsPipeline>());
	loadModelPipeline->SetShaderObject(loadModelShader.get());
	loadModelPipeline->SetCullMode(CullMode::CullBack);
	loadModelPipeline->SetisDepthEnable(true);
	loadModelPipeline->SetTopologyType(TopologyType::TriangleTopology);
	loadModelPipeline->SetRootSignature(rootSignature.Get());
	loadModelPipeline->Create();

}