/* pdp11_gp.c: GP11 General-Purpose I/O

   Copyright (c) 2021 Steven Stallion <sstallion@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the names of the authors shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the authors.

   gp           general-purpose I/O

   12-Oct-21    SES     Initial implementation

   The general-purpose I/O (GP) device bridges one or more MCP23016-based I/O
   expanders to connect SIMH to the outside world. The programming interface
   matches that of the MCP23016 with the addition of a CSR to manage interrupts.
   Up to 8 units may be attached with the unit number corresponding to the
   position of the I/O expander on the I2C bus.

   As this is not a simulated device, state cannot be saved and the file
   associated with the unit is ignored, which by convention is /dev/null.
*/

#include <mcp23016.h>

#include "pdp11_defs.h"

#define GP_DELAY        32000
#define GP_NUMDEV       8

/* GP registers */

#define GP_PORT         000                             /* data port register */
#define GP_OLAT         002                             /* output latch register */
#define GP_IPOL         004                             /* input polarity register */
#define GP_IODIR        006                             /* I/O direction register */
#define GP_INTCAP       010                             /* interrupt capture register */
#define GP_IOCON        012                             /* I/O control register */
#define GP_CSR          014                             /* control/status register */
#define GP_RSV          016                             /* reserved */

#define IOCON_V_IARES0  0                               /* interrupt activity resolution 0 */
#define IOCON_V_IARES1  8                               /* interrupt activity resolution 1 */
#define IOCON_IARES0    (1u << IOCON_V_IARES0)
#define IOCON_IARES1    (1u << IOCON_V_IARES1)

BITFIELD gp_iocon_bits[] = {
    BIT (IARES0),
    BITNCF (7),
    BIT (IARES1),
    BITNCF (7),
    ENDBITS
};

BITFIELD gp_csr_bits[] = {
    BITNCF (6),
    BIT (IE),
    BITNCF (8),
    BIT (ERR),
    ENDBITS
};

uint16 gp_port[GP_NUMDEV];                              /* data port register */
uint16 gp_olat[GP_NUMDEV];                              /* output latch register */
uint16 gp_ipol[GP_NUMDEV];                              /* input polarity register */
uint16 gp_iodir[GP_NUMDEV];                             /* I/O direction register */
uint16 gp_intcap[GP_NUMDEV];                            /* interrupt capture register */
uint16 gp_iocon[GP_NUMDEV];                             /* I/O control register */
uint16 gp_csr[GP_NUMDEV];                               /* control/status register */
int32 gp_ie;                                            /* global interrupt enable mask */

void gp_int_enable (UNIT *uptr);
void gp_int_disable (UNIT *uptr);
void gp_int_update (UNIT *uptr);
t_stat gp_rd (int32 *data, int32 addr, int32 access);
t_stat gp_wr (int32 data, int32 addr, int32 access);
t_stat gp_svc (UNIT *uptr);
t_stat gp_reset (DEVICE *dptr);
t_stat gp_attach (UNIT *uptr, CONST char *ptr);
t_stat gp_detach (UNIT *uptr);
t_stat gp_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *gp_description (DEVICE *dptr);

/* GP data structures

   gp_dev       GP device descriptor
   gp_unit      GP unit descriptor
   gp_reg       GP register list
   gp_mod       GP modifier list
*/

#define IOLN_GP         020

DIB gp_dib = {
    IOBA_AUTO, IOLN_GP * GP_NUMDEV, &gp_rd, &gp_wr,
    1, IVCL (GP), VEC_AUTO, { NULL }, IOLN_GP
    };

UNIT gp_unit[] = {
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=0 A1=0 A0=0 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=0 A1=0 A0=1 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=0 A1=1 A0=0 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=0 A1=1 A0=1 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=1 A1=0 A0=0 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=1 A1=0 A0=1 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=1 A1=1 A0=0 */
    { UDATA (NULL, UNIT_ATTABLE, 0) },                  /* A2=1 A1=1 A0=1 */
    };

UNIT gp_unit_poll = { UDATA (&gp_svc, 0, 0) };

