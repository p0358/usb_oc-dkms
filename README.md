# usb_oc-dkms

Kernel module for overclocking USB devices (controllers, mice, keyboards, etc.). Equivalent of the `hidusbf` driver on Windows.

This allows you to change their poll rates by loading this out-of-tree module, instead of having to patch and re-compile the whole kernel.

## Installation and setup

Firstly find the `VID:PID` of your USB device with `lsusb`, such as `054c:0ce6`.

### 0. Pick your bInterval value

Check the speed of your USB device with (replace with your device's `VID:PID`):

```console
$ lsusb -d 054c:0ce6 -v | grep '^Negotiated speed:'

Negotiated speed: Full Speed (12Mbps)
```

#### Low Speed (USB 1.1) (1.5 Mbps)

Extremely rare, rather forget about overclocking those too much. Note that "0" itself might not be a valid value.

| Value of bInterval | Polling Period | Frequency |
| ------------------ | -------------- | --------- |
| 0 to 15            | 8 ms           | 125 Hz    |
| 16 to 35           | 16 ms          | 62.5 Hz   |
| 36 to 255          | 32 ms          | 31.25 Hz  |

#### Full Speed (USB 1.1) (12 Mbps)

This is most of the devices.

| Value of bInterval | Polling Period | Frequency |
| ------------------ | -------------- | --------- |
| 1                  | 1 ms           | 1000 Hz   |
| 2 (*to 3*)         | 2 ms           | 500 Hz    |
| 4 (*to 7*)         | 4 ms           | 250 Hz    |
| 8 (*to 15*)        | 8 ms           | 125 Hz    |
| 16 (*to 31*)       | 16 ms          | 62.5 Hz   |
| 32 (*to 255*)      | 32 ms          | 31.25 Hz  |

#### High Speed (USB 2.0) (480 Mbps)

Still has a maximum of 1000 Hz, but the values of `bInterval` differ (`Period = 2^(bInterval - 1)`).

| Value of bInterval | Polling Period | Frequency |
| ------------------ | -------------- | --------- |
| 1                  | 1 ms           | 1000 Hz   |
| 2                  | 2 ms           | 500 Hz    |
| 3                  | 4 ms           | 250 Hz    |
| 4                  | 8 ms           | 125 Hz    |
| 5                  | 16 ms          | 62.5 Hz   |
| 6                  | 32 ms          | 31.25 Hz  |
| 7 to 255           | 32 ms          | 31.25 Hz  |

**OR** maybe it's actually the table below. The formula seems to be `Period = 2^(bInterval - 1) * 0.125`.

| Value of bInterval | Polling Period | Frequency |
| ------------------ | -------------- | --------- |
| 1                  | 0.125 ms       | 8000 Hz   |
| 2                  | 0.250 ms       | 4000 Hz   |
| 3                  | 0.500 ms       | 2000 Hz   |
| 4                  | 1 ms           | 1000 Hz   |
| 5                  | 2 ms           | 500 Hz    |
| 6                  | 4 ms           | 250 Hz    |

> [!IMPORTANT]  
> **This needs further research and testing.** Please submit overclock results after step 4 and what bInterval values corresponded to which polling.
> If in doubt, *underclock* the device to a bInterval value like 6 and then test the polling.

### 1. Install

You can go to [Releases](https://github.com/p0358/usb_oc-dkms/releases) section and download the latest `.deb`, `.rpm` or `.pkg.tar.zst`.

#### Debian/Ubuntu/Mint/Pop_OS (.deb)

```console
# curl -Lo /tmp/usb-oc-dkms.deb https://github.com/p0358/usb_oc-dkms/releases/download/v1.0/usb-oc-dkms_1.0_amd64.deb
# apt install /tmp/usb-oc-dkms.deb
```

#### Arch Linux/CachyOS/EndevourOS/Manjaro (.pkg.tar.zst)

AUR: [usb_oc-dkms](https://aur.archlinux.org/packages/usb_oc-dkms) or:

```console
# curl -Lo /tmp/usb_oc-dkms.pkg.tar.zst https://github.com/p0358/usb_oc-dkms/releases/download/v1.0/usb_oc-dkms-1.0-1-any.pkg.tar.zst
# pacman -U /tmp/usb_oc-dkms.pkg.tar.zst
```

#### Fedora (.rpm)

```console
# curl -Lo /tmp/usb_oc-dkms.rpm https://github.com/p0358/usb_oc-dkms/releases/download/v1.0/usb_oc-dkms-1.0-1.fc45.noarch.rpm
# dnf install /tmp/usb_oc-dkms.rpm
```

#### openSUSE (.rpm)

```console
# curl -Lo /tmp/usb_oc-dkms.rpm https://github.com/p0358/usb_oc-dkms/releases/download/v1.0/usb_oc-dkms-1.0-1.fc45.noarch.rpm
# zypper install --allow-unsigned-rpm /tmp/usb_oc-dkms.rpm
```

#### Nobara

Use this instead: https://github.com/GloriousEggroll/Linux-Pollrate-Patch

#### Yet another distro

I guess you're on your own.

### 2. Test it out

Load the kernel module and confirm that it's been loaded:

```console
# modprobe usb_oc
# lsmod | grep usb_oc
usb_oc                 16384  0
```

You can change the module configuration on-the-fly without unloading the module. The changes will be applied immediately. Assuming you want, for the device with `VID:PID` of `054c:0ce6`, to set bInterval to `1`, run:

```console
# echo -n '054c:0ce6:1' > /sys/module/usb_oc/parameters/interrupt_interval_override
```

If you want to overclock multiple devices, you'd do:

```console
# echo -n '054c:0ce6:1,1234:5678:1' > /sys/module/usb_oc/parameters/interrupt_interval_override
```

Do monitor the output of `dmesg` command to diagnose the module and see any warnings.

See the FAQ section for how to benchmark your device to confirm the actual overclocked rate is working. Some devices might just be impossible to overclock and will not send more data than they were designed for, when polled more frequently. Some will, for which this module is made. And some will already be set to their maximum polling rate of given USB version.

Once you're done with testing, you may unload the module with `rmmod usb_oc`. Reconnect any USB devices if you want to restore their bIntervals to default.

### 3. Configure the module to auto-load on system startup

If you found a working configuration, you will probably want to persist it, so that it always overclocks your device when you boot up your computer.

Firstly, set the module to load on system startup:

```console
# echo 'usb_oc' > /etc/modules-load.d/usb_oc.conf
```

Secondly, set the module configuration (again, assuming you want, for the device with `VID:PID` of `054c:0ce6`, to set bInterval to `1`):

```console
# echo 'options usb_oc interrupt_interval_override=054c:0ce6:1' > /etc/modules-load.d/usb_oc.conf
```

After rebooting the machine, verify the module was loaded and working with:

```console
# dmesg | grep usb_oc
```

### 4. Success

Share you overclock success stories at [#1](https://github.com/p0358/usb_oc-dkms/discussions/1) and failure stories at [#2](https://github.com/p0358/usb_oc-dkms/discussions/2)!

## Notes

### Secure Boot

If you have Secure Boot enabled (check with `mokutil --sb-state`) and you get `modprobe: ERROR: could not insert 'usb_oc': Key was rejected by service` when trying to modprobe, you need to enroll DKMS's MOK key to your machine in order for the module to be possible to load.

Just run:

```bash
mokutil --timeout -1
mokutil --import /var/lib/dkms/mok.pub
```

You'll be prompted to create a password. Enter it twice.

Reboot the computer. At boot you'll see the MOK Manager EFI interface. Press any key to enter it.

- "Enroll MOK"
- "Continue"
- "Yes"
- Enter the password you set up just now.
- Select "OK" and the computer will reboot again.

Now the module should be possible to load.

### USB 1.1 and bandwidth limitations

If your device is USB 1.1 ("Full Speed" 12 Mbps, or, heavens forbid, "Low Speed" 1.5 Mbps) and you overclock it too high (1000 Hz), you may run into bandwidth issues and sometimes experience some lagged input in games. At least I think that's why it happens and goes away with higher bInterval.

I did the maths and with 12 Mbps and 1000 Hz, it should be roughly ~1572 bytes per poll though, which doesn't seem that little. I don't know how chatty XInput protocol is. Perhaps it's just the particular device that can't keep up. In any case, at some point you might have diminishing results with overclocking the device *too much*.

### Little quirk: the device is reset after the bInterval descriptor change is made

This is so that the change is picked up. But be careful with this, and not only for this reason, but in general:

> [!WARNING]  
> Unload this module if you're trying to do something "sensitive"/"risky" with the overclocked device, such as trying to flash its firmware. I am not responsible for any problems if you do dumb things, please use common sense here.

## FAQ

### Why does Fedora RPM package use DKMS instead of AKMODS?

Because I couldn't figure out the kmod counterpart and didn't care enough to spend more time on it, if the DKMS one works (and then it works on more distros like openSUSE too). But if you care, then contributions are welcome to add it, including the CI workflow to build the appropriate RPM files (as far as I understand, we'd want to publish SRPM in Releases, but still would want to at least check the binary modules are indeed building from those for current kernel...).

### How to benchmark the device to confirm it was overclocked?

For controllers you can use **[Gamepadla Polling](https://github.com/cakama3a/Polling)** ([gamepadla-polling<sup>AUR</sup>](https://aur.archlinux.org/packages/gamepadla-polling)) and for mouse you can use **[evhz](https://git.sr.ht/~iank/evhz)** ([evhz-git<sup>AUR</sup>](https://aur.archlinux.org/packages/evhz-git)).

### Why is USB overclocking not part of upstream kernel?

What do I know? A you can see in "Alternatives" section below, some means of overclocking some devices exist, and yet the patchset to overclock any device didn't seem to get accepted. And some distros outside of Nobara seem to be [allergic](https://github.com/CachyOS/linux-cachyos/issues/240#issuecomment-2430116447) to it for some reason too. But who cares, this module allows you to overclock your devices anyway, without care about what your distro thinks about it or recompiling the whole kernel!

## Alternatives

The `usbhid` driver has options for [`mousepoll`](https://wiki.archlinux.org/title/Mouse_polling_rate#Set_polling_interval), but it [reportedly doesn't work with USB 3 devices](https://wiki.archlinux.org/title/Mouse_polling_rate#Polling_rate_not_changing). The option `kbpoll` also exists for keyboards, but likely suffers from the same problem. It also has an option `jspoll` for joysticks, but it doesn't work with gamepads using other drivers like `xpad`.

[Linux-Kernel_MiSTer] has a downstream patch for [`xpad` driver adding `cpoll` option](https://github.com/MiSTer-devel/Linux-Kernel_MiSTer/blob/master/drivers/input/joystick/xpad.c). Unfortunately it is not upstreamed.

Nobara comes with [this patch](https://github.com/GloriousEggroll/Linux-Pollrate-Patch) applied to its kernel.

Some devices have settings apps that allow changing on-device configuration used by the device's firmware and its processor. Sometimes this configuration include the poll rate that will be advertised by the device, without needing a kernel module for it (for example Solaar for select Logitech mice).

## Thanks to

Some inspiration (parts of code/scripts) taken from:

- https://github.com/hannesmann/gcadapter-oc-kmod (skeleton for a simple DKMS USB overclock module)
- https://github.com/GloriousEggroll/Linux-Pollrate-Patch (kernel patchset with a configurable USB overclocking parameter)
- https://github.com/strongtz/i915-sriov-dkms (CI and Makefiles)
- https://github.com/KyleGospo/openrgb-dkms (Fedora dkms rpm spec file)

## Dev notes

For updating, remember to update the version in:

- `dkms.conf`
- `src/usb_oc.c`
- `packaging/arch/PKGBUILD`
- `packaging/rpm/usb_oc-dkms.spec`
- (in this README in the Install section in package URLs)
