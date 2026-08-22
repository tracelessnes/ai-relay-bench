# Windows-only MIT Release Plan

## Scope

- Add an MIT license with the repository copyright holder.
- Declare Windows-only support in documentation and CMake configuration.
- Make the Windows packaging script include the license and produce a portable ZIP.
- Run Release build, tests, sensitive-material scan, and publish a GitHub Release.

## Verification

- Configure/build with Qt 6.11.1 MinGW Release.
- Run all CTest targets.
- Run scripts/check-sensitive.ps1 and git diff --check.
- Verify the ZIP contains the executable, Qt runtime, README, and LICENSE.
- Verify the GitHub tag and Release asset.
