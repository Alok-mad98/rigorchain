Rigor Core
=============

Setup
---------------------
Rigor Core is the original Rigor client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Rigor transactions, which requires a few hundred gigabytes of disk space. Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

To download Rigor Core, visit [bitcoincore.org](https://bitcoincore.org/en/download/).

Running
---------------------
The following are some helpful notes on how to run Rigor Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/rigor-qt` (GUI) or
- `bin/rigord` (headless)

### Windows

Unpack the files into a directory, and then run rigor-qt.exe.

### macOS

Drag Rigor Core to your applications folder, and then run Rigor Core.

### Need Help?

* See the documentation at the [Rigor Wiki](https://en.arda.it/wiki/Main_Page)
for help and more information.
* Ask for help on [Rigor StackExchange](https://arda.stackexchange.com).
* Ask for help on #arda on Libera Chat. If you don't have an IRC client, you can use [web.libera.chat](https://web.libera.chat/#arda).
* Ask for help on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [Technical Support board](https://bitcointalk.org/index.php?board=4.0).

Building
---------------------
The following are developer notes on how to build Rigor Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](./dependencies.md)
- [macOS Build Notes](./build-osx.md)
- [Unix Build Notes](./build-unix.md)
- [Windows Build Notes](./build-windows.md)
- [FreeBSD Build Notes](./build-freebsd.md)
- [OpenBSD Build Notes](./build-openbsd.md)
- [NetBSD Build Notes](./build-netbsd.md)
- [Android Build Notes](./build-android.md)

Development
---------------------
The Rigor repo's [root README](../../README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](./developer-notes.md)
- [Productivity Notes](./productivity.md)
- [Release Process](./release-process.md)
- [Source Code Documentation (External Link)](https://doxygen.bitcoincore.org/)
- [Translation Process](./translation_process.md)
- [Translation Strings Policy](./translation_strings_policy.md)
- [JSON-RPC Interface](./JSON-RPC-interface.md)
- [Unauthenticated REST Interface](./REST-interface.md)
- [Shared Libraries](./shared-libraries.md)
- [BIPS](./bips.md)
- [Dnsseed Policy](./dnsseed-policy.md)
- [Benchmarking](./benchmarking.md)
- [Internal Design Docs](./design)
- [convergence v3.0 Consensus Lock (CONSENSUS_LOCK_FORGE_V3)](./convergence-consensus-v1.md)
- [Middle-earth consensus vocabulary (OpuHash, ConvergenceDrawCandidates, Reconvergence)](../../docs/README.md#terminology-middle-earth-naming)
- [Protocol design — ORU/RES witness layout and RES mint vs transfer](./protocol_design.md)
- `convergence integration directive — economic vs consensus layers`
- [UTXO Coin `nFlags` — format and consensus follow-up](./utxo-coin-nflags.md)

### Resources
* Discuss on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [Development & Technical Discussion board](https://bitcointalk.org/index.php?board=6.0).
* Discuss project-specific development on #arda-core-dev on Libera Chat. If you don't have an IRC client, you can use [web.libera.chat](https://web.libera.chat/#arda-core-dev).

### Miscellaneous
- [Blackhole Public Announcement Template (CN)](./blackhole-public-announcement.md)
- [Blackhole Audit Checklist (CN)](./blackhole-audit-checklist.md)
- [Genesis Lockdown Audit Attachment](./genesis-lockdown.md)
- [Branding and Terminology Lockdown](./branding-and-terminology-lockdown.md)
- [Assets Attribution](./assets-attribution.md)
- `arda.conf Configuration File`
- [CJDNS Support](./cjdns.md)
- [Files](./files.md)
- [Fuzz-testing](./fuzzing.md)
- [I2P Support](./i2p.md)
- [Init Scripts (systemd/upstart/openrc)](./init.md)
- [Managing Wallets](./managing-wallets.md)
- [Multisig Tutorial](./multisig-tutorial.md)
- [P2P bad ports definition and list](./p2p-bad-ports.md)
- [PSBT support](./psbt.md)
- [Reduce Memory](./reduce-memory.md)
- [Reduce Traffic](./reduce-traffic.md)
- [Tor Support](./tor.md)
- [Transaction Relay Policy](./policy/README.md)
- [ZMQ](./zmq.md)

License
---------------------
Distributed under the `MIT software license`.
