# Extra extended Windows desktops

DeskHalo controls an installed **VirtualDrivers Virtual Display Driver (MttVDD)** through `\\.\pipe\MTTVirtualDisplayPipe`. It sends UTF-16LE `PING` and `SETDISPLAYCOUNT N` commands, reads UTF-8 log responses, serializes requests, then asks Windows to apply Extend topology.

No compatible driver was installed on the test PC initially. The application cannot create real Windows monitors without this prerequisite. This is distinct from rendering multiple panels in OpenXR.

Upstream release: https://github.com/VirtualDrivers/Virtual-Display-Driver/releases/tag/25.7.23

Verified test package: `VirtualDisplayDriver-x86.Driver.Only.zip` (the INF targets **NTamd64**, despite the archive name).

SHA-256: `e24210692b442b39af763536330ce78b423f19342b7a7792c26de3944e418b3a`

The catalog signature was verified as valid with a SignPath Foundation signer. Install through the upstream supported installer/control application, following its instructions, or review the separate prepared local installer before running it as administrator. Do not disable Windows signature enforcement or Secure Boot for this app.

After installation, restart DeskHalo Host. Its Displays card should detect the driver. Use **+ Desktop** in Quest or the host's **+** control. Windows must be in Extend mode. Use **Change source** to assign the newly enumerated display to a VR panel.

The source repository does not contain the third-party driver binaries or redistribute its implementation. Hardware-backed testing of monitor creation requires completing installation on the PC.
