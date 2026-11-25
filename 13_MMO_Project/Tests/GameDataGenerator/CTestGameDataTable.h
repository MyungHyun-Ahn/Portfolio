#pragma once

// 묶음의 정적 조회와 별개로 판독 시험용 인스턴스의 행을 확인한다.
template<typename TBase>
class CTestGameDataTable final : public TBase
{
public:
	[[nodiscard]]
	bool Init() override
	{
		return true;
	}

	[[nodiscard]]
	bool Validate() const override
	{
		return true;
	}

	[[nodiscard]]
	auto FindLocal(std::uint32_t id) const
	{
		const auto& rows = this->GetRows();
		const auto iter = rows.find(id);
		return iter == rows.end() ? nullptr : &iter->second;
	}
};