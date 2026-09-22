# RIGOR 协议身份 — 一页说明

**文档地位：** 一页式协议解释（不是 RFC，不是共识唯一事实源，不是市场说明）  
**日期：** 2026-09-15  
**English:** [`RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.md`](./RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.md)

```
equivalence_claimed = false
cpp_formal_equivalence = false
documentation_consensus_authority = false
```

本页用最少仍可核对的文字回答三个问题：RIGOR 为何存在、它如何实现它所定义的分离、以及主张停在何处。现行规则仍是 RFC-020 与 RFC-018。

---

## 一句话

RIGOR 将货币系统中相互冲突的流通、安全预算与稀缺职能分离：

ORU 负责交易与 PoW 安全预算，  
RES 通过公开、确定性的 Convergence 数学规则产生。

---

## 问题

单资产工作量证明链要求同一枚资产同时承担三项相互冲突的职能：

- 交换媒介
- 价值存储
- 安全预算

高流通偏向支出。  
高稀缺偏向持有。  
矿工安全需要可持续的支付。

一枚资产无法同时把这三项都推到最大。

---

## 分离

**ORU**

- 交易资产
- 手续费资产
- PoW 补贴
- 安全预算

**RES**

- 稀缺储备资产
- 不是挖出来的
- 不是 coinbase 奖励
- 只由 Convergence 产生

```
PoW
  ↓
ORU
  ↓
销毁 ORU
  ↓
Convergence 谓词
  ↓
RES
```

---

## 核心身份

RES 的全部发行判定收成一条公开整数关系：

```
destroy_cost(h)

        ↓ 精确的、保持整数的变换

R · (2²⁵ + h)² ≤ B << 40
```

这不是价格预言机。  
这不是市场预测。  
这是确定性的承认规则。  
同一组公开输入得到同一结果。

---

## 验证边界

RIGOR 不主张：

- C++ 等于形式模型
- 运行中的二进制已被形式化验证
- 经济成功有保证

主张更窄：

> 在协议模型内部，选定的共识不变量经过机器检查，并带有明确的信任边界。

---

## 设计哲学

RIGOR 保持小的共识表面：

- UTXO 状态
- 确定性谓词
- 显式发行规则
- 可公开重算

展示可以解释。  
节点做判定。  
模型可以证明性质。  
运行中的实现仍是工程。

---

## 收束

RIGOR 不是对结果的承诺。它是一套协议设计：把相互冲突的货币职能分开，并使发行判定可以公开重放。

---

## 接着读

| 文档 | 角色 |
|------|------|
| [`RIGOR_PROTOCOL_WHITEPAPER.md`](./RIGOR_PROTOCOL_WHITEPAPER.md) | 公开协议叙述 |
| [`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md) | 铸造恒等的代数 |
| [`../protocol/RFC-020.md`](../protocol/RFC-020.md) | 现行 RES 发行 |
| [`../protocol/RFC-018.md`](../protocol/RFC-018.md) | RES 转账 ORU 费销毁 |
