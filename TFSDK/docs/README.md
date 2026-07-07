# TF SDK 文档

当前文档包含两类内容:

- [TF SDK 开发依据与开发规则](ARCHITECTURE.md)
- `develop/` 下的开发注意事项文档

其中:

- `ARCHITECTURE.md` 用于定义统一架构规则
- `develop/` 用于沉淀具体平台开发过程中的可复用注意事项

`ARCHITECTURE.md` 用于指导:

- SDK 架构设计
- 模块开发
- 接口收敛
- 代码评审
- 后续重构

阅读原则:

1. 先读 `ARCHITECTURE.md`
2. 再读对应平台或模块的 `develop/` 注意事项
3. 后续所有实现和讨论都以 `ARCHITECTURE.md` 为准
4. 如果代码与文档冲突，以文档定义的目标状态为准

说明:

- 本目录不保留历史讨论型文档
- 不保留旧版路线图、变更日志、分析记录
- `develop/` 中只允许保留已经收敛为可执行注意事项的开发文档
- 若某条注意事项已经上升为通用规则，应并入 `ARCHITECTURE.md`
