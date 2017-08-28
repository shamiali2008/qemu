/*
 * ARM SMMUv3 support - Internal API
 *
 * Copyright (C) 2014-2016 Broadcom Corporation
 * Copyright (c) 2017 Red Hat, Inc.
 * Written by Prem Mallappa, Eric Auger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_ARM_SMMU_V3_INTERNAL_H
#define HW_ARM_SMMU_V3_INTERNAL_H

#include "qemu/log.h"
#include "trace.h"
#include "qemu/error-report.h"
#include "hw/arm/smmu-common.h"

/* MMIO Registers */

REG32(IDR0,                0x0)
    FIELD(IDR0, S1P,         1 , 1)
    FIELD(IDR0, TTF,         2 , 2)
    FIELD(IDR0, COHACC,      4 , 1)
    FIELD(IDR0, ASID16,      12, 1)
    FIELD(IDR0, TTENDIAN,    21, 2)
    FIELD(IDR0, STALL_MODEL, 24, 2)
    FIELD(IDR0, TERM_MODEL,  26, 1)
    FIELD(IDR0, STLEVEL,     27, 2)

REG32(IDR1,                0x4)
    FIELD(IDR1, SIDSIZE,      0 , 6)
    FIELD(IDR1, EVENTQS,      16, 5)
    FIELD(IDR1, CMDQS,        21, 5)

#define SMMU_IDR1_SIDSIZE 16

REG32(IDR2,                0x8)
REG32(IDR3,                0xc)
REG32(IDR4,                0x10)
REG32(IDR5,                0x14)
     FIELD(IDR5, OAS,         0, 3);
     FIELD(IDR5, GRAN4K,      4, 1);
     FIELD(IDR5, GRAN16K,     5, 1);
     FIELD(IDR5, GRAN64K,     6, 1);

#define SMMU_IDR5_OAS 4

REG32(IIDR,                0x1c)
REG32(CR0,                 0x20)
    FIELD(CR0, SMMU_ENABLE,   0, 1)
    FIELD(CR0, EVENTQEN,      2, 1)
    FIELD(CR0, CMDQEN,        3, 1)

REG32(CR0ACK,              0x24)
REG32(CR1,                 0x28)
REG32(CR2,                 0x2c)
REG32(STATUSR,             0x40)
REG32(IRQ_CTRL,            0x50)
    FIELD(IRQ_CTRL, GERROR_IRQEN,        0, 1)
    FIELD(IRQ_CTRL, PRI_IRQEN,           1, 1)
    FIELD(IRQ_CTRL, EVENTQ_IRQEN,        2, 1)

REG32(IRQ_CTRL_ACK,        0x54)
REG32(GERROR,              0x60)
    FIELD(GERROR, CMDQ_ERR,           0, 1)
    FIELD(GERROR, EVENTQ_ABT_ERR,     2, 1)
    FIELD(GERROR, PRIQ_ABT_ERR,       3, 1)
    FIELD(GERROR, MSI_CMDQ_ABT_ERR,   4, 1)
    FIELD(GERROR, MSI_EVENTQ_ABT_ERR, 5, 1)
    FIELD(GERROR, MSI_PRIQ_ABT_ERR,   6, 1)
    FIELD(GERROR, MSI_GERROR_ABT_ERR, 7, 1)
    FIELD(GERROR, MSI_SFM_ERR,        8, 1)

REG32(GERRORN,             0x64)

#define A_GERROR_IRQ_CFG0  0x68 /* 64b */
REG32(GERROR_IRQ_CFG1, 0x70)
REG32(GERROR_IRQ_CFG2, 0x74)

#define A_STRTAB_BASE      0x80 /* 64b */

#define SMMU_BASE_ADDR_MASK 0xffffffffffe0

REG32(STRTAB_BASE_CFG,     0x88)
    FIELD(STRTAB_BASE_CFG, FMT,      16, 2)
    FIELD(STRTAB_BASE_CFG, SPLIT,    6 , 5)
    FIELD(STRTAB_BASE_CFG, LOG2SIZE, 0 , 6)

