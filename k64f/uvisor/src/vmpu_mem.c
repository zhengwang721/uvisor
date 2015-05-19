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
#include "svc.h"
#include "vmpu.h"
#include "vmpu_mem.h"

typedef struct
{
    uint32_t word[3];
} TMemACL;

typedef struct
{
    TMemACL *acl;
    uint32_t count;
} TBoxACL;

uint32_t g_mem_acl_count;
static TMemACL g_mem_acl[UVISOR_MAX_ACLS];
static TBoxACL g_mem_box[UVISOR_MAX_BOXES];

static int vmpu_add_mem_int(uint8_t box_id, void* start, uint32_t size, UvisorBoxAcl acl)
{
    uint32_t perm;
    TBoxACL *box;
    TMemACL *rgd;

    /* handle empty or fully protected regions */
    if(!size || !(acl & (UVISOR_TACL_UACL|UVISOR_TACL_SACL)))
        return 1;

    /* ensure that ACL size can be rounded up to slot size */
    if(size % 32)
    {
        if(acl & UVISOR_TACL_SIZE_ROUND_DOWN)
            size = UVISOR_ROUND32_DOWN(size);
        else
            if(acl & UVISOR_TACL_SIZE_ROUND_UP)
                size = UVISOR_ROUND32_UP(size);
            else
                {
                    DPRINTF("Use UVISOR_TACL_SIZE_ROUND_UP/*_DOWN to round ACL size");
                    return -21;
                }
    }

    /* ensure that ACL base is a multiple of 32 */
    if((((uint32_t)start) % 32) != 0)
    {
        DPRINTF("start address needs to be aligned on a 32 bytes border");
        return -22;
    }

    /* get box config */
    if(box_id >= UVISOR_MAX_ACLS)
        return -23;
    box = &g_mem_box[box_id];

    /* initialize acl pointer */
    if(!box->acl)
    {
        /* check for integer overflow */
        if( g_mem_acl_count >= UVISOR_MAX_ACLS )
            return -24;
        box->acl = &g_mem_acl[g_mem_acl_count];
    }

    /* check for precise overflow */
    if( (&box->acl[box->count] - g_mem_acl)>=(UVISOR_MAX_ACLS-1) )
        return -25;

    /* get mem ACL */
    rgd = &box->acl[box->count];
    /* already populated - ensure to fill boxes unfragmented */
    if(rgd->word[0])
        return -26;

    /* start address, aligned tro 32 bytes */
    rgd->word[0] = (uint32_t) start;
    /* end address, aligned tro 32 bytes */
    rgd->word[1] = ((uint32_t) start) + size;

    /* handle user permissions */
    perm = (acl & UVISOR_TACL_USER) ?  acl & UVISOR_TACL_UACL : 0;

    /* if S-perms are identical to U-perms, refer from S to U */
    if(((acl & UVISOR_TACL_SACL)>>3) == perm)
        perm |= 3UL << 3;
    else
        /* handle detailed supervisor permissions */
        switch(acl & UVISOR_TACL_SACL)
        {
            case UVISOR_TACL_SREAD|UVISOR_TACL_SWRITE|UVISOR_TACL_SEXECUTE:
                perm |= 0UL << 3;
                break;

            case UVISOR_TACL_SREAD|UVISOR_TACL_SEXECUTE:
                perm |= 1UL << 3;
                break;

            case UVISOR_TACL_SREAD|UVISOR_TACL_SWRITE:
                perm |= 2UL << 3;
                break;

            default:
                DPRINTF("chosen supervisor ACL's are not supported by hardware (0x%08X)\n", acl);
                return -7;
        }
    rgd->word[2] = perm;

    /* increment ACL count */
    box->count++;
    /* increment total ACL count */
    g_mem_acl_count++;

    return 1;
}

int vmpu_add_mem(uint8_t box_id, void* start, uint32_t size, UvisorBoxAcl acl)
{
    if(    (((uint32_t*)start)>=__uvisor_config.secure_start) &&
        ((((uint32_t)start)+size)<=(uint32_t)__uvisor_config.secure_end) )
    {
        DPRINTF("\t\tFLASH\n");

        return vmpu_add_mem_int(box_id, start, size, acl & UVISOR_TACLDEF_SECURE_CONST);
    }

    if(    (((uint32_t*)start)>=__uvisor_config.reserved_end) &&
        ((((uint32_t)start)+size)<=(uint32_t)__uvisor_config.bss_start) )
    {
        DPRINTF("\t\tSTACK\n");

        /* disallow user to provide stack region ACL's */
        if(acl & UVISOR_TACL_USER)
            return -1;

        return vmpu_add_mem_int(box_id, start, size, (acl & UVISOR_TACLDEF_STACK)|UVISOR_TACL_STACK);
    }

    if(    (((uint32_t*)start)>=__uvisor_config.bss_start) &&
        ((((uint32_t)start)+size)<=(uint32_t)__uvisor_config.bss_end) )
    {
        DPRINTF("\t\tBSS\n");

        return vmpu_add_mem_int(box_id, start, size, acl & UVISOR_TACLDEF_SECURE_BSS);
    }

    return 0;
}