/*
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Copyright (c) 2025 Vincent Jardin <vjardin@free.fr>, Free Mobile
 *
 * Zephyr logging implementation for onomondo-uicc
 *
 * This implements the ss_logp function using Zephyr's logging subsystem.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdarg.h>
#include <stdio.h>

#include <onomondo/softsim/log.h>

LOG_MODULE_REGISTER(softsim_uicc, CONFIG_SOFTSIM_LOG_LEVEL);

/* Subsystem names */
static const char *subsys_str[] = {
    [SBTLV]      = "BTLV",
    [SCTLV]      = "CTLV",
    [SVPCD]      = "VPCD",
    [SIFACE]     = "IFACE",
    [SUICC]      = "UICC",
    [SCMD]       = "CMD",
    [SLCHAN]     = "LCHAN",
    [SFS]        = "FS",
    [SSTORAGE]   = "STORAGE",
    [SACCESS]    = "ACCESS",
    [SADMIN]     = "ADMIN",
    [SSFI]       = "SFI",
    [SDFNAME]    = "DFNAME",
    [SFILE]      = "FILE",
    [SPIN]       = "PIN",
    [SAUTH]      = "AUTH",
    [SPROACT]    = "PROACT",
    [STLV8]      = "TLV8",
    [SSMS]       = "SMS",
    [SREMOTECMD] = "REMOTECMD",
    [SREFRESH]   = "REFRESH",
    [SAPDU]      = "APDU",
};

void ss_logp(uint32_t subsys, uint32_t level, const char *file, int line,
             const char *format, ...)
{
    char buf[256];
    va_list ap;
    const char *subsys_name;

    /* Get subsystem name */
    if (subsys < _NUM_LOG_SUBSYS) {
        subsys_name = subsys_str[subsys];
    } else {
        subsys_name = "???";
    }

    /* Format the message */
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    /* Remove trailing newline if present (Zephyr adds its own) */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    /* Log using Zephyr logging */
    switch (level) {
    case LERROR:
        LOG_ERR("[%s] %s", subsys_name, buf);
        break;
    case LINFO:
        LOG_INF("[%s] %s", subsys_name, buf);
        break;
    case LDEBUG:
    default:
        LOG_DBG("[%s] %s", subsys_name, buf);
        break;
    }

    (void)file;
    (void)line;
}
