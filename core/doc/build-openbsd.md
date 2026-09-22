# OpenBSD Build Guide

**Updated for OpenBSD [7.3](https://www.openbsd.org/73.html)**

This guide describes how to build rigord, command-line utilities, and GUI on OpenBSD.

## Preparation

### 1. Install Required Dependencies
Run the following as root to install the base dependencies for building.

```bash
pkg_add bash git gmake libevent libtool boost
# Select the newest version of the following packages:
pkg_add autoconf automake python
```

See [dependencies.md](./dependencies.md) for a complete overview.

### 2. Clone Rigor Repo
Clone the Rigor Core repository to a directory. All build scripts and commands will run from this directory.
``` bash
git clone https://github.com/arda/arda.git
```

### 3. Install Optional Dependencies

#### Wallet Dependencies

It is not necessary to build wallet functionality to run either `rigord` or `rigor-qt`.

###### Wallet Support

`sqlite3` is required. RIGOR wallets are SQLite descriptor wallets only; Berkeley DB is not supported.

``` bash
pkg_add sqlite3
```

#### GUI Dependencies
###### Qt5

Rigor Core includes a GUI built with the cross-platform Qt Framework. To compile the GUI, Qt 5 is required.

```bash
pkg_add qt5
```

## Building Rigor Core

**Important**: Use `gmake` (the non-GNU `make` will exit with an error).

Preparation:
```bash

# Adapt the following for the version you installed (major.minor only):
export AUTOCONF_VERSION=2.71
export AUTOMAKE_VERSION=1.16

./autogen.sh
```

### 1. Configuration

There are many ways to configure Rigor Core, here are a few common examples:

##### SQLite Wallet and GUI:
This enables the GUI and SQLite wallet support, assuming `sqlite` and `qt5` are installed.

```bash
./configure --without-bdb MAKE=gmake
```

### 2. Compile
**Important**: Use `gmake` (the non-GNU `make` will exit with an error).

```bash
gmake # use "-j N" for N parallel jobs
gmake check # Run tests if Python 3 is available
```

## Resource limits

If the build runs into out-of-memory errors, the instructions in this section
might help.

The standard ulimit restrictions in OpenBSD are very strict:
```bash
data(kbytes)         1572864
```

This is, unfortunately, in some cases not enough to compile some `.cpp` files in the project,
(see issue [#6658](https://github.com/arda/arda/issues/6658)).
If your user is in the `staff` group the limit can be raised with:
```bash
ulimit -d 3000000
```
The change will only affect the current shell and processes spawned by it. To
make the change system-wide, change `datasize-cur` and `datasize-max` in
`/etc/login.conf`, and reboot.
