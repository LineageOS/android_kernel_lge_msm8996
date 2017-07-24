#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_ois_i2c.h"
#include "OIS_Init_IMT.h"

#define	OIS_FW_POLLING_PASS				0
#define	OIS_FW_POLLING_FAIL				-1
#define	OIS_FW_POLLING_VERSION_FAIL		-2

/* 8bit */
const struct STFILREG CsFilReg[] = {
	{0x0111, 0x03}, 	/*03,0111*/
	{0x0113, 0x00}, 	/*00,0113*/
	{0x0114, 0x00}, 	/*00,0114*/
	{0x0172, 0x00}, 	/*00,0172*/
	{0x01E3, 0x0F}, 	/*0F,01E3*/
	{0x01E4, 0x00}, 	/*00,01E4*/
	{0xFFFF, 0xFF}
};
/* 32bit */
const struct STFILRAM CsFilRam[] = {
	{0x1000, 0x3F800000}, 	/*3F800000,1000,0dB,invert=0*/
	{0x1001, 0x3F800000}, 	/*3F800000,1001,0dB,invert=0*/
	{0x1002, 0x00000000}, 	/*00000000,1002,Cutoff,invert=0*/
	{0x1003, 0x3F800000}, 	/*3F800000,1003,0dB,invert=0*/
	{0x1004, 0x3DF21080}, 	/*3DF21080,1004,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1005, 0x3DF21080}, 	/*3DF21080,1005,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1006, 0x3F437BC0}, 	/*3F437BC0,1006,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1007, 0x00000000}, 	/*00000000,1007,Cutoff,invert=0*/
	{0x1008, 0x3F800000}, 	/*3F800000,1008,0dB,invert=0*/
	{0x1009, 0xBF800000}, 	/*BF800000,1009,0dB,invert=1*/
	{0x100A, 0x3F800000}, 	/*3F800000,100A,0dB,invert=0*/
	{0x100B, 0x3F800000}, 	/*3F800000,100B,0dB,invert=0*/
	{0x100C, 0x3F800000}, 	/*3F800000,100C,0dB,invert=0*/
	{0x100E, 0x3F800000}, 	/*3F800000,100E,0dB,invert=0*/
	{0x1010, 0x3DCCCCC0}, 	/*3DCCCCC0,1010*/
	{0x1011, 0x00000000}, 	/*00000000,1011,Free,fs/4,invert=0*/
	{0x1012, 0x3F7FF000}, 	/*3F7FF000,1012,Free,fs/4,invert=0*/
	{0x1013, 0x40D04F40}, 	/*40D04F40,1013,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1014, 0xC0C50240}, 	/*C0C50240,1014,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1015, 0x3F259600}, 	/*3F259600,1015,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1016, 0x3F800400}, 	/*3F800400,1016,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1017, 0xBF7FCD00}, 	/*BF7FCD00,1017,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1018, 0x3F7FDD80}, 	/*3F7FDD80,1018,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1019, 0x3F800000}, 	/*3F800000,1019,Through,0dB,fs/4,invert=0*/
	{0x101A, 0x00000000}, 	/*00000000,101A,Through,0dB,fs/4,invert=0*/
	{0x101B, 0x00000000}, 	/*00000000,101B,Through,0dB,fs/4,invert=0*/
	{0x101C, 0x00000000}, 	/*00000000,101C,Cutoff,invert=0*/
	{0x101D, 0x3F800000}, 	/*3F800000,101D,0dB,invert=0*/
	{0x101E, 0x3F800000}, 	/*3F800000,101E,0dB,invert=0*/
	{0x1020, 0x3F800000}, 	/*3F800000,1020,0dB,invert=0*/
	{0x1021, 0x3F800000}, 	/*3F800000,1021,0dB,invert=0*/
	{0x1022, 0x3F800000}, 	/*3F800000,1022,0dB,invert=0*/
	{0x1023, 0x3F800000}, 	/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024, 0x00000000}, 	/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025, 0x00000000}, 	/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026, 0x00000000}, 	/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027, 0x00000000}, 	/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030, 0x00000000}, 	/*00000000,1030,Free,fs/4,invert=0*/
	{0x1031, 0x3B031240}, 	/*3B031240,1031,Free,fs/4,invert=0*/
	{0x1032, 0x3F800000}, 	/*3F800000,1032,Free,fs/4,invert=0*/
	{0x1033, 0x3F800000}, 	/*3F800000,1033,Through,0dB,fs/4,invert=0*/
	{0x1034, 0x00000000}, 	/*00000000,1034,Through,0dB,fs/4,invert=0*/
	{0x1035, 0x00000000}, 	/*00000000,1035,Through,0dB,fs/4,invert=0*/
	{0x1036, 0x4F7FFFC0}, 	/*4F7FFFC0,1036,Free,fs/4,invert=0*/
	{0x1037, 0x00000000}, 	/*00000000,1037,Free,fs/4,invert=0*/
	{0x1038, 0x00000000}, 	/*00000000,1038,Free,fs/4,invert=0*/
	{0x1039, 0x00000000}, 	/*00000000,1039,Free,fs/4,invert=0*/
	{0x103A, 0x30800000}, 	/*30800000,103A,Free,fs/4,invert=0*/
	{0x103B, 0x00000000}, 	/*00000000,103B,Free,fs/4,invert=0*/
	{0x103C, 0x3F800000}, 	/*3F800000,103C,Through,0dB,fs/4,invert=0*/
	{0x103D, 0x00000000}, 	/*00000000,103D,Through,0dB,fs/4,invert=0*/
	{0x103E, 0x00000000}, 	/*00000000,103E,Through,0dB,fs/4,invert=0*/
	{0x1043, 0x3AD27C40}, 	/*3AD27C40,1043,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1044, 0x3AD27C40}, 	/*3AD27C40,1044,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1045, 0x3F7F2D80}, 	/*3F7F2D80,1045,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1046, 0x398C8300}, 	/*398C8300,1046,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1047, 0x398C8300}, 	/*398C8300,1047,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1048, 0x3F7FDCC0}, 	/*3F7FDCC0,1048,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1049, 0x3A0C7980}, 	/*3A0C7980,1049,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x104A, 0x3A0C7980}, 	/*3A0C7980,104A,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x104B, 0x3F7FB9C0}, 	/*3F7FB9C0,104B,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x104C, 0x3A8C6640}, 	/*3A8C6640,104C,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x104D, 0x3A8C6640}, 	/*3A8C6640,104D,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x104E, 0x3F7F7380}, 	/*3F7F7380,104E,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x1053, 0x3F800000}, 	/*3F800000,1053,Through,0dB,fs/4,invert=0*/
	{0x1054, 0x00000000}, 	/*00000000,1054,Through,0dB,fs/4,invert=0*/
	{0x1055, 0x00000000}, 	/*00000000,1055,Through,0dB,fs/4,invert=0*/
	{0x1056, 0x3F800000}, 	/*3F800000,1056,Through,0dB,fs/4,invert=0*/
	{0x1057, 0x00000000}, 	/*00000000,1057,Through,0dB,fs/4,invert=0*/
	{0x1058, 0x00000000}, 	/*00000000,1058,Through,0dB,fs/4,invert=0*/
	{0x1059, 0x3F800000}, 	/*3F800000,1059,Through,0dB,fs/4,invert=0*/
	{0x105A, 0x00000000}, 	/*00000000,105A,Through,0dB,fs/4,invert=0*/
	{0x105B, 0x00000000}, 	/*00000000,105B,Through,0dB,fs/4,invert=0*/
	{0x105C, 0x3F800000}, 	/*3F800000,105C,Through,0dB,fs/4,invert=0*/
	{0x105D, 0x00000000}, 	/*00000000,105D,Through,0dB,fs/4,invert=0*/
	{0x105E, 0x00000000}, 	/*00000000,105E,Through,0dB,fs/4,invert=0*/
	{0x1063, 0x3F800000}, 	/*3F800000,1063,0dB,invert=0*/
	{0x1066, 0x3F800000}, 	/*3F800000,1066,0dB,invert=0*/
	{0x1069, 0x3F800000}, 	/*3F800000,1069,0dB,invert=0*/
	{0x106C, 0x3F800000}, 	/*3F800000,106C,0dB,invert=0*/
	{0x1073, 0x00000000}, 	/*00000000,1073,Cutoff,invert=0*/
	{0x1076, 0x3F800000}, 	/*3F800000,1076,0dB,invert=0*/
	{0x1079, 0x3F800000}, 	/*3F800000,1079,0dB,invert=0*/
	{0x107C, 0x3F800000}, 	/*3F800000,107C,0dB,invert=0*/
	{0x1083, 0x38D1B700}, 	/*38D1B700,1083,-80dB,invert=0*/
	{0x1086, 0x00000000}, 	/*00000000,1086,Cutoff,invert=0*/
	{0x1089, 0x00000000}, 	/*00000000,1089,Cutoff,invert=0*/
	{0x108C, 0x00000000}, 	/*00000000,108C,Cutoff,invert=0*/
	{0x1093, 0x00000000}, 	/*00000000,1093,Cutoff,invert=0*/
	{0x1098, 0x3F800000}, 	/*3F800000,1098,0dB,invert=0*/
	{0x1099, 0x3F800000}, 	/*3F800000,1099,0dB,invert=0*/
	{0x109A, 0x3F800000}, 	/*3F800000,109A,0dB,invert=0*/
	{0x10A1, 0x3BDA2580}, 	/*3BDA2580,10A1,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A2, 0x3BDA2580}, 	/*3BDA2580,10A2,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A3, 0x3F7C9780}, 	/*3F7C9780,10A3,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A4, 0x00000000}, 	/*00000000,10A4,Free,fs/1,invert=0*/
	{0x10A5, 0x3A031240}, 	/*3A031240,10A5,Free,fs/1,invert=0*/
	{0x10A6, 0x3F800000}, 	/*3F800000,10A6,Free,fs/1,invert=0*/
	{0x10A7, 0x3F800000}, 	/*3F800000,10A7,Through,0dB,fs/4,invert=0*/
	{0x10A8, 0x00000000}, 	/*00000000,10A8,Through,0dB,fs/4,invert=0*/
	{0x10A9, 0x00000000}, 	/*00000000,10A9,Through,0dB,fs/4,invert=0*/
	{0x10AA, 0x00000000}, 	/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB, 0x3BDA2580}, 	/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC, 0x3BDA2580}, 	/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD, 0x3F7C9780}, 	/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0, 0x3F800000}, 	/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1, 0x00000000}, 	/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2, 0x00000000}, 	/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3, 0x3F800000}, 	/*3F800000,10B3,0dB,invert=0*/
	{0x10B4, 0x00000000}, 	/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5, 0x00000000}, 	/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6, 0x3F353C00}, 	/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8, 0x3F800000}, 	/*3F800000,10B8,0dB,invert=0*/
	{0x10B9, 0x00000000}, 	/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0, 0x3FD13600}, 	/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1, 0xBFCEFAC0}, 	/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2, 0x3F5414C0}, 	/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3, 0x3F800000}, 	/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4, 0x00000000}, 	/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5, 0x00000000}, 	/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6, 0x3D506F00}, 	/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7, 0x3D506F00}, 	/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8, 0x3F65F240}, 	/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9, 0x3C1CFA80}, 	/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA, 0x3C1CFA80}, 	/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB, 0x3F7FF040}, 	/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC, 0x3E300080}, 	/*3E300080,10CC,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CD, 0xBE2EC780}, 	/*BE2EC780,10CD,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CE, 0x3F7FA840}, 	/*3F7FA840,10CE,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10D0, 0x3FFF64C0}, 	/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1, 0x00000000}, 	/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2, 0x3F800000}, 	/*3F800000,10D2,0dB,invert=0*/
	{0x10D3, 0x3F004DC0}, 	/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4, 0x3F800000}, 	/*3F800000,10D4,0dB,invert=0*/
	{0x10D5, 0x3F800000}, 	/*3F800000,10D5,0dB,invert=0*/
	{0x10D7, 0x40CD7480}, 	/*40CD7480,10D7,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x10D8, 0x40CD7480}, 	/*40CD7480,10D8,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x10D9, 0x3EDA5340}, 	/*3EDA5340,10D9,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x10DA, 0x3F5F3840}, 	/*3F5F3840,10DA,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x10DB, 0xBFD75900}, 	/*BFD75900,10DB,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x10DC, 0x3FD75900}, 	/*3FD75900,10DC,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x10DD, 0x3F5ACE00}, 	/*3F5ACE00,10DD,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x10DE, 0xBF3A0640}, 	/*BF3A0640,10DE,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x10E0, 0x3F7F6FC0}, 	/*3F7F6FC0,10E0,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x10E1, 0xBFFD6800}, 	/*BFFD6800,10E1,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x10E2, 0x3FFD6800}, 	/*3FFD6800,10E2,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x10E3, 0x3F7CB480}, 	/*3F7CB480,10E3,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x10E4, 0xBF7C2440}, 	/*BF7C2440,10E4,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x10E5, 0x3F800000}, 	/*3F800000,10E5,0dB,invert=0*/
	{0x10E8, 0x3F800000}, 	/*3F800000,10E8,0dB,invert=0*/
	{0x10E9, 0x00000000}, 	/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA, 0x00000000}, 	/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB, 0x00000000}, 	/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0, 0x3F800000}, 	/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1, 0x00000000}, 	/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2, 0x00000000}, 	/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3, 0x00000000}, 	/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4, 0x00000000}, 	/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5, 0x3F800000}, 	/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6, 0x00000000}, 	/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7, 0x00000000}, 	/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8, 0x00000000}, 	/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9, 0x00000000}, 	/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1100, 0x3F800000}, 	/*3F800000,1100,0dB,invert=0*/
	{0x1101, 0x3F800000}, 	/*3F800000,1101,0dB,invert=0*/
	{0x1102, 0x00000000}, 	/*00000000,1102,Cutoff,invert=0*/
	{0x1103, 0x3F800000}, 	/*3F800000,1103,0dB,invert=0*/
	{0x1104, 0x3DF21080}, 	/*3DF21080,1104,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1105, 0x3DF21080}, 	/*3DF21080,1105,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1106, 0x3F437BC0}, 	/*3F437BC0,1106,LPF,250Hz,0dB,fs/4,invert=0*/
	{0x1107, 0x00000000}, 	/*00000000,1107,Cutoff,invert=0*/
	{0x1108, 0x3F800000}, 	/*3F800000,1108,0dB,invert=0*/
	{0x1109, 0xBF800000}, 	/*BF800000,1109,0dB,invert=1*/
	{0x110A, 0x3F800000}, 	/*3F800000,110A,0dB,invert=0*/
	{0x110B, 0x3F800000}, 	/*3F800000,110B,0dB,invert=0*/
	{0x110C, 0x3F800000}, 	/*3F800000,110C,0dB,invert=0*/
	{0x110E, 0x3F800000}, 	/*3F800000,110E,0dB,invert=0*/
	{0x1110, 0x3DCCCCC0}, 	/*3DCCCCC0,1110*/
	{0x1111, 0x00000000}, 	/*00000000,1111,Free,fs/4,invert=0*/
	{0x1112, 0x3F7FF000}, 	/*3F7FF000,1112,Free,fs/4,invert=0*/
	{0x1113, 0x40D04F40}, 	/*40D04F40,1113,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1114, 0xC0C50240}, 	/*C0C50240,1114,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1115, 0x3F259600}, 	/*3F259600,1115,HBF,52Hz,400Hz,17.72dB,fs/4,invert=0*/
	{0x1116, 0x3F800400}, 	/*3F800400,1116,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1117, 0xBF7FCD00}, 	/*BF7FCD00,1117,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1118, 0x3F7FDD80}, 	/*3F7FDD80,1118,LBF,0.49Hz,0.837Hz,4.65dB,fs/4,invert=0*/
	{0x1119, 0x3F800000}, 	/*3F800000,1119,Through,0dB,fs/4,invert=0*/
	{0x111A, 0x00000000}, 	/*00000000,111A,Through,0dB,fs/4,invert=0*/
	{0x111B, 0x00000000}, 	/*00000000,111B,Through,0dB,fs/4,invert=0*/
	{0x111C, 0x00000000}, 	/*00000000,111C,Cutoff,invert=0*/
	{0x111D, 0x3F800000}, 	/*3F800000,111D,0dB,invert=0*/
	{0x111E, 0x3F800000}, 	/*3F800000,111E,0dB,invert=0*/
	{0x1120, 0x3F800000}, 	/*3F800000,1120,0dB,invert=0*/
	{0x1121, 0x3F800000}, 	/*3F800000,1121,0dB,invert=0*/
	{0x1122, 0x3F800000}, 	/*3F800000,1122,0dB,invert=0*/
	{0x1123, 0x3F800000}, 	/*3F800000,1123,Through,0dB,fs/1,invert=0*/
	{0x1124, 0x00000000}, 	/*00000000,1124,Through,0dB,fs/1,invert=0*/
	{0x1125, 0x00000000}, 	/*00000000,1125,Through,0dB,fs/1,invert=0*/
	{0x1126, 0x00000000}, 	/*00000000,1126,Through,0dB,fs/1,invert=0*/
	{0x1127, 0x00000000}, 	/*00000000,1127,Through,0dB,fs/1,invert=0*/
	{0x1130, 0x00000000}, 	/*00000000,1130,Free,fs/4,invert=0*/
	{0x1131, 0x3B031240}, 	/*3B031240,1131,Free,fs/4,invert=0*/
	{0x1132, 0x3F800000}, 	/*3F800000,1132,Free,fs/4,invert=0*/
	{0x1133, 0x3F800000}, 	/*3F800000,1133,Through,0dB,fs/4,invert=0*/
	{0x1134, 0x00000000}, 	/*00000000,1134,Through,0dB,fs/4,invert=0*/
	{0x1135, 0x00000000}, 	/*00000000,1135,Through,0dB,fs/4,invert=0*/
	{0x1136, 0x4F7FFFC0}, 	/*4F7FFFC0,1136,Free,fs/4,invert=0*/
	{0x1137, 0x00000000}, 	/*00000000,1137,Free,fs/4,invert=0*/
	{0x1138, 0x00000000}, 	/*00000000,1138,Free,fs/4,invert=0*/
	{0x1139, 0x00000000}, 	/*00000000,1139,Free,fs/4,invert=0*/
	{0x113A, 0x30800000}, 	/*30800000,113A,Free,fs/4,invert=0*/
	{0x113B, 0x00000000}, 	/*00000000,113B,Free,fs/4,invert=0*/
	{0x113C, 0x3F800000}, 	/*3F800000,113C,Through,0dB,fs/4,invert=0*/
	{0x113D, 0x00000000}, 	/*00000000,113D,Through,0dB,fs/4,invert=0*/
	{0x113E, 0x00000000}, 	/*00000000,113E,Through,0dB,fs/4,invert=0*/
	{0x1143, 0x3AD27C40}, 	/*3AD27C40,1143,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1144, 0x3AD27C40}, 	/*3AD27C40,1144,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1145, 0x3F7F2D80}, 	/*3F7F2D80,1145,LPF,3Hz,0dB,fs/4,invert=0*/
	{0x1146, 0x398C8300}, 	/*398C8300,1146,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1147, 0x398C8300}, 	/*398C8300,1147,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1148, 0x3F7FDCC0}, 	/*3F7FDCC0,1148,LPF,0.5Hz,0dB,fs/4,invert=0*/
	{0x1149, 0x3A0C7980}, 	/*3A0C7980,1149,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x114A, 0x3A0C7980}, 	/*3A0C7980,114A,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x114B, 0x3F7FB9C0}, 	/*3F7FB9C0,114B,LPF,1Hz,0dB,fs/4,invert=0*/
	{0x114C, 0x3A8C6640}, 	/*3A8C6640,114C,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x114D, 0x3A8C6640}, 	/*3A8C6640,114D,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x114E, 0x3F7F7380}, 	/*3F7F7380,114E,LPF,2Hz,0dB,fs/4,invert=0*/
	{0x1153, 0x3F800000}, 	/*3F800000,1153,Through,0dB,fs/4,invert=0*/
	{0x1154, 0x00000000}, 	/*00000000,1154,Through,0dB,fs/4,invert=0*/
	{0x1155, 0x00000000}, 	/*00000000,1155,Through,0dB,fs/4,invert=0*/
	{0x1156, 0x3F800000}, 	/*3F800000,1156,Through,0dB,fs/4,invert=0*/
	{0x1157, 0x00000000}, 	/*00000000,1157,Through,0dB,fs/4,invert=0*/
	{0x1158, 0x00000000}, 	/*00000000,1158,Through,0dB,fs/4,invert=0*/
	{0x1159, 0x3F800000}, 	/*3F800000,1159,Through,0dB,fs/4,invert=0*/
	{0x115A, 0x00000000}, 	/*00000000,115A,Through,0dB,fs/4,invert=0*/
	{0x115B, 0x00000000}, 	/*00000000,115B,Through,0dB,fs/4,invert=0*/
	{0x115C, 0x3F800000}, 	/*3F800000,115C,Through,0dB,fs/4,invert=0*/
	{0x115D, 0x00000000}, 	/*00000000,115D,Through,0dB,fs/4,invert=0*/
	{0x115E, 0x00000000}, 	/*00000000,115E,Through,0dB,fs/4,invert=0*/
	{0x1163, 0x3F800000}, 	/*3F800000,1163,0dB,invert=0*/
	{0x1166, 0x3F800000}, 	/*3F800000,1166,0dB,invert=0*/
	{0x1169, 0x3F800000}, 	/*3F800000,1169,0dB,invert=0*/
	{0x116C, 0x3F800000}, 	/*3F800000,116C,0dB,invert=0*/
	{0x1173, 0x00000000}, 	/*00000000,1173,Cutoff,invert=0*/
	{0x1176, 0x3F800000}, 	/*3F800000,1176,0dB,invert=0*/
	{0x1179, 0x3F800000}, 	/*3F800000,1179,0dB,invert=0*/
	{0x117C, 0x3F800000}, 	/*3F800000,117C,0dB,invert=0*/
	{0x1183, 0x38D1B700}, 	/*38D1B700,1183,-80dB,invert=0*/
	{0x1186, 0x00000000}, 	/*00000000,1186,Cutoff,invert=0*/
	{0x1189, 0x00000000}, 	/*00000000,1189,Cutoff,invert=0*/
	{0x118C, 0x00000000}, 	/*00000000,118C,Cutoff,invert=0*/
	{0x1193, 0x00000000}, 	/*00000000,1193,Cutoff,invert=0*/
	{0x1198, 0x3F800000}, 	/*3F800000,1198,0dB,invert=0*/
	{0x1199, 0x3F800000}, 	/*3F800000,1199,0dB,invert=0*/
	{0x119A, 0x3F800000}, 	/*3F800000,119A,0dB,invert=0*/
	{0x11A1, 0x3BDA2580}, 	/*3BDA2580,11A1,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11A2, 0x3BDA2580}, 	/*3BDA2580,11A2,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11A3, 0x3F7C9780}, 	/*3F7C9780,11A3,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11A4, 0x00000000}, 	/*00000000,11A4,Free,fs/1,invert=0*/
	{0x11A5, 0x3A031240}, 	/*3A031240,11A5,Free,fs/1,invert=0*/
	{0x11A6, 0x3F800000}, 	/*3F800000,11A6,Free,fs/1,invert=0*/
	{0x11A7, 0x3F800000}, 	/*3F800000,11A7,Through,0dB,fs/4,invert=0*/
	{0x11A8, 0x00000000}, 	/*00000000,11A8,Through,0dB,fs/4,invert=0*/
	{0x11A9, 0x00000000}, 	/*00000000,11A9,Through,0dB,fs/4,invert=0*/
	{0x11AA, 0x00000000}, 	/*00000000,11AA,Cutoff,invert=0*/
	{0x11AB, 0x3BDA2580}, 	/*3BDA2580,11AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11AC, 0x3BDA2580}, 	/*3BDA2580,11AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11AD, 0x3F7C9780}, 	/*3F7C9780,11AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x11B0, 0x3F800000}, 	/*3F800000,11B0,Through,0dB,fs/1,invert=0*/
	{0x11B1, 0x00000000}, 	/*00000000,11B1,Through,0dB,fs/1,invert=0*/
	{0x11B2, 0x00000000}, 	/*00000000,11B2,Through,0dB,fs/1,invert=0*/
	{0x11B3, 0x3F800000}, 	/*3F800000,11B3,0dB,invert=0*/
	{0x11B4, 0x00000000}, 	/*00000000,11B4,Cutoff,invert=0*/
	{0x11B5, 0x00000000}, 	/*00000000,11B5,Cutoff,invert=0*/
	{0x11B6, 0x3F353C00}, 	/*3F353C00,11B6,-3dB,invert=0*/
	{0x11B8, 0x3F800000}, 	/*3F800000,11B8,0dB,invert=0*/
	{0x11B9, 0x00000000}, 	/*00000000,11B9,Cutoff,invert=0*/
	{0x11C0, 0x3FD13600}, 	/*3FD13600,11C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x11C1, 0xBFCEFAC0}, 	/*BFCEFAC0,11C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x11C2, 0x3F5414C0}, 	/*3F5414C0,11C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x11C3, 0x3F800000}, 	/*3F800000,11C3,Through,0dB,fs/1,invert=0*/
	{0x11C4, 0x00000000}, 	/*00000000,11C4,Through,0dB,fs/1,invert=0*/
	{0x11C5, 0x00000000}, 	/*00000000,11C5,Through,0dB,fs/1,invert=0*/
	{0x11C6, 0x3D506F00}, 	/*3D506F00,11C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x11C7, 0x3D506F00}, 	/*3D506F00,11C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x11C8, 0x3F65F240}, 	/*3F65F240,11C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x11C9, 0x3C1CFA80}, 	/*3C1CFA80,11C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x11CA, 0x3C1CFA80}, 	/*3C1CFA80,11CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x11CB, 0x3F7FF040}, 	/*3F7FF040,11CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x11CC, 0x3E300080}, 	/*3E300080,11CC,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x11CD, 0xBE2EC780}, 	/*BE2EC780,11CD,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x11CE, 0x3F7FA840}, 	/*3F7FA840,11CE,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x11D0, 0x3FFF64C0}, 	/*3FFF64C0,11D0,6dB,invert=0*/
	{0x11D1, 0x00000000}, 	/*00000000,11D1,Cutoff,invert=0*/
	{0x11D2, 0x3F800000}, 	/*3F800000,11D2,0dB,invert=0*/
	{0x11D3, 0x3F004DC0}, 	/*3F004DC0,11D3,-6dB,invert=0*/
	{0x11D4, 0x3F800000}, 	/*3F800000,11D4,0dB,invert=0*/
	{0x11D5, 0x3F800000}, 	/*3F800000,11D5,0dB,invert=0*/
	{0x11D7, 0x40CD7480}, 	/*40CD7480,11D7,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x11D8, 0x40CD7480}, 	/*40CD7480,11D8,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x11D9, 0x3EDA5340}, 	/*3EDA5340,11D9,LPF,3000Hz,27dB,fs/1,invert=0*/
	{0x11DA, 0x3F5F3840}, 	/*3F5F3840,11DA,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x11DB, 0xBFD75900}, 	/*BFD75900,11DB,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x11DC, 0x3FD75900}, 	/*3FD75900,11DC,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x11DD, 0x3F5ACE00}, 	/*3F5ACE00,11DD,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x11DE, 0xBF3A0640}, 	/*BF3A0640,11DE,PKF,850Hz,-24dB,5,fs/1,invert=0*/
	{0x11E0, 0x3F7F6FC0}, 	/*3F7F6FC0,11E0,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x11E1, 0xBFFD6800}, 	/*BFFD6800,11E1,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x11E2, 0x3FFD6800}, 	/*3FFD6800,11E2,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x11E3, 0x3F7CB480}, 	/*3F7CB480,11E3,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x11E4, 0xBF7C2440}, 	/*BF7C2440,11E4,PKF,270Hz,-3dB,7,fs/1,invert=0*/
	{0x11E5, 0x3F800000}, 	/*3F800000,11E5,0dB,invert=0*/
	{0x11E8, 0x3F800000}, 	/*3F800000,11E8,0dB,invert=0*/
	{0x11E9, 0x00000000}, 	/*00000000,11E9,Cutoff,invert=0*/
	{0x11EA, 0x00000000}, 	/*00000000,11EA,Cutoff,invert=0*/
	{0x11EB, 0x00000000}, 	/*00000000,11EB,Cutoff,invert=0*/
	{0x11F0, 0x3F800000}, 	/*3F800000,11F0,Through,0dB,fs/1,invert=0*/
	{0x11F1, 0x00000000}, 	/*00000000,11F1,Through,0dB,fs/1,invert=0*/
	{0x11F2, 0x00000000}, 	/*00000000,11F2,Through,0dB,fs/1,invert=0*/
	{0x11F3, 0x00000000}, 	/*00000000,11F3,Through,0dB,fs/1,invert=0*/
	{0x11F4, 0x00000000}, 	/*00000000,11F4,Through,0dB,fs/1,invert=0*/
	{0x11F5, 0x3F800000}, 	/*3F800000,11F5,Through,0dB,fs/1,invert=0*/
	{0x11F6, 0x00000000}, 	/*00000000,11F6,Through,0dB,fs/1,invert=0*/
	{0x11F7, 0x00000000}, 	/*00000000,11F7,Through,0dB,fs/1,invert=0*/
	{0x11F8, 0x00000000}, 	/*00000000,11F8,Through,0dB,fs/1,invert=0*/
	{0x11F9, 0x00000000}, 	/*00000000,11F9,Through,0dB,fs/1,invert=0*/
	{0x1200, 0x00000000}, 	/*00000000,1200,Cutoff,invert=0*/
	{0x1201, 0x3F800000}, 	/*3F800000,1201,0dB,invert=0*/
	{0x1202, 0x3F800000}, 	/*3F800000,1202,0dB,invert=0*/
	{0x1203, 0x3F800000}, 	/*3F800000,1203,0dB,invert=0*/
	{0x1204, 0x41842380}, 	/*41842380,1204,HBF,40Hz,1600Hz,26dB,fs/1,invert=0*/
	{0x1205, 0xC182BA80}, 	/*C182BA80,1205,HBF,40Hz,1600Hz,26dB,fs/1,invert=0*/
	{0x1206, 0x3F259600}, 	/*3F259600,1206,HBF,40Hz,1600Hz,26dB,fs/1,invert=0*/
	{0x1207, 0x3E0DE280}, 	/*3E0DE280,1207,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1208, 0x3E0DE280}, 	/*3E0DE280,1208,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1209, 0x3F390EC0}, 	/*3F390EC0,1209,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x120A, 0x3D506F00}, 	/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B, 0x3D506F00}, 	/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C, 0x3F65F240}, 	/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D, 0x3E224B00}, 	/*3E224B00,120D,DI,-16dB,fs/16,invert=0*/
	{0x120E, 0x00000000}, 	/*00000000,120E,DI,-16dB,fs/16,invert=0*/
	{0x120F, 0x3F800000}, 	/*3F800000,120F,DI,-16dB,fs/16,invert=0*/
	{0x1210, 0x3EC662C0}, 	/*3EC662C0,1210,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1211, 0xBEC4BE80}, 	/*BEC4BE80,1211,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1212, 0x3F7F96C0}, 	/*3F7F96C0,1212,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1213, 0x3FFF64C0}, 	/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214, 0x00000000}, 	/*00000000,1214,Cutoff,invert=0*/
	{0x1215, 0x407ECA00}, 	/*407ECA00,1215,12dB,invert=0*/
	{0x1216, 0x3F004DC0}, 	/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217, 0x3F800000}, 	/*3F800000,1217,0dB,invert=0*/
	{0x1218, 0x3F764500}, 	/*3F764500,1218,PKF,350Hz,-10dB,3,fs/1,invert=0*/
	{0x1219, 0xBFF0B500}, 	/*BFF0B500,1219,PKF,350Hz,-10dB,3,fs/1,invert=0*/
	{0x121A, 0x3FF0B500}, 	/*3FF0B500,121A,PKF,350Hz,-10dB,3,fs/1,invert=0*/
	{0x121B, 0x3F6D4500}, 	/*3F6D4500,121B,PKF,350Hz,-10dB,3,fs/1,invert=0*/
	{0x121C, 0xBF6389C0}, 	/*BF6389C0,121C,PKF,350Hz,-10dB,3,fs/1,invert=0*/
	{0x121D, 0x3F800000}, 	/*3F800000,121D,0dB,invert=0*/
	{0x121E, 0x3F800000}, 	/*3F800000,121E,0dB,invert=0*/
	{0x121F, 0x3F800000}, 	/*3F800000,121F,0dB,invert=0*/
	{0x1235, 0x3F800000}, 	/*3F800000,1235,0dB,invert=0*/
	{0x1236, 0x3F800000}, 	/*3F800000,1236,0dB,invert=0*/
	{0x1237, 0x3F800000}, 	/*3F800000,1237,0dB,invert=0*/
	{0x1238, 0x3F800000}, 	/*3F800000,1238,0dB,invert=0*/
	{0xFFFF, 0xFFFFFFFF}
};

