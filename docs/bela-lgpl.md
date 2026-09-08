# Replacing the Bela Library on Stratus

Stratus uses the Bela software, including the `libbela` library. Bela is
distributed under the [GNU Lesser General Public License, version 3.0
(LGPL-3.0)](https://www.gnu.org/licenses/lgpl-3.0.html). Its source code is
available from the [Bela repository](https://github.com/BelaPlatform/Bela).

Customers may replace the Bela library installed on their Stratus with a
modified or compatible version. Connect the Stratus to a computer with a USB
data cable, then log in over SSH with these credentials:

- Username: `bela`
- Password: `bela`

```bash
ssh bela@stratus.local
```

If `stratus.local` does not resolve, use the Stratus USB-network address
instead: `192.168.7.2` or `192.168.6.2`. For more detail about the USB network
and SSH connection, see [Test on Hardware](deploy-to-hardware.md#find-your-device).
