# bcmdhd
######################
# default feature list
######################

DHDCFLAGS += -Wall -Wstrict-prototypes -Dlinux -DLINUX -DBCMDRIVER            \
        -DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE            \
        -DDHDTHREAD -DSHOW_EVENTS -DBCMDBG -DWLP2P                            \
        -DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                                \
        -DKEEP_ALIVE -DCSCAN -DPKT_FILTER_SUPPORT                             \
        -DEMBEDDED_PLATFORM -DPNO_SUPPORT -DREAD_CONFIG_FROM_FILE             \
	-DSEC_ENHANCEMENT

#####################
# Bus Interface Type
#####################
ifneq ($(CONFIG_BCMDHD_PCIE),)
	BUS_IFACE_PCIE=y
else
	BUS_IFACE_SDIO=y
endif

######################################
# bus(PCIe) specific feature list
######################################

ifeq ($(CONFIG_BCMDHD_PCIE),y)
  DHDCFLAGS += -DPCIE_FULL_DONGLE -DBCMPCIE
  # DPC priority
  DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=-1
  DHDCFLAGS += -DDHD_USE_COHERENT_MEM_FOR_RING
  # Alloc Coherent memory from DMA coherent pool
  DHDCFLAGS += -DDHD_ALLOC_COHERENT_MEM_FROM_ATOMIC_POOL
  # Disable watchdog thread
  DHDCFLAGS += -DCUSTOM_DHD_WATCHDOG_MS=0
  DHDCFLAGS += -DNUM_SCB_MAX_PROBE=3
  # Enable wakelock for legacy scan
  DHDCFLAGS += -DDHD_USE_SCAN_WAKELOCK
  # Enable PKTID AUDIT
  DHDCFLAGS += -DDHD_PKTID_AUDIT_ENABLED
  # Extended HANG event with reason codes
  # DHDCFLAGS += -DDHD_USE_EXTENDED_HANG_REASON
  # Enable Link down recovery
  DHDCFLAGS += -DSUPPORT_LINKDOWN_RECOVERY
  DHDCFLAGS += -DBCMPCIE_DISABLE_ASYNC_SUSPEND
  # Enable OTP write feature
ifeq ($(CONFIG_BCMDHD_COB),y)
  DHDCFLAGS += -DOTP_WRITE_ON
endif
  # Enable Firmware Coredump
  #DHDCFLAGS += -DDHD_FW_COREDUMP

  # Enable OOB Host wake up
  DHDCFLAGS += -DBCMPCIE_OOB_HOST_WAKE

  # Enable Load Balancing support by default.
  # DHD_LB_RXP - Perform RX Packet processing in parallel
  # DHD_LB_STATS - To display the Load Blancing statistics
  DHDCFLAGS += -DDHD_LB -DDHD_LB_RXP -DDHD_LB_STATS
  DHDCFLAGS += -DDHD_LB_PRIMARY_CPUS=0xc
  DHDCFLAGS += -DDHD_LB_SECONDARY_CPUS=0x03


endif

######################################
# common feature for customer platform
######################################


ifeq ($(CONFIG_BCMDHD_HOSTWAKEUP_USE_GPIO),y)
ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
DHDCFLAGS += -DHW_OOB
endif
DHDCFLAGS += -DOOB_INTR_ONLY
DHDCFLAGS += -DCUSTOM_OOB_GPIO_NUM=$(CONFIG_BCMDHD_GPIO_WL_HOSTWAKEUP)
endif
# DMA64 suppports on 64bit Architecture
ifeq ($(CONFIG_64BIT),y)
DHDCFLAGS := $(filter-out -DBCMDMA32,$(DHDCFLAGS))
DHDCFLAGS += -DBCMDMA64OSL
endif

# Wi-Fi Turning on Time
DHDCFLAGS += -DWIFI_TURNON_DELAY=0

DHDCFLAGS += -DCUSTOMER_HW4
#For LGE changes
DHDCFLAGS += -DCUSTOMER_HW10 -DCUSTOMER_HW4_DEBUG -DCUSTOMER_HW4_PRIVATE_CMD
DHDCFLAGS += -DWL_CFG80211
#DHDCFLAGS += -DCUSTOM_COUNTRY_CODE
DHDCFLAGS += -DCOUNTRY_SINGLE_REGREV
# Use Random Mac Addr in Wi-Fi Scan Always Mode
DHDCFLAGS += -DSUPPORT_RANDOM_MAC_SCAN
# enable DYNAMIC_MUMIMO_CONTROL
ifeq ($(CONFIG_BCMDHD_DYNAMIC_MUMIMO_CONTROL),y)
DHDCFLAGS += -DDYNAMIC_MUMIMO_CONTROL
endif

