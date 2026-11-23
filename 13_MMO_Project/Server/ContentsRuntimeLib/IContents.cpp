#include "ContentsRuntimeLibPch.h"
#include "IContents.h"

namespace ContentsRuntimeLib
{
	IContents::IContents(ContentsInstanceId instanceId) noexcept
		: m_instanceId(instanceId)
	{
	}

	ContentsInstanceId IContents::GetInstanceId() const noexcept
	{
		return m_instanceId;
	}

	void IContents::OnFrame()
	{
	}
}