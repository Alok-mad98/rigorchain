# Chapter 1 — The Problem of Single-Asset Blockchain Economics

Public permissionless blockchains are asked to serve two economic roles that
are not the same job. When both roles are forced onto **one** native asset,
the protocol inherits a structural conflict.

---

## Medium of Exchange

A **medium of exchange** is the unit people use to move value.

Users in this role treat the network as a peer-to-peer transfer system. They
care about fee cost, confirmation, and whether the asset is easy to obtain
just in time to pay. Their exposure to the network is often momentary. Many
do not hold the native unit as a long-term reserve; they acquire it to
transact and dispose of it afterward.

For these users, the practical cost of using the chain matters more than a
scarcity narrative. The asset must **circulate**.

---

## Store of Value

A **store of value** is the unit people hold because it is scarce and the
rules are stable.

Users in this role care about long-term security, decentralization, and
monetary predictability. Transaction costs can be amortized over long holding
periods. What matters is that supply cannot be inflated by discretion, that
issuance rules do not keep changing, and that the network continues to resist
attack.

For these users, the asset must **remain scarce**. Circulation is secondary.

---

## The security-budget dilemma

Proof-of-work security is paid in the native asset. In a typical single-asset
design, early security is paid by a block subsidy. As that subsidy declines
toward zero, security is expected to be paid by fees alone.

That hand-off is fragile:

| Role | What they want | What they contribute to security |
|------|----------------|----------------------------------|
| Medium-of-exchange users | Cheap, frequent transfers | Fees — but they resist high fees |
| Store-of-value users | Scarcity and permanent protection | Few transactions, so few fees |

A dominant store-of-value culture **discourages** frequent spending. A
medium-of-exchange culture **needs** spending. One ticker cannot optimize
both.

Relying on perpetual price appreciation to enlarge a shrinking subsidy is not
a protocol rule. It is a hope. That is the classic **security-budget
dilemma** of single-asset proof-of-work systems.

---

## Why one asset creates economic conflict

A single asset is asked to be, at once:

1. the thing people spend
2. the thing people hold
3. the thing that pays for hashpower

Those incentives conflict.

- If issuance continues forever, the reserve story weakens.
- If issuance ends, the miner budget must be carried by fees that
  store-of-value holders have little reason to generate.
- If the same unit is both fee-fuel and digital reserve, holders are
  punished for using the network.

This is not a branding problem. It is an architectural one.

**RIGOR’s answer is not a market promise. It is an asset split at consensus.**
ORU takes the medium-of-exchange job, including a permanent miner tail.
RES takes the store-of-value job, with a hard cap, and is created only
through Convergence.