int IniSet_IMT(uint8_t ver)
{
	if (OnsemiI2CCheck_IMT() == 0)
		return OIS_FW_POLLING_FAIL;

	E2pDat_IMT(ver);
	if (VerInf_IMT() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_VERSION_FAIL;

	//Clock setting
	IniClk_IMT();

	//CL-AF Initial Setting
	IniClAf_IMT();

	//I/O Port initial setting
	IniIop_IMT();
	//DigitalGyro Initial setting
	if (IniDgy_IMT() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;
	// Monitor & Other Initial Setting
	IniMon_IMT();
	// Servo Initial Setting
	IniSrv_IMT();
	// Gyro Filter Initial Setting
	IniGyr_IMT();
	// Gyro Filter Initial Setting
	if (IniFil_IMT() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;
	StmvSetClAf_IMT();
	// Adjust Fix Value Setting
	IniAdj_IMT();

	return OIS_FW_POLLING_PASS;
}

//********************************************************************************
// Function Name 	: WitTim
// Retun Value		: NON
// Argment Value	: Wait Time(ms)
// Explanation		: Timer Wait Function
// History			: First edition 						2009.07.31 Y.Tashita
//********************************************************************************
void WitTim(uint16_t	UsWitTim)
{
#if 1
	usleep_range(UsWitTim * 1000, UsWitTim * 1000 + 10);
#else
	uint32_t	UlLopIdx, UlWitCyc;

	UlWitCyc	= (uint32_t)((float)UsWitTim / NOP_TIME / (float)12);

	for(UlLopIdx = 0; UlLopIdx < UlWitCyc; UlLopIdx++)
	{
		;
	}
#endif
}

void MemClr_IMT(uint8_t *NcTgtPtr, uint16_t UsClrSiz)
{
	uint16_t UsClrIdx;

	for (UsClrIdx = 0; UsClrIdx < UsClrSiz; UsClrIdx++) {
		*NcTgtPtr = 0;
		NcTgtPtr++;
	}

	return;
}

//**************************
//	Global Variable
//**************************
int OnsemiI2CCheck_IMT(void)
{
	uint8_t UcLsiVer;
	RegReadA(CVER, &UcLsiVer);		//0x027E, Ver Check
	return (UcLsiVer == CVER122A) ? 1 : 0;	//LC898122A
}

//********************************************************************************
// Function Name 	: E2pDat_IMT
// Return Value		: NON
// Argument Value	: NON
// Explanation		: E2PROM Calibration Data Read Function
// History			: First edition 						2015.06.19 JH.Lim
//********************************************************************************
void E2pDat_IMT(uint8_t ver)
{
	MemClr_IMT((uint8_t *)&StCalDat_imt, sizeof(stCalDat));

	E2pRed((uint16_t)HALL_BIAS_Z, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlzGan);
	E2pRed((uint16_t)HALL_OFFSET_Z, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlzOff);
	E2pRed((uint16_t)HALL_ADOFFSET_Z, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsAdzOff);
	E2pRed((uint16_t)LOOP_GAIN_Z, 2, (uint8_t *)&StCalDat_imt.StLopGan.UsLzgVal);
	E2pRed((uint16_t)DRV_OFFSET_Z, 1, (uint8_t *)&StCalDat_imt.StAfOff.UcAfOff);
	E2pRed((uint16_t)ADJ_HALL_Z_FLG, 2, (uint8_t *)&StCalDat_imt.UsAdjHallZF);
	E2pRed((uint16_t)HALL_BIAS_X, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlxGan);
	E2pRed((uint16_t)HALL_BIAS_Y, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlyGan);

	E2pRed((uint16_t)HALL_OFFSET_X, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlxOff);
	E2pRed((uint16_t)HALL_OFFSET_Y, 2, (uint8_t *)&StCalDat_imt.StHalAdj.UsHlyOff);

	E2pRed((uint16_t)LOOP_GAIN_X, 2, (uint8_t *)&StCalDat_imt.StLopGan.UsLxgVal);
	E2pRed((uint16_t)LOOP_GAIN_Y, 2, (uint8_t *)&StCalDat_imt.StLopGan.UsLygVal);

	E2pRed((uint16_t)LENS_CENTER_FINAL_X, 2, (uint8_t *)&StCalDat_imt.StLenCen.UsLsxVal);
	E2pRed((uint16_t)LENS_CENTER_FINAL_Y, 2, (uint8_t *)&StCalDat_imt.StLenCen.UsLsyVal);

	E2pRed((uint16_t)GYRO_AD_OFFSET_X, 	2, (uint8_t *)&StCalDat_imt.StGvcOff.UsGxoVal);
	E2pRed((uint16_t)GYRO_AD_OFFSET_Y, 	2, (uint8_t *)&StCalDat_imt.StGvcOff.UsGyoVal);

	E2pRed((uint16_t)OSC_CLK_VAL, 1, (uint8_t *)&StCalDat_imt.UcOscVal);

	E2pRed((uint16_t)ADJ_HALL_FLAG, 2, (uint8_t *)&StCalDat_imt.UsAdjHallF);
	E2pRed((uint16_t)ADJ_GYRO_FLAG, 2, (uint8_t *)&StCalDat_imt.UsAdjGyroF);
	E2pRed((uint16_t)ADJ_LENS_FLAG, 2, (uint8_t *)&StCalDat_imt.UsAdjLensF);

	E2pRed((uint16_t)GYRO_GAIN_X, 4, (uint8_t *)&StCalDat_imt.UlGxgVal);
	E2pRed((uint16_t)GYRO_GAIN_Y, 4, (uint8_t *)&StCalDat_imt.UlGygVal);

	E2pRed((uint16_t)FW_VERSION_INFO, 2, (uint8_t *)&StCalDat_imt.UsVerDat);

	return;
}

//********************************************************************************
// Function Name 	: VerInf_IMT
// Return Value		: Vesion check result
// Argument Value	: NON
// Explanation		: F/W Version Check
// History			: First edition 						2015.06.19 JH.Lim
//********************************************************************************
int VerInf_IMT(void)
{
	UcVerHig_imt = (uint8_t)(StCalDat_imt.UsVerDat >> 8);		// System Version
	UcVerLow_imt = (uint8_t)(StCalDat_imt.UsVerDat) ;			// Filter Version

	if (UcVerHig_imt == 0xC0) {							// 0xC0 CVL
		UcVerHig_imt = 0x00;
	} else if (UcVerHig_imt == 0xC1) {					// 0xC1 PWM
		UcVerHig_imt = 0x01;
	} else {
		return OIS_FW_POLLING_VERSION_FAIL;
	}

	if (UcVerLow_imt == 0x13) {					// 0x13 MTM Act.
		UcVerLow_imt = 0x13;
	} else {
		return OIS_FW_POLLING_VERSION_FAIL;
	};

	return OIS_FW_POLLING_PASS;
}

//********************************************************************************
// Function Name 	: IniClk
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Clock Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void IniClk_IMT(void)
{
	RegReadA(CVER, &UcCvrCod_imt);		// 0x027E
	RegWriteA(MDLREG, MDL_VER);			// 0x00FF	Model
	RegWriteA(VRREG, FW_VER);			// 0x02D0	Version

	/*OSC ENABLE*/
#ifdef DEF_SET
	RegWriteA(OSCSTOP, 0x00);			// 0x0256
	RegWriteA(OSCSET, 0x90);			// 0x0257	OSC ini
	RegWriteA(OSCCNTEN, 0x00);			// 0x0258	OSC Cnt disable
#endif

	/*Clock Enables*/
	RegWriteA(CLKON, 0x1F);			// 0x020B

#ifdef DEF_SET
	RegWriteA(CLKSEL, 0x00);			// 0x020C
#endif

#ifdef DEF_SET
	RegWriteA(PWMDIV, 0x00);			// 0x0210	48MHz/1
	RegWriteA(SRVDIV, 0x00);			// 0x0211	48MHz/1
	RegWriteA(GIFDIV, 0x03);			// 0x0212	48MHz/3 = 16MHz
	RegWriteA(AFPWMDIV, 0x02);			// 0x0213	48MHz/2 = 24MHz
	RegWriteA(OPAFDIV, 0x04);			// 0x0214	48MHz/4 = 12MHz
#endif
}

//********************************************************************************
// Function Name 	: IniIop
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: I/O Port Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void IniIop_IMT(void)
{
#ifdef DEF_SET
	/*set IOP direction*/
	RegWriteA(P0LEV, 0x00);	// 0x0220	[ - 	| - 	| WLEV5 | WLEV4 ][ WLEV3 | WLEV2 | WLEV1 | WLEV0 ]
	RegWriteA(P0DIR, 0x00);	// 0x0221	[ - 	| - 	| DIR5	| DIR4	][ DIR3  | DIR2  | DIR1  | DIR0  ]
	/*set pull up/down*/
	RegWriteA(P0PON, 0x0F);	// 0x0222	[ -    | -	  | PON5 | PON4 ][ PON3  | PON2 | PON1 | PON0 ]
	RegWriteA(P0PUD, 0x0F);	// 0x0223	[ -    | -	  | PUD5 | PUD4 ][ PUD3  | PUD2 | PUD1 | PUD0 ]
#endif

	RegWriteA(IOP1SEL, 0x00); 	// 0x0231	IOP1 : DGDATAIN (ATT:0236h[0]=1)

#ifdef DEF_SET
	RegWriteA(IOP0SEL, 0x02); 	// 0x0230	IOP0 : IOP0
	RegWriteA(IOP2SEL, 0x02); 	// 0x0232	IOP2 : IOP2
	RegWriteA(IOP3SEL, 0x00); 	// 0x0233	IOP3 : DGDATAOUT
	RegWriteA(IOP4SEL, 0x00); 	// 0x0234	IOP4 : DGSCLK
	RegWriteA(IOP5SEL, 0x00); 	// 0x0235	IOP5 : DGSSB
	RegWriteA(DGINSEL, 0x00); 	// 0x0236	DGDATAIN 0:IOP1 1:IOP2
	RegWriteA(I2CSEL, 0x00);		// 0x0248	I2C noise reduction ON
	RegWriteA(DLMODE, 0x00);		// 0x0249	Download OFF
#endif
}


//********************************************************************************
// Function Name 	: GyOutSignal
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void GyOutSignal_IMT(void)
{
	RegWriteA(GRADR0, GYROX_INI);			// 0x0283	Set Gyro XOUT H~L
	RegWriteA(GRADR1, GYROY_INI);			// 0x0284	Set Gyro YOUT H~L

	/*Start OIS Reading*/
	RegWriteA(GRSEL, 0x02);			// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]
}

//********************************************************************************
// Function Name 	: IniDgy
// Return Value		: NON
// Argument Value	: NON
// Explanation		: Digital Gyro Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
int IniDgy_IMT(void)
{
	uint8_t	UcGrini;

	/*Set SPI Type*/
	RegWriteA(SPIM, 0x01);					// 0x028F 	[ - | - | - | - ][ - | - | - | DGSPI4 ]

	/*Set to Command Mode*/
	RegWriteA(GRSEL, 0x01);					// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	/*Digital Gyro Read settings*/
	RegWriteA(GRINI, 0x80);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegReadA(GRINI, &UcGrini);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA(GRINI, (uint8_t)(UcGrini | SLOWMODE));		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegWriteA(GRADR0, 0x6B);					// 0x0283	Set CLKSEL
	RegWriteA(GSETDT, 0x01);					// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (AccWit_IMT(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait 				*/

	RegWriteA(GRADR0, 0x1B);					// 0x0283	Set FS_SEL
	RegWriteA(GSETDT, (FS_SEL << 3));			// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (AccWit_IMT(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait 				*/

	RegWriteA(GRADR0, 0x6A);					// 0x0283	Set I2C_DIS
	RegWriteA(GSETDT, 0x11);					// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (AccWit_IMT(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait 				*/

	RegReadA(GRINI, &UcGrini);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA(GRINI, (uint8_t)(UcGrini & ~SLOWMODE));		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegWriteA(RDSEL, 0x7C);					// 0x028B	RDSEL(Data1 and 2 for continuos mode)

	GyOutSignal_IMT();

	return OIS_FW_POLLING_PASS;
}

//********************************************************************************
// Function Name 	: AccWit
// Return Value		: NON
// Argument Value	: Trigger Register Data
// Explanation		: Acc Wait Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
int	AccWit_IMT(uint8_t UcTrgDat)
{
	uint8_t	UcFlgVal;
	UcFlgVal	= 1;

	while (UcFlgVal) {
		RegReadA(GRACC, &UcFlgVal);			// 0x0282
		UcFlgVal &= UcTrgDat;
		if (CmdRdChk_IMT() != 0)
			return OIS_FW_POLLING_FAIL;
	}

	return OIS_FW_POLLING_PASS;
}

//********************************************************************************
// Function Name 	: CmdRdChk
// Retun Value		: 1 : ERROR
// Argment Value	: NON
// Explanation		: Check Cver function
// History			: First edition 						2014.02.27 K.abe
//********************************************************************************
int CmdRdChk_IMT(void)
{
	uint8_t UcTestRD;
	uint8_t UcCount;

	for (UcCount = 0; UcCount < READ_COUNT_NUM; UcCount++) {
		RegReadA(TESTRD, &UcTestRD);					// 0x027F
		if (UcTestRD == 0xAC) {
			return(0);
		}
	}
	return(1);
}

//********************************************************************************
// Function Name 	: IniSrv
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Servo Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void IniSrv_IMT()
{
	uint8_t	UcStbb0;

	UcPwmMod_imt = INIT_PWMMODE;					// Driver output mode

	RegWriteA(WC_EQON, 0x00);				// 0x0101		Filter Calcu
	RegWriteA(WC_RAMINITON,0x00);				// 0x0102
	ClrGyr_IMT(0x0000, CLR_ALL_RAM);					// All Clear

	RegWriteA(WH_EQSWX, 0x02);				// 0x0170		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	RegWriteA(WH_EQSWY, 0x02);				// 0x0171		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]

	RamAccFixMod_IMT(OFF);							// 32bit Float mode

	/* Monitor Gain */
	RamWrite32A(dm1g, 0x3F800000);			// 0x109A
	RamWrite32A(dm2g, 0x3F800000);			// 0x109B
	RamWrite32A(dm3g, 0x3F800000);			// 0x119A
	RamWrite32A(dm4g, 0x3F800000);			// 0x119B

	/* Hall output limitter */
	RamWrite32A(sxlmta1, 0x3F800000);			// 0x10E6		Hall X output Limit
	RamWrite32A(sylmta1, 0x3F800000);			// 0x11E6		Hall Y output Limit
	RamWrite32A(aflmt, 0x3F533333);				// 0x1220

	/* Emargency Stop */
	RegWriteA(WH_EMGSTPON, 0x00);				// 0x0178		Emargency Stop OFF
	RegWriteA(WH_EMGSTPTMR, 0xFF);				// 0x017A		255*(16/23.4375kHz)=174ms

	RamWrite32A(sxemglev, 0x3F800000);			// 0x10EC		Hall X Emargency threshold
	RamWrite32A(syemglev, 0x3F800000);			// 0x11EC		Hall Y Emargency threshold

	/* Hall Servo smoothing */
	RegWriteA(WH_SMTSRVON, 0x00);				// 0x017C		Smooth Servo OFF
	RegWriteA(WH_SMTSRVSMP,0x06);				// 0x017D		2.7ms=2^06/23.4375kHz
	RegWriteA(WH_SMTTMR, 0x01);				// 0x017E		1.3ms=(1+1)*16/23.4375kHz

	RamWrite32A(sxsmtav, 0xBC800000);			// 0x10ED		1/64 X smoothing ave coefficient
	RamWrite32A(sysmtav, 0xBC800000);			// 0x11ED		1/64 Y smoothing ave coefficient
	RamWrite32A(sxsmtstp, 0x3AE90466);			// 0x10EE		0.001778 X smoothing offset
	RamWrite32A(sysmtstp, 0x3AE90466);			// 0x11EE		0.001778 Y smoothing offset

	/* High-dimensional correction  */
	RegWriteA(WH_HOFCON, 0x11);				// 0x0174		OUT 3x3

	/* Front */
	RamWrite32A(sxiexp3, A3_IEXP3);			// 0x10BA
	RamWrite32A(sxiexp2, 0x00000000);			// 0x10BB
	RamWrite32A(sxiexp1, A1_IEXP1);			// 0x10BC
	RamWrite32A(sxiexp0, 0x00000000);			// 0x10BD
	RamWrite32A(sxiexp, 0x3F800000);			// 0x10BE

	RamWrite32A(syiexp3, A3_IEXP3);			// 0x11BA
	RamWrite32A(syiexp2, 0x00000000);			// 0x11BB
	RamWrite32A(syiexp1, A1_IEXP1);			// 0x11BC
	RamWrite32A(syiexp0, 0x00000000);			// 0x11BD
	RamWrite32A(syiexp, 0x3F800000);			// 0x11BE

	/* Back */
	RamWrite32A(sxoexp3, A3_IEXP3);			// 0x10FA
	RamWrite32A(sxoexp2, 0x00000000);			// 0x10FB
	RamWrite32A(sxoexp1, A1_IEXP1);			// 0x10FC
	RamWrite32A(sxoexp0, 0x00000000);			// 0x10FD
	RamWrite32A(sxoexp, 0x3F800000);			// 0x10FE

	RamWrite32A(syoexp3, A3_IEXP3);			// 0x11FA
	RamWrite32A(syoexp2, 0x00000000);			// 0x11FB
	RamWrite32A(syoexp1, A1_IEXP1);			// 0x11FC
	RamWrite32A(syoexp0, 0x00000000);			// 0x11FD
	RamWrite32A(syoexp, 0x3F800000);			// 0x11FE

	/* Sine wave */
#ifdef DEF_SET
	RegWriteA(WC_SINON, 0x00);				// 0x0180		Sin Wave off
	RegWriteA(WC_SINFRQ0, 0x00);				// 0x0181
	RegWriteA(WC_SINFRQ1, 0x60);				// 0x0182
	RegWriteA(WC_SINPHSX, 0x00);				// 0x0183
	RegWriteA(WC_SINPHSY, 0x00);				// 0x0184

	/* AD over sampling */
	RegWriteA(WC_ADMODE, 0x06);				// 0x0188		AD Over Sampling

	/* Measure mode */
	RegWriteA(WC_MESMODE, 0x00);				// 0x0190		Measurement Mode
	RegWriteA(WC_MESSINMODE, 0x00);				// 0x0191
	RegWriteA(WC_MESLOOP0, 0x08);				// 0x0192
	RegWriteA(WC_MESLOOP1, 0x02);				// 0x0193
	RegWriteA(WC_MES1ADD0, 0x00);				// 0x0194
	RegWriteA(WC_MES1ADD1, 0x00);				// 0x0195
	RegWriteA(WC_MES2ADD0, 0x00);				// 0x0196
	RegWriteA(WC_MES2ADD1, 0x00);				// 0x0197
	RegWriteA(WC_MESABS, 0x00);				// 0x0198
	RegWriteA(WC_MESWAIT, 0x00);				// 0x0199

	/* auto measure */
	RegWriteA(WC_AMJMODE, 0x00);				// 0x01A0		Automatic measurement mode

	RegWriteA(WC_AMJLOOP0, 0x08);				// 0x01A2		Self-Aadjustment
	RegWriteA(WC_AMJLOOP1, 0x02);				// 0x01A3
	RegWriteA(WC_AMJIDL0, 0x02);				// 0x01A4
	RegWriteA(WC_AMJIDL1, 0x00);				// 0x01A5
	RegWriteA(WC_AMJ1ADD0, 0x00);				// 0x01A6
	RegWriteA(WC_AMJ1ADD1, 0x00);				// 0x01A7
	RegWriteA(WC_AMJ2ADD0, 0x00);				// 0x01A8
	RegWriteA(WC_AMJ2ADD1, 0x00);				// 0x01A9
#endif

#ifdef CATCHMODE
	RegWriteA(WC_DPI1ADD0, 0x3B);				// 0x01B0		Data Pass
	RegWriteA(WC_DPI1ADD1, 0x00);				// 0x01B1
	RegWriteA(WC_DPI2ADD0, 0xBB);				// 0x01B2
	RegWriteA(WC_DPI2ADD1, 0x00);				// 0x01B3
	RegWriteA(WC_DPI3ADD0, 0x38);				// 0x01B4
	RegWriteA(WC_DPI3ADD1, 0x00);				// 0x01B5
	RegWriteA(WC_DPI4ADD0, 0xB8);				// 0x01B6
	RegWriteA(WC_DPI4ADD1, 0x00);				// 0x01B7
	RegWriteA(WC_DPO1ADD0, 0x31);				// 0x01B8		Data Pass
	RegWriteA(WC_DPO1ADD1, 0x00);				// 0x01B9
	RegWriteA(WC_DPO2ADD0, 0xB1);				// 0x01BA
	RegWriteA(WC_DPO2ADD1, 0x00);				// 0x01BB
	RegWriteA(WC_DPO3ADD0, 0x3A);				// 0x01BC
	RegWriteA(WC_DPO3ADD1, 0x00);				// 0x01BD
	RegWriteA(WC_DPO4ADD0, 0xBA);				// 0x01BE
	RegWriteA(WC_DPO4ADD1, 0x00);				// 0x01BF
	RegWriteA(WC_DPON, 0x0F);				// 0x0105		Data pass ON
#else
#ifdef DEF_SET
	/* Data Pass */
	RegWriteA(WC_DPI1ADD0, 0x00);			// 0x01B0		Data Pass
	RegWriteA(WC_DPI1ADD1, 0x00);			// 0x01B1
	RegWriteA(WC_DPI2ADD0, 0x00);			// 0x01B2
	RegWriteA(WC_DPI2ADD1, 0x00);			// 0x01B3
	RegWriteA(WC_DPI3ADD0, 0x00);			// 0x01B4
	RegWriteA(WC_DPI3ADD1, 0x00);			// 0x01B5
	RegWriteA(WC_DPI4ADD0, 0x00);			// 0x01B6
	RegWriteA(WC_DPI4ADD1, 0x00);			// 0x01B7
	RegWriteA(WC_DPO1ADD0, 0x00);			// 0x01B8		Data Pass
	RegWriteA(WC_DPO1ADD1, 0x00);			// 0x01B9
	RegWriteA(WC_DPO2ADD0, 0x00);			// 0x01BA
	RegWriteA(WC_DPO2ADD1, 0x00);			// 0x01BB
	RegWriteA(WC_DPO3ADD0, 0x00);			// 0x01BC
	RegWriteA(WC_DPO3ADD1, 0x00);			// 0x01BD
	RegWriteA(WC_DPO4ADD0, 0x00);			// 0x01BE
	RegWriteA(WC_DPO4ADD1, 0x00);			// 0x01BF
	RegWriteA(WC_DPON, 0x00);			// 0x0105		Data pass OFF
#endif
#endif
	/* Interrupt Flag */
	RegWriteA(WC_INTMSK, 0xFF);				// 0x01CE		All Mask

	/* Ram Access */
	RamAccFixMod_IMT(OFF);							// 32bit float mode

	// PWM Signal Generate
	DrvSw_IMT(OFF);									/* 0x0070	Drvier Block Ena=0 */
	RegWriteA(DRVFC2, 0x90);					// 0x0002	Slope 3, Dead Time = 30 ns
	RegWriteA(DRVSELX, 0xFF);					// 0x0003	PWM X drv max current  DRVSELX[7:0]
	RegWriteA(DRVSELY, 0xFF);					// 0x0004	PWM Y drv max current  DRVSELY[7:0]

#ifdef	PWM_BREAK
	if (UcCvrCod_imt == CVER122) {
		RegWriteA(PWMFC,   0x2D);					// 0x0011	VREF, PWMCLK/256, MODE0B, 12Bit Accuracy
	} else {
		RegWriteA(PWMFC,   0x3D);					// 0x0011	VREF, PWMCLK/128, MODE0B, 12Bit Accuracy
	}
#else
	if (UcCvrCod_imt == CVER122) {
		RegWriteA(PWMFC,   0x29);					// 0x0011	VREF, PWMCLK/256, MODE0S, 12Bit Accuracy
	} else {
		RegWriteA(PWMFC,   0x39);					// 0x0011	VREF, PWMCLK/128, MODE0S, 12Bit Accuracy
	}
#endif


	RegWriteA(PWMA, 0x00);					// 0x0010	PWM X/Y standby
	RegWriteA(PWMDLYX, 0x04);					// 0x0012	X Phase Delay Setting
	RegWriteA(PWMDLYY, 0x04);					// 0x0013	Y Phase Delay Setting

	if (CRS_OX_OY) {
		RegWriteA(DRVCH1SEL, 0x02);				// 0x0005	OUT1/OUT2	Y axis
		RegWriteA(DRVCH2SEL, 0x02);				// 0x0006	OUT3/OUT4	X axis
	} else {
		RegWriteA(DRVCH1SEL, 0x00);				// 0x0005	OUT1/OUT2	X axis
		RegWriteA(DRVCH2SEL, 0x00);				// 0x0006	OUT3/OUT4	Y axis
	}

#ifdef DEF_SET
	RegWriteA(PWMDLYTIMX, 0x00);				// 0x0014		PWM Timing
	RegWriteA(PWMDLYTIMY, 0x00);				// 0x0015		PWM Timing
#endif

	if (UcCvrCod_imt == CVER122) {
		RegWriteA(PWMPERIODY, 0x00);				// 0x001A		PWM Carrier Freq
		RegWriteA(PWMPERIODY2, 0x00);				// 0x001B		PWM Carrier Freq
	} else {
		RegWriteA(PWMPERIODX, 0x00);				// 0x0018		PWM Carrier Freq
		RegWriteA(PWMPERIODX2, 0x00);				// 0x0019		PWM Carrier Freq
		RegWriteA(PWMPERIODY, 0x00);				// 0x001A		PWM Carrier Freq
		RegWriteA(PWMPERIODY2, 0x00);				// 0x001B		PWM Carrier Freq
	}

	/* Linear PWM circuit setting */
	RegWriteA(CVA, 0xC0);			// 0x0020	Linear PWM mode enable

	if (UcCvrCod_imt == CVER122) {
		RegWriteA(CVFC, 0x22);			// 0x0021
	}

#ifdef PWM_BREAK
	RegWriteA(CVFC2, 0x80);			// 0x0022
#else
	RegWriteA(CVFC2, 0x00);			// 0x0022
#endif
	if (UcCvrCod_imt == CVER122) {
		RegWriteA(CVSMTHX, 0x00);			// 0x0023	smooth off
		RegWriteA(CVSMTHY, 0x00);			// 0x0024	smooth off
	}

	RegReadA(STBB0, &UcStbb0);		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
#ifdef SEL_CLOSED_AF							// Closed Loop AF
	UcStbb0 &= 0xA0;
#else
	UcStbb0 &= 0x80;
#endif
	RegWriteA(STBB0, UcStbb0);			// 0x0250	OIS standby
}

//********************************************************************************
// Function Name 	: ClrGyr
// Retun Value		: NON
// Argment Value	: UsClrFil - Select filter to clear.  If 0x0000, clears entire filter
//					  UcClrMod - 0x01: FRAM0 Clear, 0x02: FRAM1, 0x03: All RAM Clear
// Explanation		: Gyro RAM clear function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void ClrGyr_IMT(uint16_t UsClrFil, uint8_t UcClrMod)
{
	uint8_t	UcRamClr;
	uint8_t	count = 0;

	/*Select Filter to clear*/
	RegWriteA(WC_RAMDLYMOD1, (uint8_t)(UsClrFil >> 8));		// 0x018F		FRAM Initialize Hbyte
	RegWriteA(WC_RAMDLYMOD0, (uint8_t)UsClrFil);				// 0x018E		FRAM Initialize Lbyte

	/*Enable Clear*/
	RegWriteA(WC_RAMINITON, UcClrMod);	// 0x0102	[ - | - | - | - ][ - | - | �x��Clr | �W��Clr ]

	/*Check RAM Clear complete*/
	do {
		RegReadA(WC_RAMINITON, &UcRamClr);
		UcRamClr &= UcClrMod;

		if (count++ >= 100) {
			break;
		}

	} while (UcRamClr != 0x00);
}

//********************************************************************************
// Function Name 	: RamAccFixMod
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Ram Access Fix Mode setting function
// History			: First edition 						2013.05.21 Y.Shigeoka
//********************************************************************************
void RamAccFixMod_IMT(uint8_t UcAccMod)
{
	switch (UcAccMod) {
	case OFF :
		RegWriteA(WC_RAMACCMOD, 0x00);		// 0x018C		GRAM Access Float32bit
		break;
	case ON :
		RegWriteA(WC_RAMACCMOD, 0x31);		// 0x018C		GRAM Access Fix32bit
		break;
	default:
		break;
	}
}

//********************************************************************************
// Function Name 	: DrvSw
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Driver Mode setting function
// History			: First edition 						2012.04.25 Y.Shigeoka
//********************************************************************************
void DrvSw_IMT(uint8_t UcDrvSw)
{
	if (UcDrvSw == ON) {
		if (UcPwmMod_imt == PWMMOD_CVL) {
			RegWriteA(DRVFC, 0xF0);			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
		} else {
			RegWriteA(DRVFC, 0x00);			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
		}
	} else {
		if (UcPwmMod_imt == PWMMOD_CVL) {
			RegWriteA(DRVFC, 0x30);				// 0x0001	Drvier Block Ena=0
		} else {
			RegWriteA(DRVFC, 0x00);				// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
		}
	}
}

//********************************************************************************
// Function Name 	: IniFil
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Initial Parameter Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
int IniFil_IMT(void)
{
	uint16_t UsAryId;

	if (UcVerLow_imt != 0x13) {					// 0x13 MTM Act.
		return OIS_FW_POLLING_VERSION_FAIL;
	}

	// Filter Registor Parameter Setting
	UsAryId	= 0;
	while (CsFilReg[UsAryId].UsRegAdd != 0xFFFF) {
		RegWriteA(CsFilReg[UsAryId].UsRegAdd, CsFilReg[UsAryId].UcRegDat);
		UsAryId++;
	}

	// Filter Ram Parameter Setting
	UsAryId	= 0;
	while (CsFilRam[UsAryId].UsRamAdd < 0x1100) {
		RamWrite32A(CsFilRam[UsAryId].UsRamAdd, CsFilRam[UsAryId].UlRamDat);
		UsAryId++;
	}

	// AF Filter Ram Parameter Setting
	UsAryId	= 326;
	while (CsFilRam[UsAryId].UsRamAdd != 0xFFFF) {
		RamWrite32A(CsFilRam[UsAryId].UsRamAdd, CsFilRam[UsAryId].UlRamDat);
		UsAryId++;
	}

	return OIS_FW_POLLING_PASS;
}

//********************************************************************************
// Function Name 	: IniAdj
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Adjust Value Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void IniAdj_IMT(void)
{
	uint8_t	UcStbb0;

	IniPtAve_IMT();								// Average setting

	/* OIS */
	RegWriteA(CMSDAC0, BIAS_CUR_OIS);		// 0x0251	Hall Dac�d��
	RegWriteA(OPGSEL0, AMP_GAIN_X);			// 0x0253	Hall amp Gain X
	RegWriteA(OPGSEL1, AMP_GAIN_Y);			// 0x0254	Hall amp Gain Y
	/* AF */
#ifdef	SEL_CLOSED_AF
	RegWriteA(CMSDAC1, BIAS_CUR_AF);			// 0x0252	Hall Dac�d��
	RegWriteA(OPGSEL2, AMP_GAIN_AF);			// 0x0255	Hall amp Gain AF
#endif	//SEL_CLOSED_AF

	/* OSC Clock value */
	if (((uint8_t)StCalDat_imt.UcOscVal == 0x00) || ((uint8_t)StCalDat_imt.UcOscVal == 	0xFF)) {
		RegWriteA(OSCSET, OSC_INI);				// 0x0257	OSC ini
	} else {
		RegWriteA(OSCSET, StCalDat_imt.UcOscVal);	// 0x0257
	}

	/* adjusted value */
	/* Gyro X axis Offset */
	if ((StCalDat_imt.StGvcOff.UsGxoVal == 0x0000) || (StCalDat_imt.StGvcOff.UsGxoVal == 0xFFFF)) {
		RegWriteA(IZAH, DGYRO_OFST_XH);		// 0x02A0		Set Offset High byte
		RegWriteA(IZAL, DGYRO_OFST_XL);		// 0x02A1		Set Offset Low byte
	} else {
		RegWriteA(IZAH, (uint8_t)(StCalDat_imt.StGvcOff.UsGxoVal >> 8));	// 0x02A0		Set Offset High byte
		RegWriteA(IZAL, (uint8_t)(StCalDat_imt.StGvcOff.UsGxoVal));		// 0x02A1		Set Offset Low byte
	}
	/* Gyro Y axis Offset */
	if ((StCalDat_imt.StGvcOff.UsGyoVal == 0x0000) || (StCalDat_imt.StGvcOff.UsGyoVal == 0xFFFF)) {
		RegWriteA(IZBH, DGYRO_OFST_YH);		// 0x02A2		Set Offset High byte
		RegWriteA(IZBL, DGYRO_OFST_YL);		// 0x02A3		Set Offset Low byte
	} else {
		RegWriteA(IZBH, (uint8_t)(StCalDat_imt.StGvcOff.UsGyoVal >> 8));	// 0x02A2		Set Offset High byte
		RegWriteA(IZBL, (uint8_t)(StCalDat_imt.StGvcOff.UsGyoVal));		// 0x02A3		Set Offset Low byte
	}

	/* Ram Access */
	RamAccFixMod_IMT(ON);							// 16bit Fix mode

	/* OIS adjusted parameter */
	/* Hall X axis Bias,Offset,Lens center */
	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjHallF == 0xFFFF) || (StCalDat_imt.UsAdjHallF & (EXE_HXADJ - 	EXE_END))) {
		RamWriteA(DAXHLO, 	DAHLXO_INI);				// 0x1479
		RamWriteA(DAXHLB, 	DAHLXB_INI);				// 0x147A
	} else {
		RamWriteA(DAXHLO, StCalDat_imt.StHalAdj.UsHlxOff);	// 0x1479
		RamWriteA(DAXHLB, StCalDat_imt.StHalAdj.UsHlxGan);	// 0x147A
	}
	/* Hall Y axis Bias,Offset,Lens center */
	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjHallF == 0xFFFF) || (StCalDat_imt.UsAdjHallF & (EXE_HYADJ - 	EXE_END))) {
		RamWriteA(DAYHLO, 	DAHLYO_INI);				// 0x14F9
		RamWriteA(DAYHLB, 	DAHLYB_INI);				// 0x14FA
	} else {
		RamWriteA(DAYHLO, StCalDat_imt.StHalAdj.UsHlyOff);	// 0x14F9
		RamWriteA(DAYHLB, StCalDat_imt.StHalAdj.UsHlyGan);	// 0x14FA
	}

	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjLensF == 0xFFFF) || (StCalDat_imt.UsAdjLensF & (EXE_CXADJ - 	EXE_END))) {
		RamWriteA(OFF0Z, HXOFF0Z_INI);				// 0x1450
	} else {
		RamWriteA(OFF0Z, StCalDat_imt.StLenCen.UsLsxVal);	// 0x1450
	}
	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjLensF == 0xFFFF) || (StCalDat_imt.UsAdjLensF & (EXE_CYADJ - 	EXE_END))) {
		RamWriteA(OFF1Z, HYOFF1Z_INI);				// 0x14D0
	} else {
		RamWriteA(OFF1Z, StCalDat_imt.StLenCen.UsLsyVal);	// 0x14D0
	}

	/* Hall X axis Loop Gain */
	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjHallF == 0xFFFF) || (StCalDat_imt.UsAdjHallF & (EXE_LXADJ - 	EXE_END))) {
		RamWriteA(sxg, SXGAIN_INI);			// 0x10D3
	} else {
		RamWriteA(sxg, StCalDat_imt.StLopGan.UsLxgVal);	// 0x10D3
	}
	/* Hall Y axis Loop Gain */
	if ((StCalDat_imt.UsAdjHallF == 0x0000) || (StCalDat_imt.UsAdjHallF == 0xFFFF) || (StCalDat_imt.UsAdjHallF & (EXE_LYADJ - 	EXE_END))) {
		RamWriteA(syg, SYGAIN_INI);			// 0x11D3
	} else {
		RamWriteA(syg, StCalDat_imt.StLopGan.UsLygVal);	// 0x11D3
	}
	/* Hall Z AF adjusted parameter */
	if ((StCalDat_imt.UsAdjHallZF == 0x0000) || (StCalDat_imt.UsAdjHallZF == 0xFFFF) || (StCalDat_imt.UsAdjHallF & (EXE_HZADJ - EXE_END))) {
		RamWriteA(DAZHLO, DAHLZO_INI);		// 0x1529
		RamWriteA(DAZHLB, DAHLZB_INI);		// 0x152A
		RamWriteA(OFF4Z, AFOFF4Z_INI);		// 0x145F
		RamWriteA(OFF6Z, AFOFF6Z_INI);		// 0x1510
		RegWriteA(DRVFC4AF, AFDROF_INI << 3);								// 0x84
	} else {
		RamWriteA(DAZHLO, StCalDat_imt.StHalAdj.UsHlzOff);		// 0x1529
		RamWriteA(DAZHLB, StCalDat_imt.StHalAdj.UsHlzGan);		// 0x152A
		RamWriteA(OFF4Z, StCalDat_imt.StHalAdj.UsAdzOff);		// 0x145F
		RamWriteA(OFF6Z, AFOFF6Z_INI);					// 0x1510
		RegWriteA(DRVFC4AF, StCalDat_imt.StAfOff.UcAfOff << 3);				// 0x84
	}

	/* Ram Access */
	RamAccFixMod_IMT(OFF);							// 32bit Float mode

	/* Gyro X axis Gain */
	if ((StCalDat_imt.UlGxgVal == 0x00000000) || (StCalDat_imt.UlGxgVal == 	0xFFFFFFFF)) {
		RamWrite32A(gxzoom, GXGAIN_INI);				// 0x1020 Gyro X axis Gain adjusted value
	} else {
		RamWrite32A(gxzoom, StCalDat_imt.UlGxgVal);		// 0x1020 Gyro X axis Gain adjusted value
	}

	/* Gyro Y axis Gain */
	if ((StCalDat_imt.UlGygVal == 0x00000000) || (StCalDat_imt.UlGygVal == 	0xFFFFFFFF)) {
		RamWrite32A(gyzoom, GYGAIN_INI);				// 0x1120 Gyro Y axis Gain adjusted value
	} else {
		RamWrite32A(gyzoom, StCalDat_imt.UlGygVal);		// 0x1120 Gyro Y axis Gain adjusted value
	}

	RegWriteA(WC_RAMACCXY, 0x01);			// 0x018D	Filter copy off

	RamWrite32A(sxq, SXQ_INI);			// 0x10E5	X axis output direction initial value
	RamWrite32A(syq, SYQ_INI);			// 0x11E5	Y axis output direction initial value
