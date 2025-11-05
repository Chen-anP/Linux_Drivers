# chrdevbase 模块加载失败诊断与修复总结

日期：2025-10-29

本文档总结了从最初错误到最终解决的全过程、用到的诊断命令、根本原因分析和可复现的修复步骤，便于存档与日后复用。

---

## 问题描述
在目标开发板（aarch64，内核 `5.10.160-rockchip-rk3588`）上加载外部模块 `chrdevbase.ko` 出现失败：
- 最初 modprobe/insmod 返回 “Invalid module format” 或 “Exec format error”。
- dmesg 中出现多条：`chrdevbase: disagrees about version of symbol module_layout`。

## 最终结果
通过把目标的运行内核 `.config` 导入到交叉编译虚拟机（VM）中的内核源码、对齐 `CONFIG_LOCALVERSION`，并在 VM 中用相同交叉工具链准备内核树后重新构建模块，模块在目标上成功加载：
- dmesg 最终显示 `chrdevbase init!`
- `lsmod` 显示已加载模块
- `modinfo chrdevbase.ko` 的 `vermagic` 与目标内核对齐（`5.10.160-rockchip-rk3588+ SMP mod_unload modversions aarch64`）

---

## 诊断步骤与关键命令（及目的）

- 在目标上：
  - `uname -r` — 确认运行内核版本（用于对齐 vermagic）。
  - `dmesg | tail -n 60` — 查看加载失败时的内核日志，查找 `disagrees about version` 等信息。
  - `readlink -f /lib/modules/$(uname -r)/build` — 查看 build 链接指向何处。
  - `zcat /proc/config.gz > /tmp/running_kernel.config`（若可用） — 导出目标运行时的内核配置。

- 在编译机（VM）上：
  - 把目标的 `running_kernel.config` 复制到内核源码目录并改名为 `.config`。
  - `sed -i 's/^CONFIG_LOCALVERSION=".*"/CONFIG_LOCALVERSION="-rockchip-rk3588"/' .config` — 对齐 localversion（根据 `uname -r`）。
  - `export ARCH=arm64`、`export CROSS_COMPILE=<aarch64 toolchain prefix>` — 设置交叉编译环境。
  - `make olddefconfig`、`make modules_prepare` — 准备内核头与模块构建环境。
  - `make -C $KERNELDIR M=$MODULEDIR ARCH=arm64 CROSS_COMPILE=... modules` — 构建外部模块。
  - `modinfo ./chrdevbase.ko` — 检查 vermagic（确认内核版本字符串匹配）。

- 在目标上部署测试：
  - 拷贝 `.ko` 到 `/lib/modules/$(uname -r)/`，运行 `sudo depmod -a`。
  - `sudo modprobe -v chrdevbase` 或 `sudo insmod /lib/modules/../chrdevbase.ko`，再 `dmesg` 查看结果。

---

## 根本原因

内核启用了 modversions（CONFIG_MODVERSIONS），导出符号带有 CRC（符号版本）。如果外部模块在与目标内核不同的构建环境下构建（不同的 `.config`、不同 localversion、不同 `Module.symvers` 或不同编译器/补丁），则 MODPOST 生成的符号 CRC 可能与运行内核不同，导致内核在加载时报：“disagrees about version of symbol ...”，并拒绝加载模块（表现为 Invalid module format / Exec format error）。

此外，vermagic 中的内核版本字符串（含 localversion）若不一致也会造成加载警告/拒绝，因此需要保证 vermagic 与目标内核对齐。

---

## 修复措施（可复现步骤）

下面是可复制的最小步骤（替换路径和工具链前缀）：

1. 在目标上导出 config：
```bash
zcat /proc/config.gz > /tmp/running_kernel.config
# 或者 cp /boot/config-$(uname -r) /tmp/running_kernel.config
```

2. 在 VM（内核源码位于 `$KERNELDIR`）执行：
```bash
cd $KERNELDIR
cp /tmp/running_kernel.config .config
# 把 localversion 设置为目标的后缀（示例：-rockchip-rk3588）
sed -i 's/^CONFIG_LOCALVERSION=".*"/CONFIG_LOCALVERSION="-rockchip-rk3588"/' .config
export ARCH=arm64
export CROSS_COMPILE=/path/to/aarch64-none-linux-gnu-
make olddefconfig
make modules_prepare
```

3. 在 VM 上确保 `Module.symvers` 一致：
- 如果内核源码顶层没有正确的 `Module.symvers`，建议在内核源码树执行 `make modules`（会花时间）以生成它；或者确保你没有在模块源码目录里保留旧的 `Module.symvers`，以避免 MODPOST 使用错误的表。

4. 构建外部模块：
```bash
cd /path/to/chrdevbase
rm -f Module.symvers
make -C $KERNELDIR M=$PWD ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE modules
modinfo ./chrdevbase.ko
# 确认 vermagic 中包含 5.10.160-rockchip-rk3588
```

5. 部署到目标并加载验证：
```bash
scp ./chrdevbase.ko root@target:/tmp/
# 在目标上
sudo cp /tmp/chrdevbase.ko /lib/modules/$(uname -r)/
sudo depmod -a
sudo modprobe -v chrdevbase || sudo insmod /lib/modules/$(uname -r)/chrdevbase.ko
dmesg | tail -n 40
lsmod | grep chrdevbase
```

---

## 经验与最佳实践

- 在交叉编译模块前必做：确保 `.config`（尤其 `CONFIG_LOCALVERSION`）与目标内核匹配。
- 确保 MODPOST 使用的 `Module.symvers` 来源于与目标一致的内核构建产物；缺失时在内核源树里运行一次 `make modules` 能生成它。
- 使用一致的交叉编译器版本（不同编译器也可能影响符号 CRC）。
- 构建后先在构建主机用 `modinfo` 检查 vermagic，再部署。

---

## 参考命令速查（常用）

```bash
# 目标：查看内核版本、导出 config、查看 dmesg
uname -r
zcat /proc/config.gz > /tmp/running_kernel.config
readlink -f /lib/modules/$(uname -r)/build

# VM：对齐 config、准备内核、编译模块
cd /path/to/kernel
cp /tmp/running_kernel.config .config
sed -i 's/^CONFIG_LOCALVERSION=".*"/CONFIG_LOCALVERSION="-rockchip-rk3588"/' .config
export ARCH=arm64
export CROSS_COMPILE=/path/to/aarch64-none-linux-gnu-
make olddefconfig
make modules_prepare
rm -f /path/to/modulesrc/Module.symvers
make -C /path/to/kernel M=/path/to/modulesrc ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE modules
modinfo /path/to/modulesrc/chrdevbase.ko

# 目标：部署并加载
sudo cp chrdevbase.ko /lib/modules/$(uname -r)/
sudo depmod -a
sudo modprobe chrdevbase

dmesg | tail -n 40
```

---

## 后续建议
- 若你希望把流程自动化：可以编写一个 VM 上的脚本来自动完成 config 拷贝、localversion 对齐、make modules_prepare、生成 Module.symvers（如需）并构建外部模块，我可以为你生成该脚本。
- 若模块需要随系统启动自动加载：可以在目标上创建 `/etc/modules-load.d/chrdevbase.conf` 或写一个 systemd 服务来在启动时加载并运行你的用户态程序。

---

如果你需要，我可以现在生成：
- 在 VM 上运行的自动化构建脚本；或
- 在目标上配置模块开机自动加载的文件或 systemd 单元。

请选择下一步要我自动生成的内容。