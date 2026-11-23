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
		[[nodiscard]] 
		HRESULT DrawBackground(float red, float green, float blue) noexcept;

	private:
		struct SResources;
		std::unique_ptr<SResources> m_pResources;
	};
}