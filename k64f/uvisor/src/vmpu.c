/***************************************************************
 * This confidential and  proprietary  software may be used only
 * as authorised  by  a licensing  agreement  from  ARM  Limited
 *
 *             (C) COPYRIGHT 2013-2014 ARM Limited
 *                      ALL RIGHTS RESERVED
 *
 *  The entire notice above must be reproduced on all authorised
 *  copies and copies  may only be made to the  extent permitted
 *  by a licensing agreement from ARM Limited.
 *
 ***************************************************************/
#include <uvisor.h>
#include "vmpu.h"
#include "svc.h"
#include "debug.h"

#ifndef MPU_MAX_PRIVATE_FUNCTIONS
#define MPU_MAX_PRIVATE_FUNCTIONS 16
#endif/*MPU_MAX_PRIVATE_FUNCTIONS*/

/* predict SRAM offset */
#ifdef RESERVED_SRAM
#  define RESERVED_SRAM_START UVISOR_ROUND32_UP(SRAM_ORIGIN+RESERVED_SRAM)
#else
#  define RESERVED_SRAM_START SRAM_ORIGIN
#endif

#if (MPU_MAX_PRIVATE_FUNCTIONS>0x100UL)
#error "MPU_MAX_PRIVATE_FUNCTIONS needs to be lower/equal to 0x100"
#endif

#define MPU_FAULT_USAGE  0x00
#define MPU_FAULT_MEMORY 0x01
#define MPU_FAULT_BUS    0x02
#define MPU_FAULT_HARD   0x03
#define MPU_FAULT_DEBUG  0x04

static void vmpu_fault(int reason)
{
    uint32_t sperr,t;

    /* print slave port details */
    dprintf("CESR : 0x%08X\n\r", MPU->CESR);
    sperr = (MPU->CESR >> 27);
    for(t=0; t<5; t++)
    {
        if(sperr & 0x10)
            dprintf("  SLAVE_PORT[%i]: @0x%08X (Detail 0x%08X)\n\r",
                t,
                MPU->SP[t].EAR,
                MPU->SP[t].EDR);
        sperr <<= 1;
    }
    dprintf("CFSR : 0x%08X\n\r", SCB->CFSR);
    while(1);
}

static void vmpu_fault_bus(void)
{
    DEBUG_FAULT_BUS();
    dprintf("Bus Fault\n\r");
    dprintf("BFAR : 0x%08X\n\r", SCB->BFAR);
    vmpu_fault(MPU_FAULT_BUS);
}

static void vmpu_fault_usage(void)
{
    dprintf("Usage Fault\n\r");
    vmpu_fault(MPU_FAULT_USAGE);
}

static void vmpu_fault_hard(void)
{
    dprintf("Hard Fault\n\r");
    dprintf("HFSR : 0x%08X\n\r", SCB->HFSR);
    vmpu_fault(MPU_FAULT_HARD);
}

static void vmpu_fault_debug(void)
{
    dprintf("Debug Fault\n\r");
    vmpu_fault(MPU_FAULT_DEBUG);
}

int vmpu_acl_dev(UvisorBoxAcl acl, uint16_t device_id)
{
    return 1;
}

int vmpu_acl_mem(UvisorBoxAcl acl, uint32_t addr, uint32_t size)
{
    return 1;
}

int vmpu_acl_reg(UvisorBoxAcl acl, uint32_t addr, uint32_t rmask,
                 uint32_t wmask)
{
    return 1;
}

int vmpu_acl_bit(UvisorBoxAcl acl, uint32_t addr)
{
    return 1;
}

int vmpu_switch(uint8_t box)
{
    return -1;
}

bool vmpu_valid_code_addr(const void* address)
{
    return (((uint32_t)address) >= FLASH_ORIGIN)
        && (((uint32_t)address) < (uint32_t)__uvisor_config.secure_start);
}

