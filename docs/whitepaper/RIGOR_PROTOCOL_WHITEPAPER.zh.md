# RIGOR 协议白皮书

**双原生资产工作量证明协议：Convergence 与独立验证**

| 字段 | 值 |
|------|----|
| 文档地位 | 公开协议叙述（不是共识唯一事实源） |
| 日期 | 2026-09-15 |

本文是公开协议叙述。它说明目的、共识设计、安全假设与实现限度。
现场的承认或拒绝由共识代码连同 RFC-020、RFC-018 定义。本文不改写那些规则。

英文装订本：[`RIGOR_PROTOCOL_WHITEPAPER.md`](./RIGOR_PROTOCOL_WHITEPAPER.md)。
中文章节：`chapters/*.zh.md`。
协议唯一事实源：[`../protocol/RFC-020.md`](../protocol/RFC-020.md)、
[`../protocol/RFC-018.md`](../protocol/RFC-018.md)。

```
equivalence_claimed = false
documentation_consensus_authority = false
```

**核心身份：**

```
双原生资产
    +
Convergence
    +
公开铸造谓词
    +
独立本地验证
    +
可见的挖矿参与
```

**架构公式：**

```
RIGOR =
    两枚原生资产（ORU, RES）
  +
    封闭货币脚本模板（v0 ORU, v1 RES, mint, converge）
  +
    UTXO 本地验证
  +
    Ed25519 认证
  +
    OpuHash（SHA3-256 / RGRPOW01）+ ASERT PoW
  +
    共识定义的 RES 发行
  +
    手续费销毁（RFC-018）
  +
    Coq 协议模型验证
  +
    无全局资产状态
```

RIGOR 协议模型中的部分共识不变量已经经过机器检查。
这不等同于证明生产环境 C++ 实现与该模型完全等价。

```
形式化验证的是协议模型
        ≠
C++ 实现等价
        ≠
正在运行的二进制正确性
```

---

## 摘要

RIGOR 探讨工作量证明系统中的一个长期设计问题：同一枚原生资产如何同时承担交易效用、长期稀缺与矿工安全激励。

单资产设计把多种经济角色指派给同一单位。这些角色可能对应不同用户、不同时间视野与不同激励偏好。结果是一种 **安全预算张力** —— 架构挑战，不是主张任何单资产设计已被证伪。

**RIGOR** 在共识资产层分离这些角色。

- **ORU** 是效用资产：手续费、矿工补贴、Convergence 入口。创世后每块平坦补贴 **128 ORU**。
- **RES** 是稀缺资产。上限是 **导出的**：`RES_MAX_SUPPLY = 2²²` RES。它不是挖矿补贴创造的。新的 RES 只出现在同笔主体交易里：销毁 ORU（`OP_CONVERGE`）并在谓词 `R · (2²⁵ + h)² ≤ B << 40` 下铸造 Witness v1 RES。
- **`destroy_cost(h)`** 是协议成本，不是市场报价。协议定义承认规则；市场决定是否使用。

**独立验证** 指陌生人不把网站或探索器的结论当作共识。公开数据在本地重算。名为 Zero Trust 的产品页是选定字段重算工具。它不是全节点的替代，也不是「整条链已被形式化验证」的主张。

本文不主张协议与二进制等价，不主张主网完成，不主张市场采用、估值或未来安全结果。

完整论证见中文章节孪生文件与英文装订本。

---

## 目录

1. [`chapters/01-single-asset-problem.zh.md`](./chapters/01-single-asset-problem.zh.md) — 为何分离两种货币职能
2. [`chapters/02-two-assets.zh.md`](./chapters/02-two-assets.zh.md) — 实现分离的规则
3. [`chapters/03-native-utxo.zh.md`](./chapters/03-native-utxo.zh.md) — 原生 UTXO 身份
4. [`chapters/04-convergence.zh.md`](./chapters/04-convergence.zh.md) — Convergence
5. [`chapters/05-mathematical-fairness.zh.md`](./chapters/05-mathematical-fairness.zh.md) — 可核对的数学
6. [`chapters/06-mining.zh.md`](./chapters/06-mining.zh.md) — 工作量证明
7. [`chapters/07-self-proving.zh.md`](./chapters/07-self-proving.zh.md) — 本地验证
8. [`chapters/08-formal-boundary.zh.md`](./chapters/08-formal-boundary.zh.md) — 形式化边界
9. [`chapters/09-product-surfaces.zh.md`](./chapters/09-product-surfaces.zh.md) — 产品表面
10. [`chapters/10-security-model.zh.md`](./chapters/10-security-model.zh.md) — 安全模型
11. [`chapters/11-trade-offs.zh.md`](./chapters/11-trade-offs.zh.md) — 设计权衡
12. [`chapters/12-evolution-policy.zh.md`](./chapters/12-evolution-policy.zh.md) — 部署与演进

附录 D：[`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md)

参考文献：[`REFERENCES.md`](./REFERENCES.md)

---

## 发布边界

本文描述 RIGOR 协议设计及其形式化验证边界。

本文不声明：

- 协议模型与生产 C++ 代码完全等价；
- 编译器正确性；
- 网络层面的绝对保证；
- 市场采用或经济结果。

共识有效性仍由节点独立验证决定。
