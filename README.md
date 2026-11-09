# TF-WORK

机巢固件工作区，三个工程直接存放在 `main` 分支的目录中：

| 目录 | 工程 | Keil 工程文件 |
| --- | --- | --- |
| `xj_jc/` | 机巢主控 | `xj_jc/MDK/xj_jc.uvprojx` |
| `sun_control/` | 太阳能分发板 | `sun_control/MDK/sun_control.uvprojx` |
| `wether/` | 气象板 | `wether/MDK/wether.uvprojx` |

这三个目录是普通源码目录，克隆后即可获得全部工程。各工程原有提交历史已合并保留在本仓库中。

## 获取与更新

```bash
git clone git@github.com:sigerio/TF-WORK.git
cd TF-WORK
git pull --ff-only
```

修改工程后，在此工作区提交并推送 `main`。总仓库 [TF](https://github.com/sigerio/TF) 通过一个 `TF-WORK` 子模块固定本工作区版本。

## 构建

分别使用三个工程 `MDK/` 下的 Keil 工程文件构建。构建完成后，可运行 `copy_hex.bat` 将三个HEX复制到工作区根目录；生成的HEX和本地构建日志不纳入工作区提交。

## 文档

- [系统通信与设计整理](docs/系统通信与设计整理.md)
- [主控工程说明](xj_jc/docs/README.md)
- [主控 IEC104 协议](xj_jc/iec104_protocol_spec.md)
- [门平台测试模式需求与验证记录](xj_jc/docs/门平台测试模式需求说明.md)

主控包含104测试模式；软件验证结果及现有测试限制见需求文档。仓库导入不表示固件已经过实机验证。
