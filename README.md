# -
这是一个自由开发的东西可以随便往里面塞任何东西

## myos/ — 自研可启动图形化操作系统

一个从零开始、不依赖任何已有操作系统的 32 位图形化 OS，烧到 U 盘直接引导。
不使用 GRUB，不依赖 Windows/Mac/Linux，无版权问题（公共领域）。

### 特性
- 自写 512 字节 MBR 引导扇区（实模式 → 保护模式）
- VGA 320x200x256 真彩点绘图（自带 8x8 点阵字体）
- 自建 GDT/IDT、PIC 重映射、100 Hz PIT 定时器、PS/2 键盘
- 12 用户体系：1 个顶级 SUPER（隐藏，内核/恢复专用）+ root + 10 用户
  （含 TRUSTED / USER 角色）
- 窗口管理器：系统监视器 / 终端 / 关于，三个独立窗口
- 定时器驱动的协作式多任务模拟
- 崩溃自动进恢复环境（任何 CPU 异常 → 红屏 → 恢复 shell）
- 开机连按任意键两次（~1.2s 内）直接进恢复环境
- SUPER 登录后可执行内核级指令（最高权限）

### 构建
```bash
cd myos
python3 build.py        # 产出 os.img
```
需要：`gcc nasm ld objcopy python3`（沙箱均已具备）。

### 写入 U 盘 / 启动
```bash
sudo dd if=myos/os.img of=/dev/sdX bs=4M conv=fsync && sync
```
然后把 U 盘插上目标机器，BIOS 设为 USB 优先启动即可（无需任何已有 OS）。
`/dev/sdX` 用你的 U 盘设备名替换，**该盘所有数据会被清空**。

### 在 QEMU 里测试（可选）
```bash
qemu-system-x86_64 -drive format=raw,file=myos/os.img
```

### 默认账户
| 用户 | 角色 | 密码 | 说明 |
|------|------|------|------|
| super | SUPER | root | 顶级权限，仅恢复/内核可用 |
| root  | ROOT  | toor | 系统管理员 |
| admin / dave / grace | TRUSTED | admin1 / dave11 / grace1 | 受信用户 |
| bob / carol / eve / frank / heidi / ivan | USER | bob123 / ... | 普通用户 |

### 文件
- `myos/boot.asm` — MBR 引导扇区
- `myos/entry.asm` — 内核入口 + ISR 桩
- `myos/kernel.c` — 图形内核主体
- `myos/linker.ld` — 链接脚本（内核定位于 0x10000）
- `myos/build.py` — 构建编排（产出 os.img）
- `myos/os.img` — 可直接 dd 到 U 盘的可启动镜像
