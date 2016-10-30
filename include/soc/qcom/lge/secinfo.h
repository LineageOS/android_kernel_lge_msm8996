#ifndef __MACH_SECINFO_H__
#define __MACH_SECINFO_H__

#define FUSEPROV_INFO_MAX_SIZE        16
#define FUSEPROV_MAX_FUSE_COUNT       30
#define FUSEPROV_SEC_STRUCTURE_MAX_NUM        10

/* Known fuses regions */
typedef enum
{
  FUSEPROV_REGION_TYPE_OEM_SEC_BOOT     = 0x00000000,
  FUSEPROV_REGION_TYPE_OEM_PK_HASH      = 0x00000001,
  FUSEPROV_REGION_TYPE_SEC_HW_KEY       = 0x00000002,
  FUSEPROV_REGION_TYPE_OEM_CONFIG       = 0x00000003,
  FUSEPROV_REGION_TYPE_READ_WRITE_PERM  = 0x00000004,
  FUSEPROV_REGION_TYPE_SPARE_REG19      = 0x00000005,
  FUSEPROV_REGION_TYPE_GENERAL          = 0x00000006,
  FUSEPROV_REGION_TYPE_FEC_EN           = 0x00000007,
  FUSEPROV_REGION_TYPE_MAX              = 0x7FFFFFFF
} fuseprov_region_etype;

typedef enum
{
  FUSEPROV_OPERATION_TYPE_BLOW           = 0x00000000,
  FUSEPROV_OPERATION_TYPE_VERIFYMASK0    = 0x00000001,
  FUSEPROV_OPERATION_TYPE_MAX            = 0x7FFFFFFF
} fuseprov_operation_etype;

typedef struct
{
  unsigned int  magic1;
  unsigned int  magic2;
  unsigned int  revision;
  unsigned int  size;
  unsigned char info[FUSEPROV_INFO_MAX_SIZE];
  unsigned int  segment_number;               /* the number of segments */ //new
  unsigned int  reserved[3];
} fuseprov_secdat_hdr_type;

//add
typedef struct
{
  unsigned int  offset;
  unsigned short  type;
  unsigned short  attribute;
} fuseprov_secdat_hdr_segment_type;

typedef struct
{
  unsigned int  revision;
  unsigned int  size;
  unsigned int  fuse_count;
  unsigned int  reserved[4];
} fuseprov_qfuse_list_hdr_type;

typedef struct
{
  fuseprov_region_etype     region_type;
  unsigned int              addr;
  unsigned int              lsb;
  unsigned int              msb;
  fuseprov_operation_etype  operation;
} fuseprov_qfuse_entry;

typedef struct
{
  unsigned char    hash[32];
} fuseprov_secdat_footer_type;              /* Hash of sec.dat file starting from the main header till before this footer */

typedef struct {
 fuseprov_secdat_hdr_type hdr;
 fuseprov_secdat_hdr_segment_type segment;   //add
 fuseprov_qfuse_list_hdr_type list_hdr;
 fuseprov_qfuse_entry *pentry;
 fuseprov_secdat_footer_type footer;
} fuseprov_secdat_type;

#endif

