@echo off
setlocal DisableDelayedExpansion
if not defined LOG_SPLITTER_PYTHON set "LOG_SPLITTER_PYTHON=python"
"%LOG_SPLITTER_PYTHON%" -X utf8 -B "%~dp0SplitLogs.py" %*
exit /b %errorlevel%
