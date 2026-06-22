@echo off
where uv >nul 2>&1
if errorlevel 1 (
    echo uv not found. Installing via pip...
    python -m pip install uv
)
uv sync
echo.
echo Setup complete. Run: uv run python viewer.py
