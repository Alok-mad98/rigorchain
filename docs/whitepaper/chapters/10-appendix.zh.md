# 附录 — 协议常数与工程参考

第一至十二章是公开论证。本附录是查阅。它不增加主张，不改写 `core/src/`，不把某一天的定理计数写成协议参数。

事实源仍是运行中的 C++ 节点：`core/src/consensus/params.h`，`core/src/consensus/rigor_consensus_lock.h`。对外宣传句只在第八章 8.5，此处不复述。

---

## A. 冻结公开参数

| 符号 | 值 |
|------|----|
| ORU 补贴 | `R(0) = 0`；`h ≥ 1` 每块 128 ORU，无减半 |
| `ORU_MIN_BLOCK_SUBSIDY` | 128 ORU |
| 铸造谓词 | `R > 0 → R · (2²⁵ + h)² ≤ B << 40` |
| 展示式 | `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰` ORU / 整枚 RES（`2⁵⁰` = 曲线归一化分母，不是共识移位） |
| 创世整枚成本 | 1024 ORU / 1 RES（`destroy_cost(0) = 102400000000` 原子） |
| `RES_DESTROY_POLE` | `2²⁵ = 33 554 432` |
| `RES_DESTROY_SHIFT` | `40 = 50 − log₂(1024)`（整数共识缩放因子 `B << 40`；不是 `destroy_cost` 分母） |
| 导出上界 | `2⁷ / 2¹⁰ · 2²⁵ = 2²² = 4 194 304` RES（`419430400000000` 原子） |
| 累计上界 | `cumulative(h) = 2²² · h / (h + 2²⁵)` |
| `RES_COIN` | 100000000 原子 / 整枚 |
| 铸造位置 | 主体交易：唯一正值 `OP_CONVERGE` + 同笔 Witness v1 RES |
| 铸造成熟 | 100（与 coinbase 相同；重组保护） |
| RFC-018 | RES 转账 ORU 费一半不可领取：`Σ floor(fee(tx) / 2)` |
| 反向 | 禁止 RES → ORU |

探针（原子，展示辅助函数；展示 rounding/floor 不进入共识；共识用不等式）：

| 高度 | `destroy_cost(h)` 原子 | ORU / 1 RES |
|-----:|----------------------:|------------:|
| 0 | 102400000000 | 1024 |
| 210000 | 103685749152 | 1036.85749152 |
| 420000 | 104979520049 | 1049.79520049 |

---

## B. 铸造拒绝条件

失败即关闭。公开形式（RFC-020）：

| 拒绝码 | 含义 |
|--------|------|
| `bad-txns-res-mint-underpaid` | `R · (2²⁵ + h)² > B << 40` |
| `bad-txns-res-mint-no-burn` | 有 RES 输出，无 `OP_CONVERGE` 输出 |
| `bad-txns-res-mint-cap-overshoot` | 将超过 `419430400000000` 原子 |

---

## C. 脚本与身份

| 形状 | 身份 | 可花费 |
|------|------|--------|
| Witness v0，20 字节程序 | ORU | 是 |
| Witness v1，32 字节程序 | RES | 是（须合法诞生） |
| `<HASH160> 0xbd`（`OP_CONVERGE`） | ORU 销毁容器 | 否；不进入 UTXO 集 |

新的货币输出不得为：P2PKH、P2SH、P2WSH、裸多签、未知见证版本、Taproot。Witness v1 不是 Taproot。花费签名为 Ed25519。

Coinbase 只写明本块高度，只付 ORU。

---

## D. 工作量证明

| 项 | 值 |
|----|----|
| 出块哈希 | OpuHash = SHA3-256(`"RGRPOW01" \|\| header_80`) |
| 有效 | `OpuHash(header) ≤ target` |
| 区块身份 | SHA256d（同一 80 字节头；不是 OpuHash） |
| ASERT 间距 | 600 s |
| ASERT 半衰期 | 43200 s |
| ASERT 夹紧 | ±12.5% |
| 难度 | 不是铸造价格，不进入谓词 |
| 高度 | 块计数索引；ASERT 不把 `h` 绑定到墙钟时间 |

---

## E. 工程索引

| 路径 | 查阅什么 |
|------|----------|
| `core/src/` | 运行共识；唯一事实源 |
| `core/src/consensus/params.h` | 冻结常数 |
| `core/src/consensus/rigor_consensus_lock.h` | 锁 |
| `formal_verification/rigor_kernel/` | Coq 内核模型 |
| `formal_verification/rigor_kernel/release/FinalProofInventory.md` | 已分类定理清单（快照，不是参数） |
| `formal_verification/rigor_kernel/release/FormalizationFrontier.md` | 形式化前沿 |
| `docs/verification/FORMAL-VERIFICATION-VERDICT.md` | 模型证明了什么 |
| `docs/security/HOW_TO_VERIFY_RIGOR.md` | 如何复现 |
| `docs/developer/CODE_SSOT_PROTOCOL_FACTS.md` | 代码事实摘录 |
| `docs/specification/RIGOR_OPCODE_SPECIFICATION.md` | 操作码 |
| `docs/mining/CONVERGENCE_ROOT_CANONICAL_SPEC.md` | Convergence Root 编码 |
| `docs/architecture/CONSENSUS_FINGERPRINT_LIBRARY.md` | 指纹库（观察层，不是规则） |
| `/documentation/formal-verification` | 公开边界页 |
| `/zero-trust-verification` | 选定字段的浏览器重算 |

名词表（不是第九章叙事）：[`09-glossary.md`](./09-glossary.md)。

---

## F. 现行与历史文档

| 文档 | 角色 |
|------|------|
| RFC-020 | 现行 RES 发行：连续 `destroy_cost(h)`，平坦 128 ORU，主体销毁—铸造 |
| RFC-018 | RES 转账 ORU 费一半销毁 |
| RFC-000 | 共识宪章 |
| SPEC-002 | ORU / RES 递推 |
| `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | 冻结声明 |
| RFC-001 / 002 / 008 | 历史规格；以代码与 RFC-020 为准 |

本白皮书不编辑 RFC 原文。历史条目不是现行规则。

---

## G. 名词

| 词 | 官方含义 |
|----|----------|
| ORU | 交换媒介资产。创世后每块补贴 128 |
| RES | 协议内设计的价值存储资产。导出上界 `2²²` RES；非挖出 |
| Convergence | 同笔 `OP_CONVERGE` 销毁 ORU，可以按谓词铸造 Witness v1 RES |
| `destroy_cost(h)` | 展示用铸造成本。`2⁵⁰` 是曲线归一化分母。共识计算 `B << 40`，不计算该商。展示 rounding/floor 不进入共识 |
| OpuHash | 现行工作量证明有效性哈希（不是区块身份） |
| 共识指纹 | 对公开字段的观察性身份。不接受、不拒绝块 |

查阅到此为止。主张在第一至十二章。

铸造恒等的代数推导见附录 D：
[`../RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md`](../RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md)。
该数学设计说明是解释性文档。它不引入新的共识规则。
