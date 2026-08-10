@echo off
if not defined REDOTSTEAM_ZIG (
  echo ERROR: REDOTSTEAM_ZIG is not set. 1>&2
  exit /b 2
)
"%REDOTSTEAM_ZIG%" ar %*