#ifdef SEL_CLOSED_AF
	RamWrite32A(afag, AFAG_INI);		// 0x1203
#endif	//SEL_CLOSED_AF
	if (GXHY_GYHX) {			/* GX -> HY, GY -> HX */
		RamWrite32A(sxgx, 0x00000000);			// 0x10B8
		RamWrite32A(sxgy, 0x3F800000);			// 0x10B9

		RamWrite32A(sygy, 0x00000000);			// 0x11B8
		RamWrite32A(sygx, 0x3F800000);			// 0x11B9
	} else {
		RamWrite32A(sxgx, 0x3F800000);			// 0x10B8
		RamWrite32A(sxgy, 0x00000000);			// 0x10B9

		RamWrite32A(sygy, 0x3F800000);			// 0x11B8
		RamWrite32A(sygx, 0x00000000);			// 0x11B9
	}

	RegWriteA(PWMA, 0xC0);			// 0x0010		PWM enable

	RegReadA(STBB0, &UcStbb0);			// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 |= 0xDF;
	RegWriteA(STBB0, UcStbb0);		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]

#ifdef SEL_CLOSED_AF							// Closed Loop AF
	RegWriteA(WC_EQSW, 0x02);				// 0x01E0 The RAM value is copied 1502h to 151Fh by Filter EQON.
	RamWrite32A(AFAGZ, 0x00000000);		// 0x1504
	//	RamWrite32A(afstmg, 0xBF800000);			// 0x1202
