@echo off
setlocal
if not defined DB_GENERATOR_PYTHON set "DB_GENERATOR_PYTHON=python"
"%DB_GENERATOR_PYTHON%" -B "%~dp0GenerateDb.py" %*
exit /b %errorlevel%