int vmpu_sanity_checks(void)
{
    /* verify uvisor config structure */
    if(__uvisor_config.magic != UVISOR_MAGIC)
        while(1)
        {
            DPRINTF("config magic mismatch: &0x%08X = 0x%08X \
                                 - exptected 0x%08X\n",
                &__uvisor_config,
                __uvisor_config.magic,
                UVISOR_MAGIC);
        }

    /* verify if configuration mode is inside flash memory */
    assert((uint32_t)__uvisor_config.mode >= FLASH_ORIGIN);
    assert((uint32_t)__uvisor_config.mode <= (FLASH_ORIGIN + FLASH_LENGTH - 4));
    DPRINTF("uvisor_mode: %u\n", *__uvisor_config.mode);
    assert(*__uvisor_config.mode <= 2);

    /* verify flash origin and size */
    assert( FLASH_ORIGIN  == 0 );
    assert( __builtin_popcount(FLASH_ORIGIN + FLASH_LENGTH) == 1 );

    /* verify SRAM relocation */
    DPRINTF("uvisor_ram : @0x%08X (%u bytes) [config]\n",
        __uvisor_config.reserved_start,
        VMPU_REGION_SIZE(__uvisor_config.reserved_start,
                         __uvisor_config.reserved_end));
    DPRINTF("             (0x%08X (%u bytes) [linker]\n",
            RESERVED_SRAM_START, USE_SRAM_SIZE);
    assert( __uvisor_config.reserved_end > __uvisor_config.reserved_start );
    assert( VMPU_REGION_SIZE(__uvisor_config.reserved_start,
                             __uvisor_config.reserved_end) == USE_SRAM_SIZE );
    assert(&__stack_end__ <= __uvisor_config.reserved_end);

    assert( (uint32_t) __uvisor_config.reserved_start == RESERVED_SRAM_START);
    assert( (uint32_t) __uvisor_config.reserved_end == (RESERVED_SRAM_START +
                                                        USE_SRAM_SIZE) );

    /* verify that __uvisor_config is within valid flash */
    assert( ((uint32_t) &__uvisor_config) >= FLASH_ORIGIN );
    assert( ((((uint32_t) &__uvisor_config) + sizeof(__uvisor_config))
             <= (FLASH_ORIGIN + FLASH_LENGTH)) );

    /* verify that secure flash area is accessible and after public code */
    assert( __uvisor_config.secure_start <= __uvisor_config.secure_end );
    assert( (uint32_t) __uvisor_config.secure_end <=
            (uint32_t) (FLASH_ORIGIN + FLASH_LENGTH) );
    assert( (uint32_t) __uvisor_config.secure_start >=
            (uint32_t) &vmpu_sanity_checks );

    /* verify configuration table */
    assert( __uvisor_config.cfgtbl_start <= __uvisor_config.cfgtbl_end );
    assert( __uvisor_config.cfgtbl_start >= __uvisor_config.secure_start );
    assert( (uint32_t) __uvisor_config.cfgtbl_end <=
            (uint32_t) (FLASH_ORIGIN + FLASH_LENGTH) );

    /* verify data initialization section */
    assert( __uvisor_config.data_src >= __uvisor_config.secure_start );
    assert( __uvisor_config.data_start <= __uvisor_config.data_end );
    assert( __uvisor_config.data_start >= __uvisor_config.secure_start );
    assert( __uvisor_config.data_start >= __uvisor_config.reserved_end );
    assert( (uint32_t) __uvisor_config.data_end <=
            (uint32_t) (SRAM_ORIGIN + SRAM_LENGTH - STACK_SIZE));

    /* verify data bss section */
    assert( __uvisor_config.bss_start <= __uvisor_config.bss_end );
    assert( __uvisor_config.bss_start >= __uvisor_config.secure_start );
    assert( __uvisor_config.bss_start >= __uvisor_config.reserved_end );
    assert( (uint32_t) __uvisor_config.data_end <=
            (uint32_t) (SRAM_ORIGIN + SRAM_LENGTH - STACK_SIZE));

    /* check section ordering */
    assert( __uvisor_config.bss_end <= __uvisor_config.data_start );

    /* return error if uvisor is disabled */
    if(!__uvisor_config.mode || (*__uvisor_config.mode == 0))
        return -1;
    else
        return 0;
}

static void vmpu_init_box_memories(void)
{
    DPRINTF("erasing BSS at 0x%08X (%u bytes)\n",
        __uvisor_config.bss_start,
        VMPU_REGION_SIZE(__uvisor_config.bss_start, __uvisor_config.bss_end)
    );

    /* reset uninitialized secured box data sections */
    memset(
        __uvisor_config.bss_start,
        0,
        VMPU_REGION_SIZE(__uvisor_config.bss_start, __uvisor_config.bss_end)
    );

    DPRINTF("copying .data from 0x%08X to 0x%08X (%u bytes)\n",
        __uvisor_config.data_src,
        __uvisor_config.data_start,
        VMPU_REGION_SIZE(__uvisor_config.data_start, __uvisor_config.data_end)
    );

    /* initialize secured box data sections */
    memcpy(
        __uvisor_config.data_start,
        __uvisor_config.data_src,
        VMPU_REGION_SIZE(__uvisor_config.data_start, __uvisor_config.data_end)
    );
}

static void vmpu_add_acl(uint8_t box_id, void* start, uint32_t size, UvisorBoxAcl acl)
{
    if(acl & UVISOR_TACL_SIZE_ROUND_UP)
        size = UVISOR_ROUND32_UP(size);

    if(acl & UVISOR_TACL_SIZE_ROUND_DOWN)
        size = UVISOR_ROUND32_DOWN(size);

    DPRINTF("\t@0x%08X size=%06i acl=0x%04X\n", start, size, acl);
}