#else
	RegWriteA(WC_EQSW, 0x02);			// 0x01E0
#endif
	RegWriteA(WC_MESLOOP1, 0x02);		// 0x0193
	RegWriteA(WC_MESLOOP0, 0x00);		// 0x0192
	RegWriteA(WC_AMJLOOP1, 0x02);		// 0x01A3
	RegWriteA(WC_AMJLOOP0, 0x00);		// 0x01A2

	SetPanTiltMode_IMT(OFF);					/* Pan/Tilt OFF */

	SetH1cMod_IMT(ACTMODE);					/* Lvl Change Active mode */

	DrvSw_IMT(ON);							/* 0x0001		Driver Mode setting */

	RegWriteA(WC_EQON, 0x01);			// 0sx0101	Filter ON

	RegWriteA(WC_RAMACCXY, 0x00);			// 0x018D	Simultaneously Setting Off

	SrvCon_IMT(Z_DIR, ON);
}

//********************************************************************************
// Function Name 	: IniPtAve
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan/Tilt Average parameter setting function
// History			: First edition 						2013.09.26 Y.Shigeoka
//********************************************************************************
void IniPtAve_IMT(void)
{
	RegWriteA(WG_PANSTT1DWNSMP0, 0x00);		// 0x0134
	RegWriteA(WG_PANSTT1DWNSMP1, 0x00);		// 0x0135
	RegWriteA(WG_PANSTT2DWNSMP0, 0x90);		// 0x0136 400
	RegWriteA(WG_PANSTT2DWNSMP1, 0x01);		// 0x0137
	RegWriteA(WG_PANSTT3DWNSMP0, 0x64);		// 0x0138 100
	RegWriteA(WG_PANSTT3DWNSMP1, 0x00);		// 0x0139
	RegWriteA(WG_PANSTT4DWNSMP0, 0x00);		// 0x013A
	RegWriteA(WG_PANSTT4DWNSMP1, 0x00);		// 0x013B

	RamWrite32A(st1mean, 0x3f800000);		// 0x1235
	RamWrite32A(st2mean, 0x3B23D700);		// 0x1236	1/400
	RamWrite32A(st3mean, 0x3C23D700);		// 0x1237	1/100
	RamWrite32A(st4mean, 0x3f800000);		// 0x1238

}

