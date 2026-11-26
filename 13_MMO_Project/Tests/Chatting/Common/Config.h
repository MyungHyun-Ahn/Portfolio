#pragma once

namespace Chatting
{
	[[nodiscard]] 
	bool GetExecutableDirectory(std::wstring &outDirectory);
	[[nodiscard]] 
	bool CheckConfigFile(const WCHAR *pPath);

	// 설정 오류를 콘솔에 남기고 실패를 반환한다.
	bool ConfigError(const WCHAR *pFileName, const WCHAR *pReason);
	// 실행 파일 옆의 UTF-16LE BOM 설정을 파싱한다.
	[[nodiscard]]
	bool LoadConfigFile(const WCHAR *pFileName, MHLib::utils::CFileLoader &outLoader);
}