static void vmpu_load_boxes(void)
{
    int i, count;
    uint32_t *addr, sp, sp_size;
    const UvisorBoxAclItem *region;
    const UvisorBoxConfig **box_cfgtbl;
    uint8_t box_id;

    /* stack region grows from bss_start downwards */
    sp = UVISOR_STACK_SIZE_ROUND((uint32_t)__uvisor_config.bss_start);

    /* read stack pointer from vector table */
    g_svc_cx_curr_sp[0] = *((uint32_t**)0);
    DPRINTF("box[0] stack pointer = 0x%08X\n", g_svc_cx_curr_sp[0]);

    /* enumerate and initialize boxes */
    g_svc_cx_box_num = 1;
    for(box_cfgtbl = (const UvisorBoxConfig**) __uvisor_config.cfgtbl_start;
        box_cfgtbl < (const UvisorBoxConfig**) __uvisor_config.cfgtbl_end;
        ++addr)
    {
        /* ensure that configuration resides in flash */
        if(!(vmpu_valid_code_addr(*box_cfgtbl) &&
            vmpu_valid_code_addr(
                (uint8_t*)(*box_cfgtbl) + (sizeof(**box_cfgtbl)-1))
            ))
        {
            DPRINTF("invalid address - *box_cfgtbl must point to flash (0x%08X)\n", *box_cfgtbl);
            /* FIXME fail properly */
            while(1);
        }

        /* check for magic value in box configuration */
        if(((*box_cfgtbl)->magic)!=UVISOR_BOX_MAGIC)
        {
            DPRINTF("box[%i] @0x%08X - invalid magic\n",
                g_svc_cx_box_num,
                (uint32_t)(*box_cfgtbl)
            );
            /* FIXME fail properly */
            while(1);
        }

        /* check for magic value in box configuration */
        if(((*box_cfgtbl)->version)!=UVISOR_BOX_VERSION)
        {
            DPRINTF("box[%i] @0x%08X - invalid version (0x%04X!-0x%04X)\n",
                g_svc_cx_box_num,
                *box_cfgtbl,
                (*box_cfgtbl)->version,
                UVISOR_BOX_VERSION
            );
            /* FIXME fail properly */
            while(1);
        }

        /* increment box counter */
        if((box_id = g_svc_cx_box_num++)>=SVC_CX_MAX_BOXES)
        {
            DPRINTF("box number overflow\n");
            /* FIXME fail properly */
            while(1);
        }

        /* load box ACLs in table */
        DPRINTF("box[%i] ACL list:\n", box_id);
        region = (*box_cfgtbl)->acl_list;
        count = (*box_cfgtbl)->acl_count;
        for(i=0; i<count; i++)
        {
            /* ensure that ACL resides in flash */
            if(!vmpu_valid_code_addr(region))
            {
                DPRINTF("box[i]:acl[i] must be in code section (@0x%08X)\n",
                    g_svc_cx_box_num,
                    i,
                    *box_cfgtbl
                );
                /* FIXME fail properly */
                while(1);
            }

            vmpu_add_acl(
                box_id,
                region->start,
                region->length,
                region->acl
            );

            /* proceed to next ACL */
            region++;
        }

        /* determine stack extent */
        sp_size = UVISOR_STACK_SIZE_ROUND((*box_cfgtbl)->stack_size);
        sp -= sp_size;
        /* add stack ACL to list */
        vmpu_add_acl(
            box_id,
            (void*)sp,
            sp_size,
            UVISOR_TACL_STACK
        );
        /* set stack pointer to box stack size minus guard band */
        g_svc_cx_curr_sp[box_id] = (uint32_t*)(sp-UVISOR_STACK_BAND_SIZE);

        /* do next box configuration */
        box_cfgtbl++;
    }

    /* check consistency between allocated and actual stack sizes */
    if(sp != (uint32_t)__uvisor_config.reserved_end)
    {
        DPRINTF("stack didn't match up: 0x%X != 0x%X\n",
            sp,
            __uvisor_config.reserved_end
        );
        /* FIXME fail properly */
        while(1);
    }
    DPRINTF("vmpu_load_boxes [DONE]\n");
}

void vmpu_init(void)
{
    /* always initialize protected box memories */
    vmpu_init_box_memories();

    /* load boxes */
    vmpu_load_boxes();

    /* setup security "bluescreen" exceptions */
    ISR_SET(BusFault_IRQn,         &vmpu_fault_bus);
    ISR_SET(UsageFault_IRQn,       &vmpu_fault_usage);
    ISR_SET(HardFault_IRQn,        &vmpu_fault_hard);
    ISR_SET(DebugMonitor_IRQn,     &vmpu_fault_debug);

    /* enable mem, bus and usage faults */
    SCB->SHCSR |= 0x70000;
}
