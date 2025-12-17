# zephyr-softsim

[![CI](https://github.com/vjardin/zephyr-softsim/actions/workflows/ci.yml/badge.svg)](https://github.com/vjardin/zephyr-softsim/actions/workflows/ci.yml)

Zephyr RTOS module providing software SIM (Soft SIM / UICC / USIM) support
using the [onomondo-uicc](https://github.com/onomondo/onomondo-uicc) library.

## Overview

This module enables cellular modems with external SIM interfaces (such as
Nordic nRF91 series) to use a software-based SIM implementation instead of
a physical SIM card. It provides:

- NVS-based persistent storage for SIM files
- Zephyr logging integration for debugging
- Kconfig-based configuration for easy customization
- West manifest for dependency management

## License

- SPDX-License-Identifier: AGPL-3.0-only
- Copyright (c) 2025 Vincent Jardin <vjardin@free.fr>, Free Mobile

## Supported Features

Via the onomondo-uicc library:

- APDU command parser / encoder
- Smart Card File System (MF, DF, EF, ADF)
- PIN management (VERIFY, CHANGE, ENABLE, DISABLE, UNBLOCK)
- USIM Authentication using MILENAGE algorithm
- CAT / Proactive SIM (for OTA)
- Over-The-Air (OTA) access

## Requirements

### Hardware

- Nordic nRF91 series (nRF9160, nRF9151) or other Zephyr platform with:
  - NVS-compatible flash storage partition
  - Modem with external SIM interface support

### Software

- Zephyr RTOS 3.x or nRF Connect SDK 2.5.0+
- West build tool
- CMake 3.20+

## Quick Start

### 1. Add to Your West Manifest

Add zephyr-softsim to your project's `west.yml`:

```yaml
manifest:
  remotes:
    - name: vjardin
      url-base: https://github.com/vjardin

  projects:
    - name: zephyr-softsim
      remote: vjardin
      revision: main
      path: modules/lib/zephyr-softsim
      import: true  # This imports onomondo-uicc dependency
```

Then update:

```bash
west update
```

### 2. Enable in Your Application

Add to your `prj.conf`:

```ini
# Enable soft SIM
CONFIG_SOFTSIM=y

# Required dependencies
CONFIG_NVS=y
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FLASH_PAGE_LAYOUT=y
CONFIG_HEAP_MEM_POOL_SIZE=16384

# Logging (optional)
CONFIG_LOG=y
CONFIG_SOFTSIM_LOG_LEVEL=3
```

### 3. Use in Your Code

```c
#include <onomondo/softsim/softsim.h>
#include <onomondo/softsim/mem.h>

static struct ss_context *sim_ctx;

int softsim_init(void)
{
    sim_ctx = ss_new_ctx();
    if (!sim_ctx) {
        return -ENOMEM;
    }
    /* Provision SIM files (IMSI, Ki, OPc, etc.) */
    return 0;
}

int softsim_transact(const uint8_t *cmd, size_t cmd_len,
                     uint8_t *rsp, size_t *rsp_len)
{
    size_t len = cmd_len;
    *rsp_len = ss_transact(sim_ctx, rsp, *rsp_len, cmd, &len);
    return 0;
}
```

## Integration Methods

### Method A: West Manifest Import (Recommended)

The simplest approach - add to your `west.yml` as shown above. West will
automatically fetch both zephyr-softsim and its onomondo-uicc dependency.

### Method B: Git Submodules

```bash
# Add as submodules
git submodule add https://github.com/vjardin/zephyr-softsim.git \
    modules/lib/zephyr-softsim
git submodule add https://github.com/onomondo/onomondo-uicc.git \
    modules/lib/onomondo-uicc
```

Then add to your root `CMakeLists.txt`:

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/lib/zephyr-softsim
)
```

### Method C: Direct Include

Copy the module into your project and include directly:

```cmake
# In your CMakeLists.txt
set(ZEPHYR_ONOMONDO_UICC_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/onomondo-uicc)
add_subdirectory(lib/zephyr-softsim)
```

## Configuration Options

### Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_SOFTSIM` | n | Enable soft SIM support |
| `CONFIG_SOFTSIM_LOG_LEVEL` | 3 | Log level (0=OFF, 1=ERR, 2=WRN, 3=INF, 4=DBG) |
| `CONFIG_SOFTSIM_STORAGE_PATH` | "/softsim" | NVS storage path prefix |
| `CONFIG_SOFTSIM_MAX_PATH_LEN` | 64 | Maximum file path length |
| `CONFIG_SOFTSIM_MAX_OPEN_FILES` | 4 | Maximum concurrent open files |
| `CONFIG_SOFTSIM_MAX_FILE_SIZE` | 1536 | Maximum single file size (bytes) |

### Required Dependencies

The following must be enabled in your `prj.conf`:

```ini
CONFIG_NVS=y
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FLASH_PAGE_LAYOUT=y
CONFIG_HEAP_MEM_POOL_SIZE=16384
```

### Flash Partition

The module requires a flash partition for NVS storage. It uses (in order):

1. `settings_storage` partition (preferred)
2. `storage_partition` partition (fallback)

Ensure your board's device tree defines one of these with at least 32KB.

## Nordic nRF91 Integration

For nRF91 series modems, integrate with the nRF Modem Library:

```c
#include <nrf_modem_softsim.h>

static uint8_t rsp_buf[512];

void nrf_modem_softsim_req_handler(enum nrf_modem_softsim_cmd cmd,
                                   uint16_t req_id,
                                   const uint8_t *data,
                                   uint16_t data_len)
{
    size_t rsp_len = sizeof(rsp_buf);

    switch (cmd) {
    case NRF_MODEM_SOFTSIM_APDU:
        softsim_transact(data, data_len, rsp_buf, &rsp_len);
        nrf_modem_softsim_res(req_id, rsp_buf, rsp_len);
        break;

    case NRF_MODEM_SOFTSIM_RESET:
        /* Return ATR */
        nrf_modem_softsim_res(req_id, atr_data, atr_len);
        break;

    case NRF_MODEM_SOFTSIM_DEINIT:
        nrf_modem_softsim_res(req_id, NULL, 0);
        break;
    }
}
```

Enable in `prj.conf`:

```ini
CONFIG_NRF_MODEM_LIB_SOFTSIM=y
```

## SIM Provisioning

SIM files must be provisioned with valid credentials:

- IMSI: International Mobile Subscriber Identity
- ICCID: Integrated Circuit Card Identifier
- Ki: Subscriber Authentication Key (128-bit)
- OPc: Operator Variant Algorithm Configuration (128-bit)

Example provisioning (see onomondo-uicc documentation for details):

```c
#include <onomondo/softsim/file.h>

/* Provision IMSI file */
static const uint8_t imsi_data[] = { /* BCD encoded IMSI */ };
ss_storage_create_file("/softsim/3f00/7fff/6f07", imsi_data, sizeof(imsi_data));
```

## Logging

The module uses two Zephyr log modules:

- `softsim_fs`: File system operations
- `softsim_uicc`: UICC library operations

### Runtime Log Control

```
uart:~$ log enable dbg softsim_fs
uart:~$ log enable dbg softsim_uicc
uart:~$ log disable softsim_uicc
```

### Log Prefixes

Internal library subsystems are prefixed in log output:

| Prefix | Subsystem |
|--------|-----------|
| `[FS]` | File system |
| `[AUTH]` | Authentication |
| `[APDU]` | APDU processing |
| `[PIN]` | PIN management |
| `[FILE]` | File operations |
| `[STORAGE]` | Storage backend |

## Project Structure

```
zephyr-softsim/
├── west.yml           # West manifest (fetches onomondo-uicc)
├── CMakeLists.txt     # Zephyr build integration
├── Kconfig            # Configuration options
├── zephyr/
│   └── module.yml     # Zephyr module definition
├── src/
│   ├── fs_zephyr.c    # NVS storage backend
│   └── log_zephyr.c   # Zephyr logging backend
└── README.md          # This file
```

## Troubleshooting

### "No suitable storage partition found"

Ensure your board's device tree defines `settings_storage` or
`storage_partition`. For Nordic boards, this is usually defined in
the board's default configuration.

### "NVS mount failed"

Check that:
- Flash partition has correct size (minimum 32KB recommended)
- Flash device is properly initialized
- No other subsystem is using the same partition

### Authentication failures

Verify:
- Ki and OPc are correctly provisioned
- IMSI matches the network's expectations
- SQN (sequence number) is properly managed

## References

- [onomondo-uicc](https://github.com/onomondo/onomondo-uicc) - Core SIM library
- [Zephyr NVS](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
- [nRF Modem Library](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrfxlib/nrf_modem/README.html)
- [3GPP TS 31.102](https://www.3gpp.org/specifications) - USIM specification
- [ETSI TS 102 221](https://www.etsi.org/standards) - UICC specification

## Contributing

Contributions are welcome. Please ensure:

1. Code follows Zephyr coding style
2. Changes are tested on real hardware
3. Documentation is updated accordingly

### Code Formatting (Optional)

A `.clang-format` file is provided for Zephyr/Linux kernel style. Usage is optional:

```bash
# Check formatting (dry-run)
clang-format --dry-run -Werror src/*.c

# Auto-fix formatting
clang-format -i src/*.c
```

## Acknowledgments

- [Onomondo](https://onomondo.com/) for the onomondo-uicc library
- [Nordic Semiconductor](https://www.nordicsemi.com/) for nRF91 platform support