# Debug
DHDCFLAGS += -DSIMPLE_MAC_PRINT
DHDCFLAGS += -DDEBUGFS_CFG80211
# Enable wakelock debug function
DHDCFLAGS += -DDHD_TRACE_WAKE_LOCK
# Print out kernel panic point of file and line info when assertion happened
DHDCFLAGS += -DBCMASSERT_LOG
# Enable SI utility error log
DHDCFLAGS += -DSI_ERROR_ENFORCE

# Print 8021X
DHDCFLAGS += -DDHD_8021X_DUMP

# Print DHCP
DHDCFLAGS += -DDHD_DHCP_DUMP

# VSDB
DHDCFLAGS += -DVSDB
DHDCFLAGS += -DPROP_TXSTATUS

# Wi-Fi Direct
DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
# For p2p connection issue
DHDCFLAGS += -DWL_SCB_TIMEOUT=10
# For TDLS tear down inactive time 10 sec
DHDCFLAGS += -DCUSTOM_TDLS_IDLE_MODE_SETTING=10000
# for TDLS RSSI HIGH for establishing TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_HIGH=-80
# for TDLS RSSI HIGH for tearing down TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_LOW=-85
# Disable TDLS when P2P connection used
DHDCFLAGS += -DDISABLE_TDLS_IN_P2P

# Roaming
#DHDCFLAGS += -DROAM_AP_ENV_DETECTION -DKEEP_CUSTOM_ROAM_TRIGGER
DHDCFLAGS += -DROAM_ENABLE -DROAM_CHANNEL_CACHE -DROAM_API
#DHDCFLAGS += -DENABLE_FW_ROAM_SUSPEND
DHDCFLAGS += -DUNSET_FW_ROAM_WIPHY_FLAG
DHDCFLAGS += -DDHD_LOSSLESS_ROAMING
#DHDCFLAGS += -DENABLE_MAX_DTIM_IN_SUSPEND
#DHDCFLAGS += -DCUSTOM_BCN_TIMEOUT_IN_SUSPEND=10
#DHDCFLAGS += -DCUSTOM_ROAM_TIME_THRESH_IN_SUSPEND=6000
#DHDCFLAGS += -DMAX_DTIM_ALLOWED_INTERVAL=925

# CCX
ifeq ($(CONFIG_BRCM_CCX),y)
  DHDCFLAGS += -DBCMCCX
endif

# For WAPI
ifeq ($(CONFIG_BRCM_WAPI),y)
DHDCFLAGS += -DBCMWAPI_WPI -DBCMWAPI_WAI
endif

DHDCFLAGS += -DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT

# SoftAP
DHDCFLAGS += -DWL_SUPPORT_AUTO_CHANNEL -DSUPPORT_HIDDEN_AP
DHDCFLAGS += -DSUPPORT_SOFTAP_SINGL_DISASSOC
DHDCFLAGS += -DDISABLE_11H_SOFTAP
# For support WPA and WPA2 mixed mode
DHDCFLAGS += -DSUPPORT_SOFTAP_WPAWPA2_MIXED

# HW4 specific features
DHDCFLAGS += -DSUPPORT_PM2_ONLY
#DHDCFLAGS += -DSUPPORT_DEEP_SLEEP
DHDCFLAGS += -DSUPPORT_AMPDU_MPDU_CMD
DHDCFLAGS += -DPASS_IPV4_SUSPEND
DHDCFLAGS += -DPREVENT_REOPEN_DURING_HANG
DHDCFLAGS += -DP2P_SKIP_DFS
DHDCFLAGS += -DENABLE_TDLS_AUTO_MODE
DHDCFLAGS += -DSKIP_WLFC_ON_CONCURRENT
DHDCFLAGS += -DDHD_USE_ATOMIC_PKTGET
DHDCFLAGS += -DSKIP_WLFC_ON_CONCURRENT
DHDCFLAGS += -DDEBUG_DPC_THREAD_WATCHDOG
DHDCFLAGS += -DENABLE_ARP_SNOOP_MODE
DHDCFLAGS += -DDHD_PM_CONTROL_FROM_FILE
#DHDCFLAGS += -DCUSTOM_EVENT_PM_WAKE=20

