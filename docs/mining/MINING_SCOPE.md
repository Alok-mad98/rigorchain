# RIGOR Mining Scope

RIGOR is designed so that anyone can participate in Proof-of-Work mining without depending on a centralized mining service operated by the project.

The RIGOR project provides the protocol, the node-side mining interface, and a reference mining path.

The mining ecosystem remains open to independent implementations and services.

## What the RIGOR Project Provides

The RIGOR project provides and maintains the protocol-level and reference components required for independent mining, including:

- the RGRPOW01 Proof-of-Work specification;
- the RIGOR node;
- the node-side mining interface;
- block-template generation and block submission;
- the reference miner;
- SOLO mining documentation;
- the reference implementation required to independently verify and implement RGRPOW01 mining.

The purpose of these components is to allow a miner to operate against its own RIGOR node.

A normal SOLO configuration is:

```text
RIGOR Miner
    |
    | local RPC
    | 127.0.0.1:8761
    v
RIGOR Node
    |
    | P2P
    v
RIGOR Network
```

The mining RPC is a local coordination interface between a miner and its own node. It is not required to be publicly reachable.

## What the RIGOR Project Does Not Operate

The RIGOR project does not operate or require an official centralized mining service.

In particular, the project does not operate:

- an official mining pool;
- an official Stratum pool service;
- a centralized mining gateway;
- a pool account or payout system;
- a mining-reward settlement service.

RIGOR therefore does not require miners to submit work to a project-operated mining server.

## Independent Mining Ecosystem

RIGOR does not prohibit or prevent independent mining infrastructure.

Independent developers and operators may build and operate:

- mining pools;
- Stratum services;
- mining proxies;
- GPU miners;
- FPGA implementations;
- ASIC implementations;
- optimized mining software;
- other mining infrastructure.

Such software and services are independent implementations or services.

They are not part of the RIGOR protocol and are not operated or endorsed by the RIGOR project merely because they interoperate with RIGOR.

## Reference Miner and Specialized Miners

The RIGOR project maintains a reference mining path so that the PoW algorithm and the basic mining workflow have a directly inspectable implementation.

The purpose of the reference miner is correctness, interoperability, and independent verification.

The RIGOR project does not need to maintain a specialized high-performance implementation for every GPU architecture, FPGA platform, ASIC design, or future hardware generation.

Specialized performance implementations may be developed independently by the mining ecosystem.

## SOLO Mining

SOLO mining is a first-class operational path for RIGOR.

A miner can run its own RIGOR node and connect the miner to the node's local RPC interface.

No publicly reachable mining RPC endpoint operated by RIGOR is required.

This keeps the mining path independent of a centralized project-operated mining gateway.

Operational connectivity for that path is described in [SOLO Mining — Local Node Architecture](./SOLO_MINING.md).

## Protocol Boundary

Mining services and mining implementations exist above the RIGOR protocol boundary.

The following remain protocol-defined:

- RGRPOW01 Proof-of-Work identity;
- block-header and block-validation rules;
- transaction validity;
- issuance and asset rules;
- consensus validation.

A mining pool, miner, proxy, or hardware implementation does not redefine these rules.

Different mining implementations may compete on performance, infrastructure, availability, or operational model while remaining compatible with the same RIGOR protocol.

## Project Boundary

The RIGOR project is responsible for maintaining the protocol and the reference software necessary for participation.

The project does not need to become the operator of the entire mining industry surrounding the protocol.

RIGOR provides the rules and the reference path.

The broader mining ecosystem is open to independent participation.
