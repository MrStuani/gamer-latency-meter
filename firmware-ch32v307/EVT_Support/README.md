# CH32V307 EVT package (WCH)

These files are **Nanjing WCH Microelectronics Co., Ltd**'s property. They are
part of this repository **unmodified**, copied from the official CH32V307
evaluation-kit archive so that `firmware-ch32v307/` compiles out of the box.

They are **not** covered by this project's MIT license — see
[`NOTICE`](../../NOTICE) at the repository root.

| | |
|---|---|
| Upstream | <https://github.com/openwch/ch32v307> |
| Official archive | `CH32V307EVT.ZIP` (evaluation board description + reference application routines), linked from <https://www.wch-ic.com/products/CH32V307.html> |
| License | none stated by WCH; the upstream repository ships no `LICENSE` file |

## Replacing with your own copy

Delete this directory (plus `../USB_Host/` and `../USB_Device/`), extract the
official `CH32V307EVT.ZIP`, and put them back at the same paths.

Only these paths are actually used by the build (`Makefile`, `INCLUDES` /
`LDSCRIPT`):

    EVT_Support/Core
    EVT_Support/Debug
    EVT_Support/Peripheral/inc
    EVT_Support/Ld/Link.ld
    USB_Host/

`USB_Device/` is present in the repository but is not compiled by this
firmware — it is kept with the rest of the untouched package.
