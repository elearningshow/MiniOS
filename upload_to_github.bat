@echo off
setlocal
cd /d "F:\AI\xAI-projects\MiniOS\github"

where git >nul 2>nul
if errorlevel 1 (
  echo Git is not installed. Get it from https://git-scm.com/download/win
  pause & exit /b 1
)

if not exist ".git" (
  git init
  git branch -M main
)

git remote get-url origin >nul 2>nul
if errorlevel 1 (
  set /p GHUSER=GitHub username: 
  set /p GHREPO=Repository name [MiniOS]: 
  if "%GHREPO%"=="" set GHREPO=MiniOS
  git remote add origin https://github.com/%GHUSER%/%GHREPO%.git
)

set /p MSG=Commit message [Update]: 
if "%MSG%"=="" set MSG=Update

git add -A
git commit -m "%MSG%"
git push -u origin main

echo.
echo Done. Check your repository on GitHub.
pause