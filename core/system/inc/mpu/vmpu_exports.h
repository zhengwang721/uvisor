/***************************************************************
 * This confidential and  proprietary  software may be used only
 * as authorised  by  a licensing  agreement  from  ARM  Limited
 *
 *             (C) COPYRIGHT 2013-2015 ARM Limited
 *                      ALL RIGHTS RESERVED
 *
 *  The entire notice above must be reproduced on all authorised
 *  copies and copies  may only be made to the  extent permitted
 *  by a licensing agreement from ARM Limited.
 *
 ***************************************************************/
#ifndef __VMPU_EXPORTS_H__
#define __VMPU_EXPORTS_H__

/* supervisor user access modes */
#define UVISOR_TACL_UEXECUTE        0x0001UL
#define UVISOR_TACL_UWRITE          0x0002UL
#define UVISOR_TACL_UREAD           0x0004UL
#define UVISOR_TACL_UACL            (UVISOR_TACL_UREAD          |\
                                     UVISOR_TACL_UWRITE         |\
                                     UVISOR_TACL_UEXECUTE)

/* supervisor access modes */
#define UVISOR_TACL_SEXECUTE        0x0008UL
#define UVISOR_TACL_SWRITE          0x0010UL
#define UVISOR_TACL_SREAD           0x0020UL
#define UVISOR_TACL_SACL            (UVISOR_TACL_SREAD          |\
                                     UVISOR_TACL_SWRITE         |\
                                     UVISOR_TACL_SEXECUTE)

/* various modes */
#define UVISOR_TACL_STACK           0x0040UL
#define UVISOR_TACL_SIZE_ROUND_UP   0x0080UL
#define UVISOR_TACL_SIZE_ROUND_DOWN 0x0100UL
#define UVISOR_TACL_PERIPHERAL      0x0200UL
#define UVISOR_TACL_SHARED          0x0400UL
#define UVISOR_TACL_USER            0x0800UL

#define UVISOR_TACLDEF_SECURE_BSS   (UVISOR_TACL_UREAD          |\
                                     UVISOR_TACL_UWRITE         |\
                                     UVISOR_TACL_SREAD          |\
                                     UVISOR_TACL_SWRITE         |\
                                     UVISOR_TACL_SIZE_ROUND_UP)

#define UVISOR_TACLDEF_SECURE_CONST (UVISOR_TACL_UREAD          |\
                                     UVISOR_TACL_SREAD          |\
                                     UVISOR_TACL_SIZE_ROUND_UP)

#define UVISOR_TACLDEF_DATA         UVISOR_TACLDEF_SECURE_BSS

#define UVISOR_TACLDEF_PERIPH       (UVISOR_TACL_PERIPHERAL     |\
                                     UVISOR_TACL_UREAD          |\
                                     UVISOR_TACL_UWRITE         |\
                                     UVISOR_TACL_SIZE_ROUND_UP)

#define UVISOR_TACLDEF_STACK        (UVISOR_TACL_STACK          |\
                                     UVISOR_TACL_UREAD          |\
                                     UVISOR_TACL_UWRITE         |\
                                     UVISOR_TACL_SREAD          |\
                                     UVISOR_TACL_SWRITE)

#define UVISOR_TO_STR(x)            #x
#define UVISOR_TO_STRING(x)         UVISOR_TO_STR(x)
#define UVISOR_ARRAY_COUNT(x)       (sizeof(x)/sizeof(x[0]))
#define UVISOR_ROUND32_DOWN(x)      ((x) & ~0x1FUL)
#define UVISOR_ROUND32_UP(x)        UVISOR_ROUND32_DOWN((x)+31UL)
#define UVISOR_PAD32(x)             (32 - (sizeof(x) & ~0x1FUL))
#define UVISOR_BOX_MAGIC            0x42CFB66FUL
#define UVISOR_BOX_VERSION          100
#define UVISOR_STACK_BAND_SIZE      128
#define UVISOR_MEM_SIZE_ROUND(x)    UVISOR_ROUND32_UP(x)
#define UVISOR_STACK_SIZE_ROUND(x)  UVISOR_MEM_SIZE_ROUND((x) + \
                                    (UVISOR_STACK_BAND_SIZE * 2))
#ifndef UVISOR_BOX_STACK_SIZE
#define UVISOR_BOX_STACK_SIZE 1024
#endif/*UVISOR_BOX_STACK*/

typedef uint32_t UvisorBoxAcl;

typedef struct
{
    void* start;
    uint32_t length;
    UvisorBoxAcl acl;
} UVISOR_PACKED UvisorBoxAclItem;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t stack_size;
    uint32_t context_size;

    const UvisorBoxAclItem* const acl_list;
    uint32_t acl_count;
} UVISOR_PACKED UvisorBoxConfig;

#endif/*__VMPU_EXPORTS_H__*/