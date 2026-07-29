# MSPM0G3507 GCC / VSCode 构建与下载

本目录提供独立于 Keil 工程的 GCC 构建入口，不修改 Keil 的 `uvprojx`、scatter 文件或 Keil 启动文件。

## 构建

在工程根目录 `MSPM0G3507_Project` 中执行：

```powershell
D:\msys64\usr\bin\make.exe -f MSPM0G3507_FreeRTOS/gcc/Makefile all -j2
```

产物位于 `MSPM0G3507_FreeRTOS/gcc/build/`：

- `MSPM0G3507_FreeRTOS.elf`
- `MSPM0G3507_FreeRTOS.bin`
- `MSPM0G3507_FreeRTOS.hex`
- `MSPM0G3507_FreeRTOS.map`

VSCode 中可通过任务面板执行 `MSPM0: GCC build`。

## J-Link 探测与下载

先连接 J-Link、给目标板供电并接好 SWD，再执行 VSCode 任务：

1. `MSPM0: Probe J-Link`：只探测调试器和目标连接，不写 Flash。
2. `MSPM0: GCC download (J-Link)`：先构建，然后下载 GCC 生成的 HEX。

命令行等价操作：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\MSPM0G3507_FreeRTOS\gcc\download_jlink.ps1 -ProbeOnly
powershell -NoProfile -ExecutionPolicy Bypass -File .\MSPM0G3507_FreeRTOS\gcc\download_jlink.ps1 -Program
```

下载脚本只发送复位、`loadfile`、校验和运行命令，不发送整片擦除命令。未连接 J-Link 时会明确失败，不会误报下载成功。

## 内存注意事项

当前 GCC 链接结果大约为：

- Flash：102,940 / 131,072 字节，约 78.54%
- SRAM：静态数据、FreeRTOS heap 和主栈合计约 32,588 / 32,768 字节，约 99.45%

虽然链接通过，但 SRAM 裕量只有约 180 字节，不能据此认为系统已经完成运行时安全验证。首次下载后应通过 `rtosdiag`、任务栈 high-water mark、FreeRTOS heap 剩余量和异常钩子确认实际运行安全；如出现栈溢出、堆分配失败或 DMA/中断异常，应先降低静态内存占用或重新规划任务栈。

## Keil 兼容性

GCC 适配使用独立的：

- `startup_mspm0g350x_gcc.s`
- `mspm0g3507_gcc.ld`
- `Makefile`
- `download_jlink.ps1`

不会覆盖 Keil 的启动文件和链接配置。`.vscode/` 当前由仓库 `.gitignore` 忽略，因此 VSCode 配置属于本机工作区配置；如需提交给其他开发机，应单独调整 `.gitignore` 后再提交。