# For special PNO Event keep wake lock for 10sec
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=10

# For Passing all multicast packets to host when not in suspend mode.
# undefine for to apply Runtime multicast pkt filter
#DHDCFLAGS += -DPASS_ALL_MCAST_PKTS
DHDCFLAGS += -DMANUAL_ALL_MCAST_PKTS

# For IPV6 Neighbor Discovery Offload Support
DHDCFLAGS += -DIPV6_NDO_SUPPORT

# Early suspend
#DHDCFLAGS += -DDHD_USE_EARLYSUSPEND

# WiFi turn off delay
#DHDCFLAGS += -DWIFI_TURNOFF_DELAY=100
DHDCFLAGS += -DWIFI_TURNON_DELAY=0

# For Scan result patch
DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DDUAL_ESCAN_RESULT_BUFFER
DHDCFLAGS += -DESCAN_BUF_OVERFLOW_MGMT

# For Static Buffer
ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
  DHDCFLAGS += -DENHANCED_STATIC_BUF
  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
ifeq ($(CONFIG_BCMDHD_PCIE),y)
  DHDCFLAGS += -DDHD_USE_STATIC_CTRLBUF
ifeq ($(CONFIG_BCMDHD_PREALLOC_PKTIDMAP),y)
  DHDCFLAGS += -DDHD_USE_STATIC_PKTIDMAP
endif #CONFIG_BCMDHD_PREALLOC_PKTIDMAP
ifeq ($(CONFIG_BCMDHD_PREALLOC_MEMDUMP),y)
  DHDCFLAGS += -DDHD_USE_STATIC_MEMDUMP
endif #CONFIG_BCMDHD_PREALLOC_MEMDUMP
endif #CONFIG_BCMDHD_PCIE
endif #CONFIG_BROADCOM_WIFI_RESERVED_MEM

# For CCX
ifeq ($(CONFIG_BRCM_CCX),y)
  DHDCFLAGS += -DBCMCCX
endif

# allow supplicant to make probe response even in P2P nego state for first one time.
DHDCFLAGS += -DP2P_IE_MISSING_FIX

# DTIM listen interval in suspend mode(0 means follow AP's DTIM period)
ifeq ($(CONFIG_BCMDHD_FOLLOW_AP_DTIM_PERIOD),y)
DHDCFLAGS += -DCUSTOM_SUSPEND_BCN_LI_DTIM=0
else
DHDCFLAGS += -DCUSTOM_SUSPEND_BCN_LI_DTIM=3
endif

# Ioctl timeout 5000ms
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# Priority mismatch fix with kernel stack
DHDCFLAGS += -DPKTPRIO_OVERRIDE

# B/G/BGN mode support in HOSTAPD
ifeq ($(CONFIG_BCMDHD_HOSTAPD_BGN_MODE),y)
DHDCFLAGS += -DSUPPORT_HOSTAPD_BGN_MODE
endif

# Used short dwell time during initial scan
DHDCFLAGS += -DUSE_INITIAL_SHORT_DWELL_TIME

# Use Android wake lock mechanism
DHDCFLAGS += -DCONFIG_HAS_WAKELOCK

ifeq ($(CONFIG_BCMDHD_USE_SCHED_SCAN),y)
DHDCFLAGS += -DWL_SCHED_SCAN
endif

# softAP parametet set
DHDCFLAGS += -DSOFTAP_TPUT_ENHANCE
#DHDCFLAGS += -DDHD_TCP_WINSIZE_ADJUST
DHDCFLAGS += -DCUSTOM_DSCP_TO_PRIO_MAPPING

# WFDS
DHDCFLAGS += -DWLWFDS

# Prevent delete p2p discovery interface when suspended.
DHDCFLAGS += -DKEEP_WIFION_OPTION

# Prevent send hang event even though p2p discovery interface is not removed.
DHDCFLAGS += -DEXPLICIT_DISCIF_CLEANUP

# BT Coex Off Timeout when bt sco and dhcp co-exists
DHDCFLAGS += -DBT_DHCP_OPPR_WIN_TIME=10000

