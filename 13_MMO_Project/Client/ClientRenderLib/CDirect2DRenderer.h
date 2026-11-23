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
		// 초기화 성공 후 BeginFrame → 그리기 → EndFrame 순서로 호출한다.
		void BeginFrame(float red, float green, float blue) noexcept;
		void FillRectangle(float left, float top, float right, float bottom) noexcept;
		[[nodiscard]] HRESULT EndFrame() noexcept;

	private:
		struct SResources;
		std::unique_ptr<SResources> m_pResources;
	};
}