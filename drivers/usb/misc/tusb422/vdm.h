#ifndef __VDM_H__
#define __VDM_H__

/* VDM Header */
typedef enum {
	VDM_HDR_CMD_RESERVED,
	VDM_HDR_CMD_DISCOVER_IDENTITY,
	VDM_HDR_CMD_DISCOVER_SVIDS,
	VDM_HDR_CMD_DISCOVER_MODES,
	VDM_HDR_CMD_ENTER_MODE,
	VDM_HDR_CMD_EXIT_MODE,
	VDM_HDR_CMD_ATTENTION,
} vdm_cmd_t;

typedef enum {
	VDM_HDR_CMD_TYPE_REQ,
	VDM_HDR_CMD_TYPE_ACK,
	VDM_HDR_CMD_TYPE_NAK,
	VDM_HDR_CMD_TYPE_BUSY,
} vdm_cmd_type_t;

typedef enum {
	VDM_HDR_VERSION_10,
	VDM_HDR_VERSION_20,
} vdm_version_t;

typedef enum {
	VDM_HDR_TYPE_STRUCTURED_VDM = 1,
} vdm_type_t;

typedef enum {
	VDM_HDR_SVID_PD_SID = 0xFF00
} vdm_svid_t;

typedef struct {
	uint16_t Command:5;
	uint16_t Reserved1:1;
	uint16_t CommandType:2;
	uint16_t ObjectPosition:3;
	uint16_t Reserved2:2;
	uint16_t StructedVDMVersion:2;
	uint16_t VDMType:1;
	uint16_t SVID;
} vdm_hdr_t;

/* ID Header VDO */
typedef enum {
	VDM_ID_HDR_PRODUCT_TYPE_DFP_PDUSB_HUB		= 1,
	VDM_ID_HDR_PRODUCT_TYPE_DFP_PDUSB_HOST		= 2,
	VDM_ID_HDR_PRODUCT_TYPE_DFP_POWER_BRICK		= 3,
	VDM_ID_HDR_PRODUCT_TYPE_DFP_AMC			= 4,
} vdm_id_hdr_product_type_dfp_t;

typedef enum {
	VDM_ID_HDR_PRODUCT_TYPE_UFP_PDUSB_HUB		= 1,
	VDM_ID_HDR_PRODUCT_TYPE_UFP_PDUSB_PERIPHERAL	= 2,
	VDM_ID_HDR_PRODUCT_TYPE_UFP_AMC			= 5,
} vdm_id_hdr_product_type_ufp_t;

typedef struct {
	uint16_t VID;
	uint16_t Reserved:7;
	uint16_t ProductTypeDFP:3;
	uint16_t ModalOperationSupport:1;
	uint16_t ProductTypeUFP:3;
	uint16_t DeviceCap:1;
	uint16_t HostCap:1;
} vdm_id_hdr_t;

#endif /* __VDM_H__ */