//********************************************************************************
// Function Name 	: SetDOFSTDAF
// Retun Value		: NON
// Argment Value	: NON
// Explanation		:
// History			: First edition 						2014.07.11 T.Tokoro
//********************************************************************************
void SetDOFSTDAF_IMT(uint8_t ucSetDat)
{
	uint8_t ucRegDat;

	if (UcCvrCod_imt == CVER122) {
		RegReadA(DRVFC3AF, &ucRegDat);
		RegWriteA(DRVFC4AF, (ucSetDat & 0x18) << 3);					// 0x84
		RegWriteA(DRVFC3AF, (ucRegDat & 0x70) | (ucSetDat & 0x07));	// 0x83
	} else {
		RegWriteA(DRVFC4AF, ucSetDat << 3);								// 0x84
	}
}

//********************************************************************************
// Function Name 	: SetPanTiltMode
// Return Value		: NON
// Argument Value	: NON
// Explanation		: Pan-Tilt Enable/Disable
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void SetPanTiltMode_IMT(uint8_t UcPnTmod)
{
	switch (UcPnTmod) {
	case OFF :
		RegWriteA(WG_PANON, 0x00);			// 0x0109	X,Y Pan/Tilt Function OFF
		break;
	case ON :
		RegWriteA(WG_PANON, 0x01);			// 0x0109	X,Y Pan/Tilt Function ON
		break;
	default:
		break;
	}
}

//********************************************************************************
// Function Name 	: SetH1cMod
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set H1C coefficient Level chang Function
// History			: First edition 						2013.04.18 Y.Shigeoka
//********************************************************************************
void SetH1cMod_IMT(uint8_t	UcSetNum)
{

	switch(UcSetNum) {
	case (ACTMODE):				// initial
		RamWrite32A(gxh1c, DIFIL_S2);		/* 0x1012 */
		RamWrite32A(gyh1c, DIFIL_S2);		/* 0x1112 */

		/* enable setting */
		/* Zoom Step */
		UlH1Coefval_imt	= DIFIL_S2;

		UcH1LvlMod_imt = 0;

		// Limit value Value Setting
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */

		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */

		RamWrite32A(gxhc_tmp, 	DIFIL_S2);	/* 0x100E Base Coef */
		RamWrite32A(gxmg, CHGCOEF);		/* 0x10AA Change coefficient gain */

		RamWrite32A(gyhc_tmp, 	DIFIL_S2);	/* 0x110E Base Coef */
		RamWrite32A(gymg, CHGCOEF);		/* 0x11AA Change coefficient gain */

		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;

	case (S2MODE):				// cancel lvl change mode
		RegWriteA(WG_HCHR, 0x10);			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break;

	case (MOVMODE):			// Movie mode

		UcH1LvlMod_imt = UcSetNum;
		RamWrite32A(gxlmt6L, MINLMT_MOV);	/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT_MOV);	/* 0x112D L-Limit */

		RamWrite32A(gxmg, CHGCOEF_MOV);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF_MOV);		/* 0x11AA Change coefficient gain */

//		RamWrite32A(gxhc_tmp, UlH1Coefval_imt);		/* 0x100E Base Coef */
//		RamWrite32A(gyhc_tmp, UlH1Coefval_imt);		/* 0x110E Base Coef */

		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;

	default:
//		IniPtMovMod(OFF);							// Pan/Tilt setting (Still)

		UcH1LvlMod_imt = UcSetNum;

		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */

		RamWrite32A(gxmg, CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF);			/* 0x11AA Change coefficient gain */

//		RamWrite32A(gxhc_tmp, UlH1Coefval_imt);		/* 0x100E Base Coef */
//		RamWrite32A(gyhc_tmp, UlH1Coefval_imt);		/* 0x110E Base Coef */

		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;
	}
}