# For fast initial scan
DHDCFLAGS += -DUSE_INITIAL_SHORT_DWELL_TIME

DHDCFLAGS += -DCUSTOMER_SCAN_TIMEOUT_SETTING

##############################
# Android Platform Definition
##############################

#################
# Android N
#################
#Android N features
DHDCFLAGS += -DWL_ENABLE_P2P_IF
DHDCFLAGS += -DWL_CFG80211_STA_EVENT
DHDCFLAGS += -DWL_SUPPORT_BACKPORTED_KPATCHES
DHDCFLAGS += -DWL_IFACE_COMB_NUM_CHANNELS
DHDCFLAGS += -DCONFIG_CONTROL_PM

ifneq ($(filter y, $(CONFIG_BCM4359) $(CONFIG_BCM4358) $(CONFIG_BCM43455) $(CONFIG_BCM4356) $(CONFIG_BCM4339)),)
# DHDCFLAGS += -DLINKSTAT_SUPPORT
DHDCFLAGS += -DCONNECTION_STATISTICS
DHDCFLAGS += -DGSCAN_SUPPORT
DHDCFLAGS += -DRSSI_MONITOR_SUPPORT
DHDCFLAGS += -DPFN_SCANRESULT_2
#DHDCFLAGS += -DAPF #removed by cheolsook.lee 20160621, p2p disconnect issue
DHDCFLAGS += -DNDO_CONFIG_SUPPORT
DHDCFLAGS += -DDHD_SET_COUNTRY_SUPPORT
DHDCFLAGS += -DWL_VENDOR_EXT_SUPPORT
ANDROID_OFILES := wl_cfgvendor.o bcmxtlv.o bcm_app_utils.o
endif


##########################
# driver type
# m: module type driver
# y: built-in type driver
##########################
DRIVER_TYPE = $(CONFIG_BCMDHD)

#############################
# Platform dependent feature
#############################
ifeq ($(DRIVER_TYPE),y)
DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD
endif

ifeq ($(CONFIG_BCMDHD_COB),y)
DHDCFLAGS += -DGET_CUSTOM_MAC_ENABLE
endif

#########################
# Chip dependent feature
#########################
ifneq ($(CONFIG_BCM4359),)
  DHDCFLAGS += -DBCM4359_CHIP
  DHDCFLAGS += -DENABLE_BCN_LI_BCN_WAKEUP
  DHDCFLAGS += -DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT
  DHDCFLAGS += -DCUSTOM_BCN_TIMEOUT=6
  DHDCFLAGS += -DSOFTAP_UAPSD_OFF

# tput enhancement for common
  DHDCFLAGS += -DUSE_WL_TXBF
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DDISABLE_FRAMEBURST_VSDB
  DHDCFLAGS += -DDISABLE_PM_BCNRX
# IF race condition debug
  DHDCFLAGS += -DDHD_IFDEBUG
#  DHDCFLAGS += -DDHD_LOG_DUMP
  DHDCFLAGS += -DALLOW_CHSW_EVT

# tput enhancement for PCIE
ifeq ($(BUS_IFACE_PCIE),y)
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=64 -DCUSTOM_IBSS_AMPDU_BA_WSIZE=16
  DHDCFLAGS += -DCUSTOM_AMPDU_MPDU=48
  DHDCFLAGS += -DCUSTOM_AMPDU_RELEASE=32
  DHDCFLAGS += -DCUSTOM_AMSDU_AGGSF=2
  DHDCFLAGS += -DCUSTOM_TCPACK_SUPP_RATIO=15
  DHDCFLAGS += -DCUSTOM_TCPACK_DELAY_TIME=5
  DHDCFLAGS += -DTCPACK_SUPPRESS_HOLD_HRT
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
endif

ifeq ($(CONFIG_RUNTIME_TCPACK_SUPPRESS), y)
DHDCFLAGS += -DSET_TCPACK_SUPPRESS
endif #CONFIG_RUNTIME_TCPACK_SUPPRESS