#define A_CMDQ_BASE        0x90 /* 64b */
REG32(CMDQ_PROD,           0x98)
REG32(CMDQ_CONS,           0x9c)
    FIELD(CMDQ_CONS, ERR, 24, 7)

#define A_EVENTQ_BASE      0xa0 /* 64b */
REG32(EVENTQ_PROD,         0xa8)
REG32(EVENTQ_CONS,         0xac)

#define A_EVENTQ_IRQ_CFG0  0xb0 /* 64b */
REG32(EVENTQ_IRQ_CFG1,     0xb8)
REG32(EVENTQ_IRQ_CFG2,     0xbc)

REG32(CIDR0,               0xff0)
REG32(CIDR1,               0xff4)
REG32(CIDR2,               0xff8)
REG32(CIDR3,               0xffc)
REG32(PIDR0,               0xfe0)
REG32(PIDR1,               0xfe4)
REG32(PIDR2,               0xfe8)
REG32(PIDR3,               0xfec)
REG32(PIDR4,               0xfd0)

static inline int smmu_enabled(SMMUv3State *s)
{
    return FIELD_EX32(s->cr[0], CR0, SMMU_ENABLE);
}

typedef struct Cmd {
    uint32_t word[4];
} Cmd;

typedef struct Evt  {
    uint32_t word[8];
} Evt;

static inline uint64_t smmu_read64(uint64_t r, unsigned offset,
                                   unsigned size)
{
    if (size == 8 && !offset) {
        return r;
    }

    /* 32 bit access */

    if (offset && offset != 4)  {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "SMMUv3 MMIO read: bad offset/size %u/%u\n",
                      offset, size);
        return 0;
    }

    return extract64(r, offset << 3, 32);
}

/* Interrupts */

#define smmuv3_eventq_irq_enabled(s)                   \
    (FIELD_EX32(s->irq_ctrl, IRQ_CTRL, EVENTQ_IRQEN))
#define smmuv3_gerror_irq_enabled(s)                  \
    (FIELD_EX32(s->irq_ctrl, IRQ_CTRL, GERROR_IRQEN))

void smmuv3_trigger_irq(SMMUv3State *s, SMMUIrq irq, uint32_t gerror_mask);
void smmuv3_write_gerrorn(SMMUv3State *s, uint32_t gerrorn);

/* Queue Handling */

#define LOG2SIZE(q)        extract64((q)->base, 0, 5)
#define BASE(q)            ((q)->base & SMMU_BASE_ADDR_MASK)
#define WRAP_MASK(q)       (1 << LOG2SIZE(q))
#define INDEX_MASK(q)      ((1 << LOG2SIZE(q)) - 1)
#define WRAP_INDEX_MASK(q) ((1 << (LOG2SIZE(q) + 1)) - 1)

#define Q_CONS_ENTRY(q)  (BASE(q) + \
                          (q)->entry_size * ((q)->cons & INDEX_MASK(q)))
#define Q_PROD_ENTRY(q)  (BASE(q) + \
                          (q)->entry_size * ((q)->prod & INDEX_MASK(q)))

#define Q_CONS(q) ((q)->cons & INDEX_MASK(q))
#define Q_PROD(q) ((q)->prod & INDEX_MASK(q))

#define Q_CONS_WRAP(q) (((q)->cons & WRAP_MASK(q)) >> LOG2SIZE(q))
#define Q_PROD_WRAP(q) (((q)->prod & WRAP_MASK(q)) >> LOG2SIZE(q))

#define Q_FULL(q) \
    (((((q)->cons) & INDEX_MASK(q)) == \
      (((q)->prod) & INDEX_MASK(q))) && \
     ((((q)->cons) & WRAP_MASK(q)) != \
      (((q)->prod) & WRAP_MASK(q))))