//********************************************************************************
// Function Name 	: IniClAf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Closed Loop AF Initial Setting
// History			: First edition 						2014.06.16 K.Nakamura
//********************************************************************************
void IniClAf_IMT(void)
{
	uint8_t	UcStbb0;

	RegWriteA(AFFC, 0x10);				// 0x0088	Closed Af, Constant Current select
	RegWriteA(DRVFCAF, 0x10);			// 0x0081	Drv.MODEAF=0, Drv.ENAAF=0, Constant Current MODE
	RegWriteA(DRVFC2AF, 0x00);			// 0x0082
	RegWriteA(DRVFC3AF, 0x40);			// 0x0083	[bit7-5] Constant DAC Gain, [bit2-0] Offset low order 3bit
	RegWriteA(DRVFC4AF, 0x80);			// 0x0084	[bit7-6] Offset high order 2bit
	RegWriteA(PWMAAF, 0x00);				// 0x0090	AF PWM Disable
	RegWriteA(DRVCH3SEL, 0x01);		// 0x0085	AF H bridge control

	RegWriteA(CCFCAF, 0x08);				// 0x00A1	[ - | CCDTSEL | - | - ][ CCDTSEL | - | - ]

	RegReadA(STBB0, &UcStbb0);			// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 |= 0xA0;
	RegWriteA(STBB0, UcStbb0);			// 0x0250	AF ON
	RegWriteA(STBB1, 0x05);			// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]

	RamWrite32A(AF2PWM, 0x00000000);		// 0x151E	Clear RAM
	RegWriteA(CCAAF, 0x80);			// 0x00A0	[7]=0:OFF 1:ON

	StmvSetClAf_IMT();
}

//********************************************************************************
// Function Name 	: StmvSetClAf
// Retun Value		: NON
// Argment Value	: CLAF Stmv Set
// Explanation		: Step move setting Function
// History			: First edition 						2014.09.19 T.Tokoro
//********************************************************************************
void StmvSetClAf_IMT(void)
{
	RegWriteA(WC_EQSW, 0x10);			// 0x01E0	AmEqSw[4][1][0]=OFF
	//RegWriteA(WC_DWNSMP1, 0x00);		// 0x01E3	down sampling I-Filter
	RegWriteA(WC_DWNSMP2, 0x00);			// 0x01E4	Step move Fs=1/1

	RamAccFixMod_IMT(ON);					// Fix mode

	RamWriteA(AFSTMSTP, AF_STM_STP);	// 0x1514	Step
	RamWriteA(afssmv1, AF_SSMV1); 		// 0x1222	Gain Change 1st timing
	RamWriteA(afssmv2, AF_SSMV2); 		// 0x1223	Gain Change 2nd timing
	RamWriteA(afstma, AF_STMA); 			// 0x121D	Gain 1st
	RamWriteA(afstmb, AF_STMB); 			// 0x121E	Gain 2nd
	RamWriteA(afstmc, AF_STMC); 			// 0x121F	Gain 3rd

	RamAccFixMod_IMT(OFF);					// Float mode

	RegWriteA(WC_STPMVMOD, 0x00);		// 0x01E2
}

//********************************************************************************
// Function Name 	: IniAf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Open AF Initial Setting
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void IniAf_IMT(void)
{
	uint8_t UcStbb0;

	AfDrvSw_IMT(OFF);								/* AF Drvier Block Ena=0 */
#ifdef AF_BIDIRECTION
	//RegWriteA(TCODEH, 	0x04);			// TCODEH(0304h)
	//RamWriteA(TREG_H, 	0x0000);			// TREG_H(0x0380) - TREG_L(0x0381)
	RamWriteA(TCODEH, 0x0200);			// 0x0304 - 0x0305 (Register continuos write)
	RegWriteA(DRVFCAF, 0x10);			// DRVFCAF(0x0081)
	RegWriteA(DRVFC3AF, 0x40);			// DRVFC3AF(0x0083)
	RegWriteA(DRVFC4AF, 0x80);			// DRVFC4AF(0x0084)
	RegWriteA(AFFC, 0x90);			// AFFC(0x0088)
	//RegWriteA(CCAAF, 	0x80);			// CCAAF(0x00A0)
#else	//AF_BIDIRECTION
	RegWriteA(DRVFCAF, 0x20);					// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
	RegWriteA(DRVFC3AF, 0x00);					// 0x0083	DGAINDAF	Gain 0
	RegWriteA(DRVFC4AF, 0x80);					// 0x0084	DOFSTDAF
	RegWriteA(PWMAAF, 0x00);					// 0x0090	AF PWM standby
	RegWriteA(AFFC, 0x80);						// 0x0088	OpenAF/-/-
#endif	//AF_BIDIRECTION
	RegWriteA(DRVFC2AF, 0x00);				// 0x0082	AF slope0
	RegWriteA(DRVCH3SEL, 0x01);				// 0x0085	AF H bridge control
	RegWriteA(PWMFCAF, 0x01);				// 0x0091	AF VREF, Carrier, MODE1
	RegWriteA(PWMPERIODAF, 0x20);				// 0x0099	AF none-synchronism
#ifdef AF_BIDIRECTION
	RegWriteA(CCFCAF, 0x08);					// CCFCAF(0x00A1)
#else	//AF_BIDIRECTION
	RegWriteA(CCFCAF, 0x40);					// 0x00A1	GND/-
#endif	//AF_BIDIRECTION

	RegReadA(STBB0, &UcStbb0);		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x7F;
	RegWriteA(STBB0, UcStbb0);			// 0x0250	OIS standby
	RegWriteA(STBB1, 0x00);				// 0x0264	All standby

	/* AF Initial setting */
#ifdef AF_BIDIRECTION
	//SetTregAf(0x0400);						//
	RamWriteA(TCODEH, 0x0600);			// 0x0304 - 0x0305 (Register continuos write) fast mode on
#else	//AF_BIDIRECTION
	RamWriteA(TCODEH, 0x0000);			// 0x0304 - 0x0305 (Register continuos write)

	TRACE("AF INI ---------------------------->\n");
	RegWriteA(FSTMODE, 	FSTMODE_AF);		// 0x0302
	RamWriteA(RWEXD1_L, RWEXD1_L_AF);		// 0x0396 - 0x0397 (Register continuos write)
	RamWriteA(RWEXD2_L, RWEXD2_L_AF);		// 0x0398 - 0x0399 (Register continuos write)
	RamWriteA(RWEXD3_L, RWEXD3_L_AF);		// 0x039A - 0x039B (Register continuos write)
	RegWriteA(FSTCTIME, FSTCTIME_AF);		// 0x0303
	TRACE("AF INI <----------------------------\n");
#endif	//AF_BIDIRECTION

	UcStbb0 |= 0x80;
	RegWriteA(STBB0, UcStbb0);			// 0x0250
	RegWriteA(STBB1, 0x05);			// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]

	AfDrvSw_IMT(ON);								/* AF Drvier Block Ena=1 */
}

//********************************************************************************
// Function Name 	: AfDrvSw
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: AF Driver Mode setting function
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void AfDrvSw_IMT(uint8_t UcDrvSw)
{
	if (UcDrvSw == ON) {
#ifdef AF_BIDIRECTION
		RegWriteA(DRVFCAF, 0x10);			// DRVFCAF(0x0081)
#else	//AF_BIDIRECTION
		RegWriteA(DRVFCAF, 0x20);				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
#endif	//AF_BIDIRECTION
		RegWriteA(CCAAF, 0x80);				// 0x00A0	[7]=0:OFF 1:ON
	} else {
		RegWriteA(CCAAF, 0x00);				// 0x00A0	[7]=0:OFF 1:ON
	}
}

//********************************************************************************
// Function Name 	: IniMon
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Monitor & Other Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void IniMon_IMT(void)
{
	RegWriteA(PWMMONA, 0x00);				// 0x0030	0:off
	RegWriteA(MONSELA, 0x5C);				// 0x0270	DLYMON1
	RegWriteA(MONSELB, 0x5D);				// 0x0271	DLYMON2
	RegWriteA(MONSELC, 0x00);				// 0x0272
	RegWriteA(MONSELD, 0x00);				// 0x0273

	// Monitor Circuit
	RegWriteA(WC_PINMON1, 0x00);			// 0x01C0		Filter Monitor
	RegWriteA(WC_PINMON2, 0x00);			// 0x01C1
	RegWriteA(WC_PINMON3, 0x00);			// 0x01C2
	RegWriteA(WC_PINMON4, 0x00);			// 0x01C3
	/* Delay Monitor */
	RegWriteA(WC_DLYMON11, 0x04);			// 0x01C5		DlyMonAdd1[10:8]
	RegWriteA(WC_DLYMON10, 0x40);			// 0x01C4		DlyMonAdd1[ 7:0]
	RegWriteA(WC_DLYMON21, 0x04);			// 0x01C7		DlyMonAdd2[10:8]
	RegWriteA(WC_DLYMON20, 0xC0);			// 0x01C6		DlyMonAdd2[ 7:0]
	RegWriteA(WC_DLYMON31, 0x00);			// 0x01C9		DlyMonAdd3[10:8]
	RegWriteA(WC_DLYMON30, 0x00);			// 0x01C8		DlyMonAdd3[ 7:0]
	RegWriteA(WC_DLYMON41, 0x00);			// 0x01CB		DlyMonAdd4[10:8]
	RegWriteA(WC_DLYMON40, 0x00);			// 0x01CA		DlyMonAdd4[ 7:0]

	/* Monitor */
	RegWriteA(PWMMONA, 0x80);				// 0x0030	1:on
	//	RegWriteA(IOP0SEL, 	0x01); 			// 0x0230	IOP0 : MONA
	/**/
}

//********************************************************************************
// Function Name 	: IniGyr
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Setting Initialize Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
#define	TRI_LEVEL		0x3A800100		/* 0.001 */
#define	TIMELOW			0x50			/* */
#define	TIMEHGH			0x05			/* */
#define	TIMEBSE			0x5D			/* 3.96ms */
#define	MONADR			GXXFZ
#define	GANADR			gxadj
#define	XMINGAIN		0x00000000
#define	XMAXGAIN		0x3F800000
#define	YMINGAIN		0x00000000
#define	YMAXGAIN		0x3F800000
#define	XSTEPUP			0x38D1B717		/* 0.0001	 */
#define	XSTEPDN			0xBD4CCCCD		/* -0.05 	 */
#define	YSTEPUP			0x38D1B717		/* 0.0001	 */
#define	YSTEPDN			0xBD4CCCCD		/* -0.05 	 */
void IniGyr_IMT(void)
{
#ifdef CATCHMODE
	/* CPU control */
	RegWriteA(WC_CPUOPEON, 0x11);	// 0x0103	 	CPU control
	RegWriteA(WC_CPUOPE1ADD, 0x31);	// 0x018A
	RegWriteA(WC_CPUOPE2ADD, 0x3A);	// 0x018B
#endif

	/*Gyro Filter Setting*/
#ifdef CATCHMODE
	RegWriteA(WG_EQSW, 0x43);		// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
#else
	RegWriteA(WG_EQSW, 0x03);						// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
#endif

	/*Gyro Filter Down Sampling*/

	RegWriteA(WG_SHTON, 0x10);		// 0x0107		[ - | - | - | CmSht2PanOff ][ - | - | CmShtOpe(1:0) ]
	//				CmShtOpe[1:0] 00: �V���b??OFF, 01: �V���b??ON, 1x:�O������

#ifdef DEF_SET
	RegWriteA(WG_SHTDLYTMR, 0x00);	// 0x0117	 	Shutter Delay
	RegWriteA(WG_GADSMP, 0x00);	// 0x011C		Sampling timing
	RegWriteA(WG_HCHR, 0x00);	// 0x011B		H-filter limitter control not USE

	//				CmLmt3Mod	0: �ʏ탊?�b??����, 1: ?�̔��a��?�b??����
	RegWriteA(WG_VREFADD, 0x12);		// 0x0119	 	�Z��??�߂����s���x��RAM�̃A�h���X����6�r�b�g?(default 0x12 = GXH1Z2/GYH1Z2)
#endif
	RegWriteA(WG_SHTMOD, 0x06);		// 0x0116	 	Shutter Hold mode
	RegWriteA(WG_LMT3MOD, 0x00);		// 0x0118 	[ - | - | - | - ][ - | - | - | CmLmt3Mod ]

	// Limiter
	RamWrite32A(gxlmt1H, GYRLMT1H);			// 0x1028
	RamWrite32A(gylmt1H, GYRLMT1H);			// 0x1128

	RamWrite32A(gxlmt3HS0, GYRLMT3_S1);		// 0x1029
	RamWrite32A(gylmt3HS0, GYRLMT3_S1);		// 0x1129

	RamWrite32A(gxlmt3HS1, GYRLMT3_S2);		// 0x102A
	RamWrite32A(gylmt3HS1, GYRLMT3_S2);		// 0x112A

	RamWrite32A(gylmt4HS0, GYRLMT4_S1);		//0x112B	Y��Limiter4 High?�l0
	RamWrite32A(gxlmt4HS0, GYRLMT4_S1);		//0x102B	X��Limiter4 High?�l0

	RamWrite32A(gxlmt4HS1, GYRLMT4_S2);		//0x102C	X��Limiter4 High?�l1
	RamWrite32A(gylmt4HS1, GYRLMT4_S2);		//0x112C	Y��Limiter4 High?�l1


	/* Pan/Tilt parameter */
	RegWriteA(WG_PANADDA, 0x12);		// 0x0130	GXH1Z2/GYH1Z2 Select
	RegWriteA(WG_PANADDB, 0x09);		// 0x0131	GXIZ/GYIZ Select

	//Threshold
	RamWrite32A(SttxHis, 0x00000000);			// 0x1226
	RamWrite32A(SttxaL, 0x00000000);			// 0x109D
	RamWrite32A(SttxbL, 0x00000000);			// 0x109E
	RamWrite32A(Sttx12aM, GYRA12_MID);	// 0x104F
	RamWrite32A(Sttx12aH, GYRA12_HGH);	// 0x105F
	RamWrite32A(Sttx12bM, GYRB12_MID);	// 0x106F
	RamWrite32A(Sttx12bH, GYRB12_HGH);	// 0x107F
	RamWrite32A(Sttx34aM, GYRA34_MID);	// 0x108F
	RamWrite32A(Sttx34aH, GYRA34_HGH);	// 0x109F
	RamWrite32A(Sttx34bM, GYRB34_MID);	// 0x10AF
	RamWrite32A(Sttx34bH, GYRB34_HGH);	// 0x10BF
	RamWrite32A(SttyaL, 0x00000000);			// 0x119D
	RamWrite32A(SttybL, 0x00000000);			// 0x119E
	RamWrite32A(Stty12aM, GYRA12_MID);	// 0x114F
	RamWrite32A(Stty12aH, GYRA12_HGH);	// 0x115F
	RamWrite32A(Stty12bM, GYRB12_MID);	// 0x116F
	RamWrite32A(Stty12bH, GYRB12_HGH);	// 0x117F
	RamWrite32A(Stty34aM, GYRA34_MID);	// 0x118F
	RamWrite32A(Stty34aH, GYRA34_HGH);	// 0x119F
	RamWrite32A(Stty34bM, GYRB34_MID);	// 0x11AF
	RamWrite32A(Stty34bH, GYRB34_HGH);	// 0x11BF

	// Pan level
	RegWriteA(WG_PANLEVABS, 0x00);		// 0x0133

	// Average parameter are set IniAdj

	// Phase Transition Setting
#ifdef CATCHMODE
	// State 2 -> 1
	RegWriteA(WG_PANSTT21JUG0, 0x07);		// 0x0140
	RegWriteA(WG_PANSTT21JUG1, 0x00);		// 0x0141
	// State 3 -> 1
	RegWriteA(WG_PANSTT31JUG0, 0x00);		// 0x0142
	RegWriteA(WG_PANSTT31JUG1, 0x00);		// 0x0143
	// State 4 -> 1
	RegWriteA(WG_PANSTT41JUG0, 0x00);		// 0x0144
	RegWriteA(WG_PANSTT41JUG1, 0x00);		// 0x0145
	// State 1 -> 2
	RegWriteA(WG_PANSTT12JUG0, 0x00);		// 0x0146
	RegWriteA(WG_PANSTT12JUG1, 0x07);		// 0x0147
	// State 1 -> 3
	RegWriteA(WG_PANSTT13JUG0, 0x00);		// 0x0148
	RegWriteA(WG_PANSTT13JUG1, 0x00);		// 0x0149
	// State 2 -> 3
	RegWriteA(WG_PANSTT23JUG0, 0x00);		// 0x014A
	RegWriteA(WG_PANSTT23JUG1, 0x00);		// 0x014B
	// State 4 -> 3
	RegWriteA(WG_PANSTT43JUG0, 0x00);		// 0x014C
	RegWriteA(WG_PANSTT43JUG1, 0x00);		// 0x014D
	// State 3 -> 4
	RegWriteA(WG_PANSTT34JUG0, 0x00);		// 0x014E
	RegWriteA(WG_PANSTT34JUG1, 0x00);		// 0x014F
	// State 2 -> 4
	RegWriteA(WG_PANSTT24JUG0, 0x00);		// 0x0150
	RegWriteA(WG_PANSTT24JUG1, 0x00);		// 0x0151
	// State 4 -> 2
	RegWriteA(WG_PANSTT42JUG0, 0x00);		// 0x0152
	RegWriteA(WG_PANSTT42JUG1, 0x00);		// 0x0153
#else
	// State 2 -> 1
	RegWriteA(WG_PANSTT21JUG0, 0x00);		// 0x0140
	RegWriteA(WG_PANSTT21JUG1, 0x00);		// 0x0141
	// State 3 -> 1
	RegWriteA(WG_PANSTT31JUG0, 0x00);				// 0x0142
	RegWriteA(WG_PANSTT31JUG1, 0x00);				// 0x0143
	// State 4 -> 1
	RegWriteA(WG_PANSTT41JUG0, 0x01);				// 0x0144
	RegWriteA(WG_PANSTT41JUG1, 0x00);				// 0x0145
	// State 1 -> 2
	RegWriteA(WG_PANSTT12JUG0, 0x00);				// 0x0146
	RegWriteA(WG_PANSTT12JUG1, 0x07);				// 0x0147
	// State 1 -> 3
	RegWriteA(WG_PANSTT13JUG0, 0x00);				// 0x0148
	RegWriteA(WG_PANSTT13JUG1, 0x00);				// 0x0149
	// State 2 -> 3
	RegWriteA(WG_PANSTT23JUG0, 0x11);				// 0x014A
	RegWriteA(WG_PANSTT23JUG1, 0x00);				// 0x014B
	// State 4 -> 3
	RegWriteA(WG_PANSTT43JUG0, 0x00);				// 0x014C
	RegWriteA(WG_PANSTT43JUG1, 0x00);				// 0x014D
	// State 3 -> 4
	RegWriteA(WG_PANSTT34JUG0, 0x01);				// 0x014E
	RegWriteA(WG_PANSTT34JUG1, 0x00);				// 0x014F
	// State 2 -> 4
	RegWriteA(WG_PANSTT24JUG0, 0x00);				// 0x0150
	RegWriteA(WG_PANSTT24JUG1, 0x00);				// 0x0151
	// State 4 -> 2
	RegWriteA(WG_PANSTT42JUG0, 0x44);				// 0x0152
	RegWriteA(WG_PANSTT42JUG1, 0x04);				// 0x0153
#endif

	// State Timer
	RegWriteA(WG_PANSTT1LEVTMR, 0x00);				// 0x015B
	RegWriteA(WG_PANSTT2LEVTMR, 0x00);				// 0x015C
	RegWriteA(WG_PANSTT3LEVTMR, 0x00);				// 0x015D
#ifdef CATCHMODE
	RegWriteA(WG_PANSTT4LEVTMR, 0x00);		// 0x015E
#else
	RegWriteA(WG_PANSTT4LEVTMR, 0x03);				// 0x015E
#endif

	// Control filter
#ifdef CATCHMODE
	RegWriteA(WG_PANTRSON0, 0x1B);		// 0x0132	USE iSTP

#else
	RegWriteA(WG_PANTRSON0, 0x11);				// 0x0132	USE I12/iSTP/Gain-Filter
#endif

	// State Setting
	//	IniPtMovMod(OFF);							// Pan/Tilt setting (Still)

	// Hold
	RegWriteA(WG_PANSTTSETILHLD, 0x00);		// 0x015F

	// State2,4 Step Time Setting
	RegWriteA(WG_PANSTT2TMR0, 0x01);		// 0x013C
	RegWriteA(WG_PANSTT2TMR1, 0x00);		// 0x013D
#ifdef CATCHMODE
	RegWriteA(WG_PANSTT4TMR0, 0x01);		// 0x013E
	RegWriteA(WG_PANSTT4TMR1, 0x00);		// 0x013F
#else
	RegWriteA(WG_PANSTT4TMR0, 0x02);					// 0x013E
	RegWriteA(WG_PANSTT4TMR1, 0x07);					// 0x013F
#endif

	RegWriteA(WG_PANSTTXXXTH, 0x00);		// 0x015A

	RamWrite32A(gxlevlow, TRI_LEVEL);					// 0x10AE	Low Th
	RamWrite32A(gylevlow, TRI_LEVEL);					// 0x11AE	Low Th
	RamWrite32A(gxadjmin, XMINGAIN);					// 0x1094	Low gain
	RamWrite32A(gxadjmax, XMAXGAIN);					// 0x1095	Hgh gain
	RamWrite32A(gxadjdn, XSTEPDN);					// 0x1096	-step
	RamWrite32A(gxadjup, XSTEPUP);					// 0x1097	+step
	RamWrite32A(gyadjmin, YMINGAIN);					// 0x1194	Low gain
	RamWrite32A(gyadjmax, YMAXGAIN);					// 0x1195	Hgh gain
	RamWrite32A(gyadjdn, YSTEPDN);					// 0x1196	-step
	RamWrite32A(gyadjup, YSTEPUP);					// 0x1197	+step

	RegWriteA(WG_LEVADD, (uint8_t)MONADR);		// 0x0120	Input signal
	RegWriteA(WG_LEVTMR, TIMEBSE);				// 0x0123	Base Time
	RegWriteA(WG_LEVTMRLOW, TIMELOW);				// 0x0121	X Low Time
	RegWriteA(WG_LEVTMRHGH, TIMEHGH);				// 0x0122	X Hgh Time
	RegWriteA(WG_ADJGANADD, (uint8_t)GANADR);		// 0x0128	control address
	RegWriteA(WG_ADJGANGO, 0x00);					// 0x0108	manual off

	/* exe function */
	//	AutoGainControlSw(OFF);							/* Auto Gain Control Mode OFF */
	AutoGainControlSw_IMT(ON);							/* Auto Gain Control Mode ON  */
}

