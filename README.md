# PiDP-11 I/O Expander Software

This repository contains the latest revision of the [PiDP-11][1] software with
updates to support the [PiDP-11 I/O Expander][2] in [SIMH][3]. Releases from
this repository are intended to fully replace the original PiDP-11 software. A
complete list of changes can be found [here][4].

Project documentation is hosted on [Hackaday][2] and assembled boards are
available for purchase from [Tindie][5].

## Installation

> **Note**: These instructions assume you have a working hardware installation.
> Be sure to follow the steps documented on [Hackaday][6] and verify that the
> I/O expander is responding on the I2C bus before continuing.

Installing releases from this repository follows the same steps as installing
the official PiDP-11 software:

    $ sudo mkdir /opt/pidp11
    $ cd /opt/pidp11
    $ sudo wget https://github.com/sstallion/pidp11/releases/latest/download/pidp11.tar.gz
    $ sudo tar -xvf pidp11.tar.gz
    $ sudo /opt/pidp11/install/install.sh

Once installed, reboot the system and the front panel should light up.

## Theory of Operation

The general-purpose I/O (GP) device bridges one or more MCP23016-based I/O
expanders to connect SIMH to the outside world. The programming interface
matches that of the MCP23016 with the addition of a CSR to manage interrupts. Up
to 8 units may be attached with the unit number corresponding to the position of
the I/O expander on the I2C bus.

As this is not a simulated device, state cannot be saved and the file associated
with the unit is ignored, which by convention is `/dev/null`.

GP11 units are mapped to a fixed set of addresses with a single auto-configured
interrupt vector:

| UNIT | BASE ADDRESS |
|:----:|:------------:|
| GP0  |   17776000   |
| GP1  |   17776020   |
| GP2  |   17776040   |
| GP3  |   17776060   |
| GP4  |   17776100   |
| GP5  |   17776120   |
| GP6  |   17776140   |
| GP7  |   17776160   |

GP11 registers are mapped to the following offsets:

| REGISTER | OFFSET |
|:--------:|:------:|
| PORT     |   00   |
| OLAT     |   02   |
| IPOL     |   04   |
| IODIR    |   06   |
| INTCAP   |   10   |
| IOCON    |   12   |
| CSR      |   14   |

For example, to examine the contents of the `GP0` `CSR` register from the SIMH
console, issue:

    sim> examine 17776014

> **Note**: The `examine GP0 CSR` command can also be used, but be aware that
> the values returned by this command are cached and do not represent the true
> state of the underlying hardware.

With the exception of the `CSR` register, each register is documented in the
MCP23016 [datasheet][7]. The `CSR` register contains two bits of interest: `IE`
(bit 6) and `ERR` (bit 15). The `IE` bit controls whether or not changes to
input state will interrupt the system, which must be set by software and can be
cleared by either software or hardware. The `ERR` bit is used to report
low-level errors communicating with the MCP23016. It is set by hardware and must
be cleared by software.

## Usage

An I/O expander using the default I2C address can be attached from the SIMH
console by issuing:

    sim> attach GP0 /dev/null

> **Note**: GP0 is attached by default when booting IDLED to simplify GPIO
> programming using the front panel switches.

Once attached, registers can be examined and deposited using the `EXAMINE` and
`DEPOSIT` commands in SIMH or by using the front panel switches.

To verify that the I/O expander is attached and functioning properly, the
`IODIR` and `CSR` registers may be examined using the front panel switches:

1. Toggle the `HALT` switch to stop execution.
2. Load address `17776006` using the `S0-21` switches followed by the `LOAD
   ADRS` switch.
3. Press the `EXAM` switch to display the contents of the `IODIR` register in
   the data display; bits 0-15 should be lit indicating that all ports are
   configured as inputs (POR default).
4. Load address `17776014` using the `S0-21` switches followed by the `LOAD
   ADRS` switch.
5. Press the `EXAM` switch to display the content of the `CSR` register in the
   data display; bits 0-15 should be unlit indicating that no error occurred and
   interrupts are disabled.
6. Toggle the `ENABLE` switch followed by `CONT` to resume execution.

The above procedure can also be accomplished using the SIMH console:

    ^E
    sim> examine 17776006
    17776006:       177777
    sim> examine 17776014
    17776014:       000000
    sim> continue

... but where was the fun in that?

## Contributing

Pull requests are welcome! If a problem is encountered using this repository,
please file an issue on [GitHub][8].

## License

Modifications to SIMH are licensed under the same modified X-Windows license.
See see the documentation included with each software package for more details.

[1]: https://obsolescence.wixsite.com/obsolescence/pidp-11
[2]: https://hackaday.io/project/181311
[3]: https://github.com/simh/simh
[4]: https://github.com/sstallion/pidp11/compare/tracking...master
[5]: https://www.tindie.com/products/24781
[6]: https://hackaday.io/project/181311/instructions
[7]: https://ww1.microchip.com/downloads/en/DeviceDoc/20090C.pdf
[8]: https://github.com/sstallion/pidp11/issues
