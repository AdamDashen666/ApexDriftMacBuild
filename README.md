# APEX DRIFT Mac 版构建

在 GitHub Actions 的 Mac 构建机上，用 Unreal Engine 5.9 源码编译 APEX DRIFT 的 Mac 版（Apple 芯片）。

- 本仓库只有构建流程（`.github/workflows/build-mac.yml`）和游戏的 C++ 代码、配置（`project/`）。
- **不含** Unreal Engine 源码：构建时用有权限的账号临时拉取，不提交、不公开。
- **不含** 美术资源和成品：内容在 Windows 上预先烘焙好，成品 dmg 发到私有仓库。

Unreal® Engine 是 Epic Games 的商标。游戏素材署名见 [project/CREDITS.txt](project/CREDITS.txt)。