//********************************************************************************
// Function Name 	: AutoGainControlSw
// Retun Value		: NON
// Argment Value	: 0 :OFF  1:ON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.11.30 Y.Shigeoka
//********************************************************************************
void AutoGainControlSw_IMT(uint8_t UcModeSw)
{
	if (UcModeSw == OFF) {
		RegWriteA(WG_ADJGANGXATO, 0xA0);					// 0x0129	X exe off
		RegWriteA(WG_ADJGANGYATO, 0xA0);					// 0x012A	Y exe off
		RamWrite32A(GANADR, XMAXGAIN);			// Gain Through
		RamWrite32A(GANADR | 0x0100, YMAXGAIN);			// Gain Through
	} else {
		RegWriteA(WG_ADJGANGXATO, 0xA3);					// 0x0129	X exe on
		RegWriteA(WG_ADJGANGYATO, 0xA3);					// 0x012A	Y exe on
	}
}

//********************************************************************************
// Function Name 	: RtnCen
// Retun Value		: Command Status
// Argment Value	: Command Parameter
// Explanation		: Return to center Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
uint8_t RtnCen_IMT(uint8_t UcCmdPar)
{
	uint8_t	UcCmdSts;

	UcCmdSts = EXE_END;

	GyrCon_IMT(OFF);											// Gyro OFF

	if (!UcCmdPar) {										// X,Y Centering
		StbOnn_IMT();											// Slope Mode
	} else if (UcCmdPar == 0x01) {							// X Centering Only
		SrvCon_IMT(X_DIR, ON);								// X only Servo ON
		SrvCon_IMT(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) {							// Y Centering Only
		SrvCon_IMT(X_DIR, OFF);								// Y only Servo ON
		SrvCon_IMT(Y_DIR, ON);
	}

	return(UcCmdSts);
}

//********************************************************************************
// Function Name 	: GyrCon
// Retun Value		: NON
// Argment Value	: Gyro Filter ON or OFF
// Explanation		: Gyro Filter Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void GyrCon_IMT(uint8_t	UcGyrCon)
{
	// Return HPF Setting
	RegWriteA(WG_SHTON, 0x00);									// 0x0107

	if (UcGyrCon == ON) {												// Gyro ON
		ClrGyr_IMT(0x000E, CLR_FRAM1);		// Gyro Delay RAM Clear
		RamWrite32A(sxggf, 0x3F800000);	// 0x10B5
		RamWrite32A(syggf, 0x3F800000);	// 0x11B5
	} else if (UcGyrCon == SPC) {										// Gyro ON for LINE
		RamWrite32A(sxggf, 0x3F800000);	// 0x10B5
		RamWrite32A(syggf, 0x3F800000);	// 0x11B5
	} else {															// Gyro OFF
		RamWrite32A(sxggf, 0x00000000);	// 0x10B5
		RamWrite32A(syggf, 0x00000000);	// 0x11B5
	}
}

//********************************************************************************
// Function Name 	: StbOnn
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************

void StbOnn_IMT(void)
{
	uint8_t	UcRegValx,UcRegValy;					// Registor value
	uint8_t	UcRegIni;
	uint8_t	UcRegIniCnt = 0;

	RegReadA(WH_EQSWX, &UcRegValx);			// 0x0170
	RegReadA(WH_EQSWY, &UcRegValy);			// 0x0171

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {
		RegWriteA(WH_SMTSRVON, 0x01);				// 0x017C		Smooth Servo ON
		SrvCon_IMT(X_DIR, ON);
		SrvCon_IMT(Y_DIR, ON);
		UcRegIni = 0x11;
		while ((UcRegIni & 0x77) != 0x66) {
			RegReadA(RH_SMTSRVSTT, &UcRegIni);		// 0x01F8		Smooth Servo phase read

			if (CmdRdChk_IMT() !=0)	break;				// Dead Lock check (responce check)
			if ((UcRegIni & 0x77) == 0)	UcRegIniCnt++;
			if (UcRegIniCnt > 10){
				break;			// Status Error
			}
		}
		RegWriteA(WH_SMTSRVON, 0x00);				// 0x017C		Smooth Servo OFF
	} else {
		SrvCon_IMT(X_DIR, ON);
		SrvCon_IMT(Y_DIR, ON);
	}
}

//********************************************************************************
// Function Name 	: SrvCon
// Retun Value		: NON
// Argment Value	: X or Y Select, Servo ON/OFF
// Explanation		: Servo ON,OFF Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void SrvCon_IMT(uint8_t	UcDirSel, uint8_t	UcSwcCon)
{
	if (UcSwcCon) {
		if (UcDirSel == X_DIR) {						// X Direction
			RegWriteA(WH_EQSWX, 0x03);			// 0x0170
			RamWrite32A(sxggf, 0x00000000);		// 0x10B5
		} else if (UcDirSel == Y_DIR) {					// Y Direction
			RegWriteA(WH_EQSWY, 0x03);			// 0x0171
			RamWrite32A(syggf, 0x00000000);		// 0x11B5
#ifdef SEL_CLOSED_AF
		} else if (UcDirSel == Z_DIR) {
//			RegWriteA(WC_EQSW, 0x13);			// 0x01E0 AmEqSw[4][1][0]=ON
			AfSrvOnOffset_IMT();
#endif	// SEL_CLOSED_AF
		}
	} else {
		if (UcDirSel == X_DIR) {					// X Direction
			RegWriteA(WH_EQSWX, 0x02);			// 0x0170
			RamWrite32A(SXLMT, 0x00000000);		// 0x1477
		} else if (UcDirSel == Y_DIR) {				// Y Direction
			RegWriteA(WH_EQSWY, 0x02);			// 0x0171
			RamWrite32A(SYLMT, 0x00000000);		// 0x14F7
#ifdef SEL_CLOSED_AF
		} else if (UcDirSel == Z_DIR) {
			RegWriteA(WC_EQSW, 0x02);			// 0x01E0 AmEqSw[4][1]=ON, AmEqSw[0]=OFF
			RamWrite32A(AFAGZ, 0x00000000);	// 0x1504
			RamWrite32A(AF2PWM, 0x00000000);		// 0x151E
#endif	// SEL_CLOSED_AF
		}
	}
}

//********************************************************************************
// Function Name 	: AfSrvOnOffset
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Smooth Servo ON Function
// History			: First edition 						2014.10.14 Shigeoka
//********************************************************************************
void AfSrvOnOffset_IMT(void)
{
	uint8_t	UcRegDat;
	uint16_t UsRamDat;

	RegReadA(WC_EQSW, &UcRegDat);			// 0x01E0
	if ((UcRegDat & 0x13) != 0x13) {
		RegWriteA(WC_EQSW, 0x02);			// 0x01E0 AmEqSw[4][1]=ON
		RamAccFixMod_IMT(ON);
		RamWriteA(AFAGZ, 0x0000);	// 0x1504
		RamWriteA(AFDZ1, 0x0000);	// 0x1505
		RamWriteA(AFDZ2, 0x0000);	// 0x1506
		RamWriteA(AFEZ1, 0x0000);	// 0x1508
		RamWriteA(AFEZ2, 0x0000);	// 0x1509
		RamWriteA(AFUZ1, 0x0000);	// 0x150B
		RamWriteA(AFUZ2, 0x0000);	// 0x150C
		RamWriteA(AFIZ1, 0x0000);	// 0x150E
		RamWriteA(AFIZ2, 0x0000);	// 0x150F
		RamWriteA(AFFZ, 0x0000);	// 0x1516
		RamWriteA(AFGZ, 0x0000);	// 0x1517
		RamWriteA(AFG3Z, 0x0000);	// 0x1518
		RamWriteA(AFPZ1, 0x0000);	// 0x1519
		RamWriteA(AFPZ3, 0x0000);	// 0x151B
		RamWriteA(AFPZ2, 0x0000);	// 0x151A
		RamWriteA(AFPZ4, 0x0000);	// 0x151C
		RamWriteA(AFLMTZ, 0x0000);	// 0x151D

		RamReadA(AFINZ, &UsRamDat);		// 0x1502
		RamWriteA(AFSTMZ2, UsRamDat);		// 0x151F
		RamWriteA(AFSTMTGT, UsRamDat);		// 0x1513
		RamAccFixMod_IMT(OFF);
		RegWriteA(WC_EQSW, 0x13);			// 0x01E0 AmEqSw[4][1][0]=ON
		StmvExeClAf_IMT(0x0000);
	}
}

//********************************************************************************
// Function Name 	: StmvExeClAf
// Retun Value		: NON
// Argment Value	: CLAF Stmv
// Explanation		: Step move Function
// History			: First edition 						2014.09.19 T.Tokoro
//********************************************************************************
void StmvExeClAf_IMT(uint16_t usRamDat)
{
	uint8_t UcRegDat;

	RamAccFixMod_IMT(ON);					// Fix mode
	RamWriteA(AFSTMTGT, usRamDat);		// 0x1513
	RamAccFixMod_IMT(OFF);					// Float mode

	RegWriteA(WC_STPMV, 0x01);			// 0x01E1

	RegReadA(WC_STPMV, &UcRegDat);

	while (UcRegDat & 0x01) {
		RegReadA(WC_STPMV, &UcRegDat);	// 0x01E1
	}
}

//********************************************************************************
// Function Name 	: OisEna
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void OisEna_IMT(void)
{
	// Servo ON
	SrvCon_IMT(X_DIR, ON);
	SrvCon_IMT(Y_DIR, ON);
	GyrCon_IMT(SPC);
}

//********************************************************************************
// Function Name 	: OisOff_IMT
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS disable Control Function
// History			: First edition
//********************************************************************************
void OisOff_IMT(void)
{
	GyrCon_IMT(OFF);
}

//********************************************************************************
// Function Name 	: AfVcmCod
// Return Value		: NON
// Argument Value	: AF Code
// Explanation		: VCM AF Code setting function
// History			: First edition
//********************************************************************************
void AfVcmCod_IMT(signed short SsCodVal)
{
	RamAccFixMod_IMT(ON);						// 0x018C
	RamWriteA(AFSTMTGT, SsCodVal);		// 0x1513
	RamAccFixMod_IMT(OFF);					// 0x018C
	RegWriteA(WC_STPMV, 0x01);			// 0x01E1
}
/*********************************************************************
 Function Name	: TneGvc
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: Tunes the Gyro VC offset
 History			: First edition							2013.01.15  Y.Shigeoka
*********************************************************************/
#define		GYROFF_HIGH_MOBILE		0x028F //10 DPS
#define		GYROFF_LOW_MOBILE		0xFD71 //-10 DPS
uint8_t TneGvc_IMT(void)
{
	int  SiRsltSts;
	uint16_t UsGxoVal, UsGyoVal;

	// A/D Offset Clear
	RegWriteA(IZAH, 0x00);	// 0x02A0		Set Offset High byte
	RegWriteA(IZAL, 0x00);	// 0x02A1		Set Offset Low byte
	RegWriteA(IZBH, 0x00);	// 0x02A2		Set Offset High byte
	RegWriteA(IZBL, 0x00);	// 0x02A3		Set Offset Low byte

	RegWriteA(WC_RAMACCXY, 0x01);			// 0x018D	Filter copy on

	// Measure Filter1 Setting
	RamWrite32A(mes1aa, 0x3F800000);		// 0x10F0	Through
	RamWrite32A(mes1ab, 0x00000000);		// 0x10F1
	RamWrite32A(mes1ac, 0x00000000);		// 0x10F2
	RamWrite32A(mes1ad, 0x00000000);		// 0x10F3
	RamWrite32A(mes1ae, 0x00000000);		// 0x10F4
	RamWrite32A(mes1ba, 0x3F800000);		// 0x10F5	Through
	RamWrite32A(mes1bb, 0x00000000);		// 0x10F6
	RamWrite32A(mes1bc, 0x00000000);		// 0x10F7
	RamWrite32A(mes1bd, 0x00000000);		// 0x10F8
	RamWrite32A(mes1be, 0x00000000);		// 0x10F9

	RegWriteA(WC_RAMACCXY, 0x00);			// 0x018D	Simultaneously Setting Off

	//////////
	// X
	//////////
	RegWriteA(WC_MES1ADD0, 0x00);		// 0x0194
	RegWriteA(WC_MES1ADD1, 0x00);		// 0x0195
	ClrGyr_IMT(0x1000, CLR_FRAM1);		/* Measure Filter RAM Clear */

	UsGxoVal = (uint16_t)GenMes_IMT(AD2Z, 0);	/* GYRMON1(0x1110) <- GXADZ(0x144A) */
	RegWriteA(IZAH, (uint8_t)(UsGxoVal >> 8));	/* 0x02A0 Set Offset High byte */
	RegWriteA(IZAL, (uint8_t)(UsGxoVal));		/* 0x02A1 Set Offset Low byte */

	//////////
	// Y
	//////////
	RegWriteA(WC_MES1ADD0, 0x00);		// 0x0194
	RegWriteA(WC_MES1ADD1, 0x00);		// 0x0195
	ClrGyr_IMT(0x1000, CLR_FRAM1);		/* Measure Filter RAM Clear */

	UsGyoVal = (uint16_t)GenMes_IMT(AD3Z, 0);	/* GYRMON2(0x1111) <- GYADZ(0x14CA) */
	RegWriteA(IZBH, (uint8_t)(UsGyoVal >> 8));	// 0x02A2		Set Offset High byte
	RegWriteA(IZBL, (uint8_t)(UsGyoVal));		// 0x02A3		Set Offset Low byte

	SiRsltSts = EXE_END;						/* Clear Status */

	if (((int16_t)UsGxoVal < (int16_t)GYROFF_LOW_MOBILE) ||
		((int16_t)UsGxoVal > (int16_t)GYROFF_HIGH_MOBILE)) {
			SiRsltSts |= EXE_GXADJ;
	}

	if (((int16_t)UsGyoVal < (int16_t)GYROFF_LOW_MOBILE) ||
		((int16_t)UsGyoVal > (int16_t)GYROFF_HIGH_MOBILE)) {
			SiRsltSts |= EXE_GYADJ;
	}

	if (EXE_END == SiRsltSts) {
		E2pWrt((uint16_t)0x0920, 2, (uint8_t *)&UsGxoVal);
		WitTim(10);		/*GYRO OFFSET Mobile X */
		E2pWrt((uint16_t)0x0922, 2, (uint8_t *)&UsGyoVal);
		WitTim(10);		/*GYRO OFFSET Mobile Y */
		CDBG("%s normal 10 DPS under !!0x%x, 0x%x return 0x%x\n", __func__, UsGxoVal, UsGyoVal, SiRsltSts);
		return SiRsltSts;
	}
	CDBG("%s 10 DPS over !!0x%x, 0x%x return 0x%x\n", __func__, UsGxoVal, UsGyoVal, SiRsltSts);

	return SiRsltSts;
}

/*********************************************************************
 Function Name	: lc898122a_gen_measure
 Retun Value		: A/D Convert Result
 Argment Value	: Measure Filter Input Signal Ram Address
 Explanation		: General Measure Function
 History			: First edition							2013.01.10 Y.Shigeoka
*********************************************************************/
int16_t GenMes_IMT(uint16_t UsRamAdd, uint8_t UcMesMod)
{
	int16_t	SsMesRlt;
	uint8_t	UcMesFin;

	RegWriteA(WC_MES1ADD0, (uint8_t)UsRamAdd);							// 0x0194
	RegWriteA(WC_MES1ADD1, (uint8_t)((UsRamAdd >> 8) & 0x0001));	// 0x0195
	RamWrite32A(MSABS1AV, 0x00000000);				// 0x1041	Clear

	if (!UcMesMod) {
		RegWriteA(WC_MESLOOP1, 0x04);				// 0x0193
		RegWriteA(WC_MESLOOP0, 0x00);				// 0x0192	1024 Times Measure
		RamWrite32A(msmean, 0x3A7FFFF7);				// 0x1230	1/CmMesLoop[15:0]
	} else {
		RegWriteA(WC_MESLOOP1, 0x01);				// 0x0193
		RegWriteA(WC_MESLOOP0, 0x00);				// 0x0192	1 Times Measure
		RamWrite32A(msmean, 0x3F800000);				// 0x1230	1/CmMesLoop[15:0]
	}

	RegWriteA(WC_MESABS, 0x00);						// 0x0198	none ABS
	//BsyWit(WC_MESMODE, 0x01);						// 0x0190	normal Measure
	RegWriteA(WC_MESMODE, 0x01);						// 0x0190	normal Measure
	WitTim(100);								/* Wait 1024 Times Measure Time */
	RegReadA(WC_MESMODE, &UcMesFin);					// 0x0190	normal Measure
	if (0x00 == UcMesFin)
		WitTim(100);

	RamAccFixMod_IMT(ON);							// 16bit Fix mode

	RamReadA(MSABS1AV, (uint16_t *)&SsMesRlt);	// 0x1041

	RamAccFixMod_IMT(OFF);							// 32bit Float mode

	return SsMesRlt;
}

static struct msm_ois_func_tbl imtech_ois_func_tbl;

int32_t imtech_init_set_onsemi_ois(struct msm_ois_ctrl_t *o_ctrl,
						  struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	uint16_t ver;

	CDBG("%s Enter\n", __func__);

	if (copy_from_user(&ver, (void *)set_info->setting, sizeof(uint16_t)))
		ver = OIS_VER_RELEASE;

	CDBG("%s: ois_on! ver:%d\n", __func__, ver);

	rc = IniSet_IMT(ver);
	if (rc < 0) {
		CDBG("lc898122a_init fail %d\n", rc);
		return rc;
	}

	switch (ver) {
	case OIS_VER_RELEASE:
		CDBG("%s OIS_VER_RELEASE\n", __func__);
		break;
	case OIS_VER_CALIBRATION:
	case OIS_VER_DEBUG:
		CDBG("%s OIS_VER_DEBUG\n", __func__);
#if 1
		if (TneGvc_IMT() != 0x02) {
			CDBG("%s gyro offset cal fail !!\n", __func__);
			return OIS_INIT_EEPROM_ERROR;
		}
#endif
		break;
	default:
		break;
	}

	RtnCen_IMT(0);

	imtech_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	CDBG("%s End\n", __func__);

	return rc;
}

int32_t imtech_onsemi_ois_on(struct msm_ois_ctrl_t *o_ctrl,
					struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	CDBG("%s Enter\n", __func__);
	OisEna_IMT();
	CDBG("%s End\n", __func__);
	return rc;
}

int32_t imtech_onsemi_ois_off(struct msm_ois_ctrl_t *o_ctrl,
					struct msm_ois_set_info_t *set_info)
{
	CDBG("%s Enter\n", __func__);
	OisOff_IMT();
	usleep_range(1000, 1010);
	CDBG("%s End\n", __func__);

	return OIS_SUCCESS;
}

int32_t imtech_onsemi_ois_mode(struct msm_ois_ctrl_t *o_ctrl,
					struct msm_ois_set_info_t *set_info)
{
	int cur_mode = imtech_ois_func_tbl.ois_cur_mode;
	uint8_t mode = *(uint8_t *)set_info->setting;

	CDBG("%s Enter\n", __func__);
	switch (mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
		CDBG("%s:OIS_MODE_PREVIEW_CAPTURE %d\n", __func__, mode);
		break;
	case OIS_MODE_CAPTURE:
		CDBG("%s:OIS_MODE_CAPTURE %d\n", __func__, mode);
		break;
	case OIS_MODE_VIDEO:
		CDBG("%s:OIS_MODE_VIDEO %d\n", __func__, mode);
		break;
	case OIS_MODE_CENTERING_ONLY:
		CDBG("%s:OIS_MODE_CENTERING_ONLY %d\n", __func__, mode);
		break;
	case OIS_MODE_CENTERING_OFF:
		CDBG("%s:OIS_MODE_CENTERING_OFF %d\n", __func__, mode);
		break;
	default:
		CDBG("%s:not support %d\n", __func__, mode);
		break;
	}

	if (cur_mode == mode)
		return OIS_SUCCESS;

	switch (mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
		OisEna_IMT();
		SetH1cMod_IMT(0x80);
		break;
	case OIS_MODE_VIDEO:
		OisEna_IMT();
		SetH1cMod_IMT(0xFF);
		break;
	case OIS_MODE_CENTERING_ONLY:
		RtnCen_IMT(0);
		break;
	case OIS_MODE_CENTERING_OFF:
		break;
	default:
		break;
	}

	imtech_ois_func_tbl.ois_cur_mode = mode;

	CDBG("%s End\n", __func__);

	return OIS_SUCCESS;
}

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 176
#define STABLE_THRESHOLD  600 /* 3.04[pixel]*1.12[um/pixel]*HALL_SCALE_FACTOR */

int32_t imtech_onsemi_ois_stat(struct msm_ois_ctrl_t *o_ctrl,
					struct msm_ois_set_info_t *set_info)
{
	struct msm_ois_info_t ois_stat;
	uint16_t hall_x, hall_y = 0;
	uint16_t gyro_x, gyro_y = 0;
	uint8_t *ptr_dest = NULL;

	CDBG("%s Enter\n", __func__);
	memset(&ois_stat, 0, sizeof(ois_stat));
	RamAccFixMod_IMT(ON);
	RamReadA(AD2Z, &gyro_x);
	RamReadA(AD3Z, &gyro_y);
	RamReadA(AD0OFFZ, &hall_x);
	RamReadA(AD1OFFZ, &hall_y);
	RamAccFixMod_IMT(OFF);

#if 0
	CDBG("%s gyro_x 0x%x | hall_x 0x%x\n", __func__,
		 (int16_t)gyro_x, (int16_t)hall_x);

	CDBG("%s gyro_y 0x%x | hall_y 0x%x\n", __func__,
		 (int16_t)gyro_y, (int16_t)hall_y);
#endif

	snprintf(ois_stat.ois_provider, ARRAY_SIZE(ois_stat.ois_provider), "IMTECH_ONSEMI");
	ois_stat.gyro[0] = (int16_t)gyro_x;
	ois_stat.gyro[1] = (int16_t)gyro_y;
	ois_stat.hall[0] = (int16_t)hall_x;
	ois_stat.hall[1] = (int16_t)hall_y;
	ois_stat.is_stable = 1;

	ptr_dest = (uint8_t *)set_info->ois_info;
	if (copy_to_user(ptr_dest, &ois_stat, sizeof(ois_stat))) {
		CDBG("%s: failed copy_to_user result\n", __func__);
	}

	CDBG("%s End\n", __func__);
	return OIS_SUCCESS;
}

#define LENS_MOVE_POLLING_LIMIT (10)
#define LENS_MOVE_THRESHOLD     (5) /* um */
#define HALL_LIMIT 8602
int32_t imtech_onsemi_ois_move_lens(struct msm_ois_ctrl_t *o_ctrl,
						struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	int16_t offset[2] = {0,};
	uint16_t hall_x, hall_y  = 0;
	int16_t hall_signed_x, hall_signed_y  = 0;
	int16_t target_signed_x, target_signed_y  = 0;
	CDBG("%s Enter\n", __func__);

	if (copy_from_user(&offset[0], set_info->setting, sizeof(int16_t) * 2)) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		rc = -EFAULT;
		return rc;
	}

	CDBG("%s: 0x%x, 0x%x\n", __func__, offset[0], offset[1]);

	RamAccFixMod_IMT(ON);
	RamWriteA(0x1450, offset[0]); /* target x position input */
	RamWriteA(0x14D0, offset[1]); /* target x position input */
	usleep_range(100000, 100010);
	RamReadA(0x1440, &hall_x);
	RamReadA(0x14c0, &hall_y);

	hall_signed_x = (int16_t)hall_x;
	hall_signed_y = (int16_t)hall_y;
	target_signed_x = (int16_t)offset[0];
	target_signed_y = (int16_t)offset[1];
#if 0
	CDBG("%s 1 hall_signed_x 0x%x | hall_signed_y 0x%x\n", __func__,
		 hall_signed_x, hall_signed_y);

	CDBG("%s 1 target_signed_x 0x%x | target_signed_y 0x%x\n", __func__,
		 target_signed_x, target_signed_y);
#endif
	RamAccFixMod_IMT(OFF);

	rc = OIS_FAIL;
	if (abs(hall_signed_x + target_signed_x) < 5825 ||
		abs(hall_signed_y + target_signed_y) < 5825)
		rc = OIS_SUCCESS;
	else {
		printk("%s move fail\n", __func__);
		printk("%s LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR: 0x%x\n", __func__,
			   LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR);
		printk("%s hall_signed_x - target_signed_x: 0x%x - 0x%x = 0x%x\n", __func__,
			   hall_signed_x, target_signed_x, hall_signed_x - target_signed_x);
		printk("%s hall_signed_y - target_signed_y: 0x%x - 0x%x = 0x%x\n", __func__,
			   hall_signed_y, target_signed_y, hall_signed_y - target_signed_y);
	}

	CDBG("%s End\n", __func__);
	return rc;
}

void imtech_imx234_onsemi_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	imtech_ois_func_tbl.ini_set_ois = imtech_init_set_onsemi_ois;
	imtech_ois_func_tbl.enable_ois = imtech_onsemi_ois_on;
	imtech_ois_func_tbl.disable_ois = imtech_onsemi_ois_off;
	imtech_ois_func_tbl.ois_mode = imtech_onsemi_ois_mode;
	imtech_ois_func_tbl.ois_stat = imtech_onsemi_ois_stat;
	imtech_ois_func_tbl.ois_move_lens = imtech_onsemi_ois_move_lens;
	imtech_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	msm_ois_t->sid_ois = 0x48 >> 1;
	msm_ois_t->func_tbl = &imtech_ois_func_tbl;
}

