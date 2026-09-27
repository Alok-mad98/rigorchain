Sample init scripts and service configuration for rigord
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/bitcoind.service:    inherited systemd unit sample
    contrib/init/bitcoind.openrc:     inherited OpenRC SysV-style sample
    contrib/init/bitcoind.openrcconf: inherited OpenRC conf.d sample
    contrib/init/bitcoind.conf:       inherited Upstart sample
    contrib/init/bitcoind.init:       inherited CentOS SysV sample

These unit files still use Bitcoin Core path names (`/etc/bitcoin/bitcoin.conf`).
The RIGOR node itself reads `rigor.conf` (see `RIGOR_CONF_FILENAME`). Do not
treat the sample paths as current RIGOR package layout.

Service User
---------------------------------

The inherited Linux startup samples assume a service user/group named
`bitcoin`. They must be created before attempting to use those samples.
The macOS configuration assumes rigord will be set up for the current user.

Configuration
---------------------------------

Running rigord as a daemon does not require any manual configuration. You may
set the `rpcauth` setting in the `rigor.conf` configuration file to override
the default behaviour of using a special cookie for authentication.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that rigord and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If rigord is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but it's location can be overridden
with the option '-rpccookiefile'.

This allows for running rigord without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

For an example configuration file that describes the configuration settings,
see `share/examples/rigor.conf`.

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

    Binary:              /usr/bin/rigord
    Configuration file:  /etc/bitcoin/bitcoin.conf (inherited sample path)
    Data directory:      /var/lib/rigord
    PID file:            /var/run/rigord/rigord.pid (OpenRC and Upstart) or
                         /run/rigord/rigord.pid (systemd)
    Lock file:           /var/lock/subsys/rigord (CentOS)

The PID directory (if applicable) and data directory should both be owned by the
service user and group used by the chosen init sample. It is advised for
security reasons to make the configuration file and data directory only
readable by that user and group. Access to rigor-cli and other rigord RPC
clients can then be controlled by group membership.

NOTE: When using the inherited systemd unit, directory creation and permissions
are handled by systemd. The sample still refers to Bitcoin Core paths.

NOTE: The inherited init samples pass `-conf=/etc/bitcoin/bitcoin.conf` on the
command line. Those flags take precedence over a config file. The running
RIGOR node reads `rigor.conf` unless `-conf` is set. Do not assume the sample
unit is a finished RIGOR package.

### macOS

    Binary:              /usr/local/bin/rigord
    Configuration file:  ~/Library/Application Support/RIGOR/rigor.conf
    Data directory:      ~/Library/Application Support/RIGOR
    Lock file:           ~/Library/Application Support/RIGOR/.lock

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start rigord` and to enable for system startup run
`systemctl enable rigord`

NOTE: When installing for systemd in Debian/Ubuntu the .service file needs to be copied to the /lib/systemd/system directory instead.

### OpenRC

Rename rigord.openrc to rigord and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/rigord start` and configure it to run on startup with
`rc-update add rigord`

### Upstart (for Debian/Ubuntu based distributions)

Upstart is the default init system for Debian/Ubuntu versions older than 15.04. If you are using version 15.04 or newer and haven't manually configured upstart you should follow the systemd instructions instead.

Drop rigord.conf in /etc/init.  Test by running `service rigord start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy rigord.init to /etc/init.d/rigord. Test by running `service rigord start`.

Using this script, you can adjust the path and flags to the rigord program by
setting the BITCOIND and FLAGS environment variables in the file
/etc/sysconfig/rigord. You can also use the DAEMONOPTS environment variable here.

### macOS

Copy org.rigor.rigord.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.rigor.rigord.plist`.

This Launch Agent will cause rigord to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run rigord as the current user.
You will need to modify org.rigor.rigord.plist if you intend to use it as a
Launch the daemon with a dedicated service user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
