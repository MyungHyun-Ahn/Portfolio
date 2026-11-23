#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <wrl/client.h>
#include <memory>
#include <new>
#include <utility>
#include "CDirect2DRenderer.h"

namespace ClientRenderLib
{
	// delete가 아닌 Release로 참조를 반납해야 하는 COM 객체에는 ComPtr을 사용한다.
	using Microsoft::WRL::ComPtr;
	constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

	struct CDirect2DRenderer::SResources
	{
		ComPtr<IDXGISwapChain1> spSwapChain;  	// 창에 표시할 이미지 버퍼들을 관리하는 객체 
												// - 더블 버퍼링을 담당함(버퍼를 묶어서 번갈아 쓰는 구조)
												// - BufferCount = 2 설정
		ComPtr<ID2D1DeviceContext> spContext; 	// 그림을 그리는 객체
		ComPtr<ID2D1SolidColorBrush> spRectangleBrush;
	};

	CDirect2DRenderer::CDirect2DRenderer() noexcept = default;
	CDirect2DRenderer::~CDirect2DRenderer() = default;

	// Direct2D로 그린 결과가 이 창에 나오도록 연결하는 함수
	HRESULT CDirect2DRenderer::Initialize(HWND window) noexcept
	{
		// 입력 확인과 임시 자원 준비
		if (window == nullptr)
			return E_INVALIDARG;
		if (m_pResources)
			return E_UNEXPECTED;

		std::unique_ptr<SResources> pResources(new (std::nothrow) SResources{});
		if (!pResources)
			return E_OUTOFMEMORY;

		// Direct2D에서도 쓸 수 있도록 D3D11 디바이스에 BGRA 지원을 켠다.
		ComPtr<ID3D11Device> spD3dDevice;
		// 하드웨어 장치를 먼저 만든다.
		HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
										   nullptr, 0, D3D11_SDK_VERSION, spD3dDevice.GetAddressOf(), nullptr, nullptr);
		if (FAILED(result))
		{
			// 하드웨어 장치 생성에 실패하면 소프트웨어 렌더링인 WARP로 재시도한다.
			result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
									   nullptr, 0, D3D11_SDK_VERSION, spD3dDevice.ReleaseAndGetAddressOf(), nullptr, nullptr);
			if (FAILED(result))
				return result;
		}

		// 위에서 얻은 D3D11와 같은 장치의 DXGI 인터페이스를 얻는다.
		ComPtr<IDXGIDevice> spDxgiDevice;
		result = spD3dDevice.As(&spDxgiDevice);
		if (FAILED(result))
			return result;

		// 장치가 사용하는 그래픽 어댑터를 얻는다.
		ComPtr<IDXGIAdapter> spAdapter;
		result = spDxgiDevice->GetAdapter(spAdapter.GetAddressOf());
		if (FAILED(result))
			return result;

		// 그 어댑터의 상위 DXGI 팩터리를 얻는다.
		ComPtr<IDXGIFactory2> spDxgiFactory;
		result = spAdapter->GetParent(IID_PPV_ARGS(spDxgiFactory.GetAddressOf()));
		if (FAILED(result))
			return result;

		// 창에 연결할 스왑 체인을 만든다.
		DXGI_SWAP_CHAIN_DESC1 swapChainDescription{};
		swapChainDescription.Format = kBackBufferFormat;
		swapChainDescription.SampleDesc.Count = 1;
		swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDescription.BufferCount = 2;
		swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		result = spDxgiFactory->CreateSwapChainForHwnd(spD3dDevice.Get(), window, &swapChainDescription, nullptr, nullptr, pResources->spSwapChain.GetAddressOf());
		if (FAILED(result))
			return result;

		// 창 크기는 앱에서 처리하지만 전체화면 전환은 아직 지원하지 않으므로 DXGI의 Alt+Enter 자동 전환을 막는다.
		// - 나중에 앱에서 전체화면 전환을 직접 구현할 것
		result = spDxgiFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
		if (FAILED(result))
			return result;

		// Direct2D 객체를 만드는 출발점
		// - SINGLE_THREADED : 여러 스레드에서 동시에 접근할 때 Direct2D가 대신 동기화 해주지 않는다는 뜻
		ComPtr<ID2D1Factory1> spD2dFactory;
		result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, spD2dFactory.GetAddressOf());
		if (FAILED(result))
			return result;

		// 기존 DXGI 장치에 연결된 Direct2D 장치를 만든다.
		// - D3D11 장치를 또 만드는 게 아닌, 같은 그래픽 기반에서 Direct2D 리소스와 컨텍스트를 쓸 수 있게 함
		ComPtr<ID2D1Device> spD2dDevice;
		result = spD2dFactory->CreateDevice(spDxgiDevice.Get(), spD2dDevice.GetAddressOf());
		if (FAILED(result))
			return result;

		// 실제로 그리기 명령을 받을 객체를 만든다.
		// - BeginFrame()과 EndFrame()에서 BeginDraw(), Clear(), EndDraw()를 호출하는 객체.
		result = spD2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, pResources->spContext.GetAddressOf());
		if (FAILED(result))
			return result;

		result = bindBackBufferTarget(*pResources);
		if (FAILED(result))
			return result;

		result = pResources->spContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
			pResources->spRectangleBrush.GetAddressOf());
		if (FAILED(result))
			return result;

		// 준비가 끝난 리소스를 랜더러에 넘긴다.
		m_pResources = std::move(pResources);
		return S_OK;
	}

	HRESULT CDirect2DRenderer::Resize(UINT widthPixels, UINT heightPixels) noexcept
	{
		if (!m_pResources)
			return E_UNEXPECTED;

		if (widthPixels == 0 || heightPixels == 0)
			return E_INVALIDARG;

		auto& resources = *m_pResources;
		DXGI_SWAP_CHAIN_DESC1 description{};
		HRESULT result = resources.spSwapChain->GetDesc1(&description);
		if (FAILED(result))
			return result;

		if (description.Width == widthPixels && description.Height == heightPixels)
			return S_OK;

		// 리사이즈 과정 #1 : 이전 그리기 대상을 떼어낸다.
		// - 컨텍스트가 이전 타깃을 붙들고 있으면 DXGI가 백 버퍼를 교체할 수 없다.
		resources.spContext->SetTarget(nullptr);

		// 리사이즈 과정 #2 : 같은 스왑 체인 안에서 백 버퍼를 새 크기로 교체한다.
		result = resources.spSwapChain->ResizeBuffers(0, widthPixels, heightPixels, DXGI_FORMAT_UNKNOWN, description.Flags);
		if (FAILED(result))
			return result;

		return bindBackBufferTarget(resources);
	}

	HRESULT CDirect2DRenderer::bindBackBufferTarget(SResources& resources) noexcept
	{
		// 리사이즈 과정 #3 - 새 버퍼를 준비한다.
		// Direct2D의 그리기 대상은 스왑 체인의 백 버퍼다.
		ComPtr<IDXGISurface> spBackBuffer;
		HRESULT result = resources.spSwapChain->GetBuffer(0, IID_PPV_ARGS(spBackBuffer.GetAddressOf()));
		if (FAILED(result))
			return result;

		// 버퍼를 어떻게 사용할지 정한다. - 아직 비트맵을 만드는 건 아님
		// TARGET : 이 비트맵에 그릴 수 있다.
		// CANNOT_DRAW : DrawBitmap() 같은 호출의 입력 이미지로 쓸 수 없다.
		// 즉, 여기에 그릴수는 있지만, 이걸 다른 곳에 그리는 재료로 쓰진 않는다는 뜻
		const auto targetProperties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(kBackBufferFormat, D2D1_ALPHA_MODE_IGNORE));

		ComPtr<ID2D1Bitmap1> spTarget;
		result = resources.spContext->CreateBitmapFromDxgiSurface(spBackBuffer.Get(), &targetProperties, spTarget.GetAddressOf());
		if (FAILED(result))
			return result;

		// 리사이즈 과정 #4 : 새 버퍼를 Direct2D의 그리기 대상으로 연결한다.
		resources.spContext->SetTarget(spTarget.Get());
		return S_OK;
	}

	void CDirect2DRenderer::BeginFrame(float red, float green, float blue) noexcept
	{
		m_pResources->spContext->BeginDraw();
		m_pResources->spContext->Clear(D2D1::ColorF(red, green, blue));
	}

	void CDirect2DRenderer::FillRectangle(float left, float top, float right, float bottom) noexcept
	{
		m_pResources->spContext->FillRectangle(D2D1::RectF(left, top, right, bottom), m_pResources->spRectangleBrush.Get());
	}

	HRESULT CDirect2DRenderer::EndFrame() noexcept
	{
		const HRESULT result = m_pResources->spContext->EndDraw();
		if (FAILED(result))
			return result;

		// 백 버퍼에 그린 프레임을 스왑체인에 제출해서, 창에 표시할 프레임을 전환한다.
		return m_pResources->spSwapChain->Present(1, 0);
	}
}