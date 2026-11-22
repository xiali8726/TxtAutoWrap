# TxtAutoWrap

Notepad++ 插件：打开 / 切换到 `.txt` 文件时自动开启**显示换行**（只影响显示，不改动文件内容）；
其他类型的文件沿用 “视图 > 自动换行” 的全局设置。

*A Notepad++ plugin that automatically enables word wrap for `.txt` files only, leaving other file types on your global wrap setting.*

## 安装

1. 从 [Releases](https://github.com/xiali8726/TxtAutoWrap/releases) 下载 `TxtAutoWrap_x64.zip`（64 位 Notepad++）。
2. 解压，把 `TxtAutoWrap.dll` 放到：
   `<Notepad++目录>\plugins\TxtAutoWrap\TxtAutoWrap.dll`
3. 重启 Notepad++。

## 菜单（插件 > TxtAutoWrap）

- **对 .txt 文件自动换行**：开关，状态保存在 `<插件配置目录>\TxtAutoWrap.ini`，重启后保留
- **帮助**：功能说明与注意事项
- **关于**：作者 / 版本 / 许可证

## 注意事项

1. 按扩展名判断（不区分大小写），未保存的“新文件”不会自动换行。
2. “另存为”或重命名改变扩展名后会重新判断。
3. 分屏时，若在非 txt 文件上手动切换了“视图 > 自动换行”，另一侧的 txt 可能暂时不换行，点一下该侧窗口即可恢复。

## 编译

需要 MinGW-w64（x64），例如：

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

然后在源码目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## 许可证

[GPL-3.0](LICENSE) © xiali8726