# New Features
  DHDCFLAGS += -DWL11U -DMFP
  DHDCFLAGS += -DBCMCCX
  DHDCFLAGS += -DWES_SUPPORT
  DHDCFLAGS += -DOKC_SUPPORT
  DHDCFLAGS += -DWLTDLS
  DHDCFLAGS += -DWLFBT
  DHDCFLAGS += -DDHD_ENABLE_LPC
  DHDCFLAGS += -DWLAIBSS -DWLAIBSS_PS
  DHDCFLAGS += -DWL_RELMCAST
  DHDCFLAGS += -DSUPPORT_LTECX
  DHDCFLAGS += -DSUPPORT_2G_VHT
  DHDCFLAGS += -DSUPPORT_WL_TXPOWER
  DHDCFLAGS += -DWL11ULB
 # virtual interface for RSDB
  DHDCFLAGS += -DWL_VIRTUAL_APSTA
 # disable pruned scan
  DHDCFLAGS += -DDISABLE_PRUNED_SCAN
 # Remove common feature for BCM4359
  DHDCFLAGS := $(filter-out -DDISABLE_11H_SOFTAP,$(DHDCFLAGS))
 # Block ARP during DHCP on STA/SoftAP concurrent mode
  DHDCFLAGS += -DAPSTA_BLOCK_ARP_DURING_DHCP
 # Use restricted channels on STA/SoftAP concurrent mode
  DHDCFLAGS += -DAPSTA_RESTRICTED_CHANNEL
# To support APF
#  DHDCFLAGS += -DAPF #removed by cheolsook.lee 20160621, p2p disconnect issue
# To support AP rxchain power save
  DHDCFLAGS += -DSUPPORT_AP_POWERSAVE

ifeq ($(CONFIG_ARCH_MSM8996),y)
 # Runtime PM
  DHDCFLAGS += -DDHD_PCIE_RUNTIMEPM -DMAX_IDLE_COUNT=11 -DCUSTOM_DHD_RUNTIME_MS=100
 # Enable Dongle Isolation
  DHDCFLAGS += -DDONGLE_ENABLE_ISOLATION
 # Disable SOFTAP_SEND_HANGEVT
  DHDCFLAGS := $(filter-out -DSOFTAP_SEND_HANGEVT,$(DHDCFLAGS))
endif

 # RSDB mode from file config
 # DHDCFLAGS += -DRSDB_MODE_FROM_FILE
 # LOGTRACE_EVENT
 DHDCFLAGS += -DSHOW_LOGTRACE
# DHDCFLAGS += -DLOGTRACE_FROM_FILE

ifeq ($(CONFIG_BCM4359),y)
  DHDCFLAGS += -DUSE_LATE_INITCALL_SYNC
  DHDCFLAGS += -DPRIVATE_SCANSUPPRESS
# DRIVER_TYPE = y
endif
endif

#########
# Others
#########

#EXTRA_LDFLAGS += --strip-debug

EXTRA_CFLAGS += $(DHDCFLAGS) -DDHD_DEBUG
EXTRA_CFLAGS += -DSRCBASE=\"$(src)\"
EXTRA_CFLAGS += -I$(src)/include/ -I$(src)/

KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(shell pwd)

DHDOFILES := dhd_pno.o dhd_common.o dhd_ip.o dhd_custom_gpio.o dhd_custom_lge.o	dhd_linux.o \
	dhd_linux_sched.o dhd_cfg80211.o dhd_linux_platdev.o dhd_linux_wq.o aiutils.o \
	bcmevent.o bcmutils.o hnd_pktq.o hnd_pktpool.o bcmwifi_channels.o hndpmu.o linux_osl.o \
	sbutils.o siutils.o wl_android.o wl_cfg80211.o wl_cfgp2p.o wl_cfg_btcoex.o wldev_common.o \
	wl_linux_mon.o wl_roam.o dhd_wlfc.o

ifeq ($(CONFIG_BCMDHD_PCIE),y)
DHDOFILES += dhd_pcie.o dhd_pcie_linux.o dhd_flowring.o	dhd_msgbuf.o pcie_core.o
endif

ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
DHDOFILES += dhd_custom_memprealloc.o dhd_custom_msm_lge.o
endif #CONFIG_BROADCOM_WIFI_RESERVED_MEM

DHDOFILES += $(ANDROID_OFILES)

obj-$(CONFIG_BCMDHD) += bcmdhd.o

bcmdhd-objs := $(DHDOFILES)

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules

clean:
	rm -rf *.o *.ko *.mod.c *~ .*.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin \

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
