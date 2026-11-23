#pragma once

namespace ClientRenderLib
{
	class CDirect2DRenderer
	{
	public:
		CDirect2DRenderer() noexcept;
		~CDirect2DRenderer();
		CDirect2DRenderer(const CDirect2DRenderer&) = delete;
		CDirect2DRenderer& operator=(const CDirect2DRenderer&) = delete;

		[[nodiscard]] // Direct2DRenderer를 초기화 합니다.
		HRESULT Initialize(HWND window) noexcept;
		// 초기화 이후 프레임 밖에서 슬롯마다 한 번 호출한다. 이미 로드한 슬롯은 E_UNEXPECTED를 반환한다.
		[[nodiscard]]
		HRESULT LoadSpritePng(unsigned int slot, const wchar_t* pPath) noexcept;
		// 초기화 후 프레임 사이에 호출한다. 실패하면 더 이상 프레임을 그리지 않는다.
		[[nodiscard]] 
		HRESULT Resize(UINT widthPixels, UINT heightPixels) noexcept;
		// 초기화 성공 후 BeginFrame → 그리기 → EndFrame 순서로 호출한다.
		void BeginFrame(float red, float green, float blue) noexcept;
		void FillRectangle(float left, float top, float right, float bottom) noexcept;
		// 지정한 슬롯에서 픽셀 단위 소스 사각형을 잘라 그린다.
		struct SSpriteSourceRect
		{
			float m_leftPixels;
			float m_topPixels;
			float m_rightPixels;
			float m_bottomPixels;
		};
		void DrawSprite(unsigned int slot, float left, float top, float right, float bottom, SSpriteSourceRect source) noexcept;
		[[nodiscard]] 
		HRESULT EndFrame() noexcept;

	private:
		struct SResources;
		[[nodiscard]] 
		static HRESULT bindBackBufferTarget(SResources& resources) noexcept;

	private:
		std::unique_ptr<SResources> m_pResources;
	};
}