#define Q_EMPTY(q) \
    (((((q)->cons) & INDEX_MASK(q)) == \
      (((q)->prod) & INDEX_MASK(q))) && \
     ((((q)->cons) & WRAP_MASK(q)) == \
      (((q)->prod) & WRAP_MASK(q))))

#define Q_INCONSISTENT(q) \
((((((q)->prod) & INDEX_MASK(q)) > (((q)->cons) & INDEX_MASK(q))) && \
((((q)->prod) & WRAP_MASK(q)) != (((q)->cons) & WRAP_MASK(q)))) || \
(((((q)->prod) & INDEX_MASK(q)) < (((q)->cons) & INDEX_MASK(q))) && \
((((q)->prod) & WRAP_MASK(q)) == (((q)->cons) & WRAP_MASK(q))))) \

#define SMMUV3_CMDQ_ENABLED(s) \
     (FIELD_EX32(s->cr[0], CR0, CMDQEN))

#define SMMUV3_EVENTQ_ENABLED(s) \
     (FIELD_EX32(s->cr[0], CR0, EVENTQEN))

static inline void smmu_write_cmdq_err(SMMUv3State *s, uint32_t err_type)
{
    s->cmdq.cons = FIELD_DP32(s->cmdq.cons, CMDQ_CONS, ERR, err_type);
}

void smmuv3_write_eventq(SMMUv3State *s, Evt *evt);

/* Commands */

enum {
    SMMU_CMD_PREFETCH_CONFIG = 0x01,
    SMMU_CMD_PREFETCH_ADDR,
    SMMU_CMD_CFGI_STE,
    SMMU_CMD_CFGI_STE_RANGE,
    SMMU_CMD_CFGI_CD,
    SMMU_CMD_CFGI_CD_ALL,
    SMMU_CMD_CFGI_ALL,
    SMMU_CMD_TLBI_NH_ALL     = 0x10,
    SMMU_CMD_TLBI_NH_ASID,
    SMMU_CMD_TLBI_NH_VA,
    SMMU_CMD_TLBI_NH_VAA,
    SMMU_CMD_TLBI_EL3_ALL    = 0x18,
    SMMU_CMD_TLBI_EL3_VA     = 0x1a,
    SMMU_CMD_TLBI_EL2_ALL    = 0x20,
    SMMU_CMD_TLBI_EL2_ASID,
    SMMU_CMD_TLBI_EL2_VA,
    SMMU_CMD_TLBI_EL2_VAA,  /* 0x23 */
    SMMU_CMD_TLBI_S12_VMALL  = 0x28,
    SMMU_CMD_TLBI_S2_IPA     = 0x2a,
    SMMU_CMD_TLBI_NSNH_ALL   = 0x30,
    SMMU_CMD_ATC_INV         = 0x40,
    SMMU_CMD_PRI_RESP,
    SMMU_CMD_RESUME          = 0x44,
    SMMU_CMD_STALL_TERM,
    SMMU_CMD_SYNC,          /* 0x46 */
};