extern DEVICE gp_dev;
REG gp_reg[] = {
    { BRDATAD  (PORT,     gp_port, DEV_RDX, 16, GP_NUMDEV, "data port register") },
    { BRDATAD  (OLAT,     gp_olat, DEV_RDX, 16, GP_NUMDEV, "output latch register") },
    { BRDATAD  (IPOL,     gp_ipol, DEV_RDX, 16, GP_NUMDEV, "input polarity register") },
    { BRDATAD  (IODIR,   gp_iodir, DEV_RDX, 16, GP_NUMDEV, "I/O direction register") },
    { BRDATAD  (INTCAP, gp_intcap, DEV_RDX, 16, GP_NUMDEV, "interrupt capture register") },
    { BRDATADF (IOCON,   gp_iocon, DEV_RDX, 16, GP_NUMDEV, "I/O control register", gp_iocon_bits) },
    { BRDATADF (CSR,       gp_csr, DEV_RDX, 16, GP_NUMDEV, "control/status register", gp_csr_bits) },
    { FLDATAD  (INT,    IREQ (GP), INT_V_GP,               "interrupt pending flag") },
    { NULL }
    };

MTAB gp_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
      NULL, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL },
    { 0 }
    };

DEVICE gp_dev = {
    "GP", gp_unit, gp_reg, gp_mod,
    GP_NUMDEV, 10, 16, 1, DEV_RDX, 16,
    NULL, NULL, &gp_reset,
    NULL, &gp_attach, &gp_detach,
    &gp_dib, DEV_DONTAUTO | DEV_NOSAVE | DEV_UBUS | DEV_QBUS, 0,
    NULL, NULL, NULL, &gp_help, NULL, NULL,
    &gp_description
    };

/* General-Purpose I/O interrupt routines */

void gp_int_enable (UNIT *uptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
int32 unitno = (int32)(uptr - dptr->units);

gp_ie |= (1u << unitno);
}

void gp_int_disable (UNIT *uptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
int32 unitno = (int32)(uptr - dptr->units);

gp_ie &= ~(1u << unitno);
if (gp_ie == 0)
    CLR_INT (GP);
}

void gp_int_update (UNIT *uptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
int32 unitno = (int32)(uptr - dptr->units);

if (gp_csr[unitno] & CSR_IE)
    gp_int_enable (uptr);
else
    gp_int_disable (uptr);
}

/* General-Purpose I/O address routines */

t_stat gp_rd (int32 *data, int32 addr, int32 access)
{
int32 unitno = (int32)((addr - gp_dib.ba) >> 4);
UNIT *uptr = &gp_unit[unitno];

if (!(uptr->flags & UNIT_ATT))
    return SCPE_NOTATT;

switch (addr & 017) {
    case GP_PORT:
        if (mcp23016_get_port (uptr->up7, &gp_port[unitno]) < 0)
            break;
        *data = gp_port[unitno];
        return SCPE_OK;

    case GP_OLAT:
        if (mcp23016_get_output (uptr->up7, &gp_olat[unitno]) < 0)
            break;
        *data = gp_olat[unitno];
        return SCPE_OK;

    case GP_IPOL:
        if (mcp23016_get_polarity (uptr->up7, &gp_ipol[unitno]) < 0)
            break;
        *data = gp_ipol[unitno];
        return SCPE_OK;

    case GP_IODIR:
        if (mcp23016_get_direction (uptr->up7, &gp_iodir[unitno]) < 0)
            break;
        *data = gp_iodir[unitno];
        return SCPE_OK;

    case GP_INTCAP:
        if (mcp23016_get_interrupt (uptr->up7, &gp_intcap[unitno]) < 0)
            break;
        *data = gp_intcap[unitno];
        return SCPE_OK;

    case GP_IOCON:
        if (mcp23016_get_control (uptr->up7, &gp_iocon[unitno]) < 0)
            break;
        *data = gp_iocon[unitno];
        return SCPE_OK;

    case GP_CSR:
        *data = gp_csr[unitno];
        return SCPE_OK;
}
gp_csr[unitno] |= CSR_ERR;
return SCPE_IOERR;
}