static const char *cmd_stringify[] = {
    [SMMU_CMD_PREFETCH_CONFIG] = "SMMU_CMD_PREFETCH_CONFIG",
    [SMMU_CMD_PREFETCH_ADDR]   = "SMMU_CMD_PREFETCH_ADDR",
    [SMMU_CMD_CFGI_STE]        = "SMMU_CMD_CFGI_STE",
    [SMMU_CMD_CFGI_STE_RANGE]  = "SMMU_CMD_CFGI_STE_RANGE",
    [SMMU_CMD_CFGI_CD]         = "SMMU_CMD_CFGI_CD",
    [SMMU_CMD_CFGI_CD_ALL]     = "SMMU_CMD_CFGI_CD_ALL",
    [SMMU_CMD_CFGI_ALL]        = "SMMU_CMD_CFGI_ALL",
    [SMMU_CMD_TLBI_NH_ALL]     = "SMMU_CMD_TLBI_NH_ALL",
    [SMMU_CMD_TLBI_NH_ASID]    = "SMMU_CMD_TLBI_NH_ASID",
    [SMMU_CMD_TLBI_NH_VA]      = "SMMU_CMD_TLBI_NH_VA",
    [SMMU_CMD_TLBI_NH_VAA]     = "SMMU_CMD_TLBI_NH_VAA",
    [SMMU_CMD_TLBI_EL3_ALL]    = "SMMU_CMD_TLBI_EL3_ALL",
    [SMMU_CMD_TLBI_EL3_VA]     = "SMMU_CMD_TLBI_EL3_VA",
    [SMMU_CMD_TLBI_EL2_ALL]    = "SMMU_CMD_TLBI_EL2_ALL",
    [SMMU_CMD_TLBI_EL2_ASID]   = "SMMU_CMD_TLBI_EL2_ASID",
    [SMMU_CMD_TLBI_EL2_VA]     = "SMMU_CMD_TLBI_EL2_VA",
    [SMMU_CMD_TLBI_EL2_VAA]    = "SMMU_CMD_TLBI_EL2_VAA",
    [SMMU_CMD_TLBI_S12_VMALL]  = "SMMU_CMD_TLBI_S12_VMALL",
    [SMMU_CMD_TLBI_S2_IPA]     = "SMMU_CMD_TLBI_S2_IPA",
    [SMMU_CMD_TLBI_NSNH_ALL]   = "SMMU_CMD_TLBI_NSNH_ALL",
    [SMMU_CMD_ATC_INV]         = "SMMU_CMD_ATC_INV",
    [SMMU_CMD_PRI_RESP]        = "SMMU_CMD_PRI_RESP",
    [SMMU_CMD_RESUME]          = "SMMU_CMD_RESUME",
    [SMMU_CMD_STALL_TERM]      = "SMMU_CMD_STALL_TERM",
    [SMMU_CMD_SYNC]            = "SMMU_CMD_SYNC",
};

#define SMMU_CMD_STRING(type) (                                      \
(type < ARRAY_SIZE(cmd_stringify)) ? cmd_stringify[type] : "UNKNOWN" \
)

/* CMDQ fields */

typedef enum {
    SMMU_CERROR_NONE = 0,
    SMMU_CERROR_ILL,
    SMMU_CERROR_ABT,
    SMMU_CERROR_ATC_INV_SYNC,
} SMMUCmdError;

enum { /* Command completion notification */
    CMD_SYNC_SIG_NONE,
    CMD_SYNC_SIG_IRQ,
    CMD_SYNC_SIG_SEV,
};

#define CMD_TYPE(x)         extract32((x)->word[0], 0 , 8)
#define CMD_SSEC(x)         extract32((x)->word[0], 10, 1)
#define CMD_SSV(x)          extract32((x)->word[0], 11, 1)
#define CMD_RESUME_AC(x)    extract32((x)->word[0], 12, 1)
#define CMD_RESUME_AB(x)    extract32((x)->word[0], 13, 1)
#define CMD_SYNC_CS(x)      extract32((x)->word[0], 12, 2)
#define CMD_SSID(x)         extract32((x)->word[0], 12, 20)
#define CMD_SID(x)          ((x)->word[1])
#define CMD_VMID(x)         extract32((x)->word[1], 0 , 16)
#define CMD_ASID(x)         extract32((x)->word[1], 16, 16)
#define CMD_RESUME_STAG(x)  extract32((x)->word[2], 0 , 16)
#define CMD_RESP(x)         extract32((x)->word[2], 11, 2)
#define CMD_LEAF(x)         extract32((x)->word[2], 0 , 1)
#define CMD_STE_RANGE(x)    extract32((x)->word[2], 0 , 5)
#define CMD_ADDR(x) ({                                        \
            uint64_t high = (uint64_t)(x)->word[3];           \
            uint64_t low = extract32((x)->word[2], 12, 20);    \
            uint64_t addr = high << 32 | (low << 12);         \
            addr;                                             \
        })

int smmuv3_cmdq_consume(SMMUv3State *s);

#endif