t_stat gp_wr (int32 data, int32 addr, int32 access)
{
int32 unitno = (int32)((addr - gp_dib.ba) >> 4);
UNIT *uptr = &gp_unit[unitno];

if (!(uptr->flags & UNIT_ATT))
    return SCPE_NOTATT;

switch (addr & 017) {
    case GP_PORT:
        gp_port[unitno] = data;
        if (mcp23016_set_port (uptr->up7, gp_port[unitno]) < 0)
            break;
        return SCPE_OK;

    case GP_OLAT:
        gp_olat[unitno] = data;
        if (mcp23016_set_output (uptr->up7, gp_olat[unitno]) < 0)
            break;
        return SCPE_OK;

    case GP_IPOL:
        gp_ipol[unitno] = data;
        if (mcp23016_set_polarity (uptr->up7, gp_ipol[unitno]) < 0)
            break;
        return SCPE_OK;

    case GP_IODIR:
        gp_iodir[unitno] = data;
        if (mcp23016_set_direction (uptr->up7, gp_iodir[unitno]) < 0)
            break;
        return SCPE_OK;

    case GP_INTCAP:
        return SCPE_RO;

    case GP_IOCON:
        gp_iocon[unitno] = data;
        if (mcp23016_set_control (uptr->up7, gp_iocon[unitno]) < 0)
            break;
        return SCPE_OK;

    case GP_CSR:
        gp_csr[unitno] = data;
        gp_int_update (uptr);
        return SCPE_OK;
}
gp_csr[unitno] |= CSR_ERR;
return SCPE_IOERR;
}

/* General-Purpose I/O service */

t_stat gp_svc (UNIT *uptr)
{
if (gp_ie) {
    switch (mcp23016_has_interrupt (uptr->up7)) {
        case 0: CLR_INT (GP); break;
        case 1: SET_INT (GP); break;
        default:
            return SCPE_IOERR;
    }
}
sim_activate_after (uptr, GP_DELAY);
return SCPE_OK;
}

/* General-Purpose I/O support routines */

t_stat gp_reset (DEVICE *dptr)
{
int32 i, unitno;
UNIT *uptr;

for (i = 0; i < dptr->numunits; i++) {
    uptr = &dptr->units[i];
    unitno = (int32)(uptr - dptr->units);
    gp_port[unitno] = 0;
    gp_olat[unitno] = 0;
    gp_ipol[unitno] = 0;
    gp_iodir[unitno] = 0;
    gp_intcap[unitno] = 0;
    gp_iocon[unitno] = 0;
    gp_csr[unitno] = 0;
    gp_int_disable (uptr);
    if (uptr->flags & UNIT_ATT)
    if (mcp23016_reset (uptr->up7) < 0) {
        gp_csr[unitno] |= CSR_ERR;
        return SCPE_IOERR;
    }
}
return auto_config (dptr->name, 1);
}

t_stat gp_attach (UNIT *uptr, CONST char *cptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
int32 unitno = (int32)(uptr - dptr->units);
t_stat reason;

/* Managing interrupts is less than ideal; GP11 assumes that all units share
   the same interrupt output, which requires an independent unit for proper
   scheduling. This unit is activated when the first unit is attached and is
   hidden from the user.
*/
if (gp_unit_poll.up7 == NULL) {
    gp_unit_poll.up7 = mcp23016_interrupt_open ("/dev/gpiochip0", 19);
    if (gp_unit_poll.up7 == NULL)
        return SCPE_OPENERR;
    sim_activate_after (&gp_unit_poll, GP_DELAY);
}

uptr->up7 = mcp23016_open ("/dev/i2c-1", unitno);
if (uptr->up7 == NULL)
    return SCPE_OPENERR;

reason = attach_unit (uptr, cptr);
if (uptr->flags & UNIT_ATT)
    gp_int_update (uptr);
else
    mcp23016_close (uptr->up7);
return reason;
}

t_stat gp_detach (UNIT *uptr)
{
if (!(uptr->flags & UNIT_ATT))
    return SCPE_NOTATT;

gp_int_disable (uptr);
mcp23016_close (uptr->up7);
return detach_unit (uptr);
}

t_stat gp_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "GP11 General-Purpose I/O (GP)\n\n");
fprintf (st, "The general-purpose I/O (GP) device bridges one or more MCP23016-based I/O\n");
fprintf (st, "expanders to connect SIMH to the outside world. The programming interface\n");
fprintf (st, "matches that of the MCP23016 with the addition of a CSR to manage interrupts.\n");
fprintf (st, "Up to 8 units may be attached with the unit number corresponding to the\n");
fprintf (st, "position of the I/O expander on the I2C bus.\n\n");
fprintf (st, "As this is not a simulated device, state cannot be saved and the file\n");
fprintf (st, "associated with the unit is ignored, which by convention is /dev/null.\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
return SCPE_OK;
}

const char *gp_description (DEVICE *dptr)
{
return "GP11 General-Purpose I/O";
}
