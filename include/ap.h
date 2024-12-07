/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    ap.h

    Abstract:
    Miniport generic portion header file

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
*/
#ifndef __AP_H__
#define __AP_H__




/* ============================================================= */
/*      Common definition */
/* ============================================================= */
#define MBSS_VLAN_INFO_GET(__pAd, __VLAN_VID, __VLAN_Priority, __func_idx) \
{																		\
	if ((__func_idx < __pAd->ApCfg.BssidNum) &&					\
		(__func_idx < HW_BEACON_MAX_NUM) &&						\
		(__pAd->ApCfg.MBSSID[__func_idx].wdev.VLAN_VID != 0))			\
	{																	\
		__VLAN_VID = __pAd->ApCfg.MBSSID[__func_idx].wdev.VLAN_VID;	\
		__VLAN_Priority = __pAd->ApCfg.MBSSID[__func_idx].wdev.VLAN_Priority; \
	}																	\
}

#define WDEV_VLAN_INFO_GET(__pAd, __VLAN_VID, __VLAN_Priority, __wdev) \
{																		\
	if ((__wdev->VLAN_VID != 0))			\
	{																	\
		__VLAN_VID = __wdev->VLAN_VID;	\
		__VLAN_Priority = __wdev->VLAN_Priority; \
	}																	\
}


typedef struct _AUTH_FRAME_INFO{
	UCHAR addr1[MAC_ADDR_LEN];
	UCHAR addr2[MAC_ADDR_LEN];
	USHORT auth_alg;
	USHORT auth_seq;
	USHORT auth_status;
	CHAR Chtxt[CIPHER_TEXT_LEN];
}AUTH_FRAME_INFO;


/* ============================================================= */
/*      Function Prototypes */
/* ============================================================= */

BOOLEAN APBridgeToWirelessSta(
    IN  PRTMP_ADAPTER   pAd,
    IN  PUCHAR          pHeader,
    IN  UINT            HdrLen,
    IN  PUCHAR          pData,
    IN  UINT            DataLen,
    IN  ULONG           fromwdsidx);

INT ApAllowToSendPacket(
	IN RTMP_ADAPTER *pAd,
	IN struct wifi_dev *wdev,
	IN PNDIS_PACKET pPacket,
	OUT UCHAR *pWcid);

INT ap_rx_pkt_allow(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk);
INT ap_rx_ps_handle(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk);
INT ap_rx_foward_handle(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, PNDIS_PACKET pPacket);

INT APSendPacket(RTMP_ADAPTER *pAd, PNDIS_PACKET pPacket);

NDIS_STATUS APInsertPsQueue(
	IN RTMP_ADAPTER *pAd,
	IN PNDIS_PACKET pPacket,
	IN STA_TR_ENTRY *tr_entry,
	IN UCHAR QueIdx);

NDIS_STATUS APHardTransmit(RTMP_ADAPTER *pAd, TX_BLK *pTxBlk);

VOID rx_eapol_frm_handle(
	IN RTMP_ADAPTER *pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN RX_BLK *pRxBlk,
	IN UCHAR wdev_idx);

VOID APRxErrorHandle(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk);

BOOLEAN APChkCls2Cls3Err(
	IN RTMP_ADAPTER *pAd,
	IN UCHAR Wcid, 
	IN HEADER_802_11 *pHeader);

VOID RTMPDescriptorEndianChange(UCHAR *pData, ULONG DescriptorType);
    
VOID RTMPFrameEndianChange(
    IN  RTMP_ADAPTER *pAd,
    IN  UCHAR *pData,
    IN  ULONG Dir,
    IN  BOOLEAN FromRxDoneInt);

/* ap_assoc.c */

VOID APAssocStateMachineInit(
    IN  PRTMP_ADAPTER   pAd, 
    IN  STATE_MACHINE *S, 
    OUT STATE_MACHINE_FUNC Trans[]);

VOID MbssKickOutStas(RTMP_ADAPTER *pAd, INT apidx, USHORT Reason);
VOID APMlmeKickOutSta(RTMP_ADAPTER *pAd, UCHAR *staAddr, UCHAR Wcid, USHORT Reason);

#ifdef DOT11W_PMF_SUPPORT
VOID APMlmeKickOutAllSta(RTMP_ADAPTER *pAd, UCHAR apidx, USHORT Reason);
#endif /* DOT11W_PMF_SUPPORT */

VOID  APCls3errAction(RTMP_ADAPTER *pAd, ULONG wcid, HEADER_802_11 *hdr);

/* ap_auth.c */

void APAuthStateMachineInit(
    IN PRTMP_ADAPTER pAd, 
    IN STATE_MACHINE *Sm, 
    OUT STATE_MACHINE_FUNC Trans[]);

VOID APCls2errAction(RTMP_ADAPTER *pAd, ULONG wcid, HEADER_802_11 *hdr);

/* ap_connect.c */

#ifdef CONFIG_AP_SUPPORT
BOOLEAN BeaconTransmitRequired(RTMP_ADAPTER *pAd, INT apidx, BSS_STRUCT *mbss);
#ifdef MT_MAC
VOID APMakeAllTimFrame(RTMP_ADAPTER *pAd);
VOID APMakeTimFrame(RTMP_ADAPTER *pAd, INT apidx);
VOID APCheckBcnQHandler(RTMP_ADAPTER *pAd, INT apidx, BOOLEAN *is_pretbtt_int);
#endif
#endif /* CONFIG_AP_SUPPORT */

VOID APMakeBssBeacon(RTMP_ADAPTER *pAd, INT apidx);
VOID  APUpdateBeaconFrame(RTMP_ADAPTER *pAd, INT apidx);
VOID APMakeAllBssBeacon(RTMP_ADAPTER *pAd);
VOID  APUpdateAllBeaconFrame(RTMP_ADAPTER *pAd);

/* ap_sync.c */
VOID APSyncStateMachineInit(
    IN PRTMP_ADAPTER pAd,
    IN STATE_MACHINE *Sm,
    OUT STATE_MACHINE_FUNC Trans[]);

VOID APScanTimeout(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3);

VOID ApSiteSurvey(
	IN	PRTMP_ADAPTER  		pAd,
	IN	PNDIS_802_11_SSID	pSsid,
	IN	UCHAR				ScanType,
	IN	BOOLEAN				ChannelSel);

VOID SupportRate(
	IN PUCHAR SupRate,
	IN UCHAR SupRateLen,
	IN PUCHAR ExtRate,
	IN UCHAR ExtRateLen,
	OUT PUCHAR *Rates,
	OUT PUCHAR RatesLen,
	OUT PUCHAR pMaxSupportRate);


BOOLEAN ApScanRunning(RTMP_ADAPTER *pAd);

#ifdef DOT11_N_SUPPORT
VOID APUpdateOperationMode(RTMP_ADAPTER *pAd);

#ifdef DOT11N_DRAFT3
VOID APOverlappingBSSScan(RTMP_ADAPTER *pAd);

INT GetBssCoexEffectedChRange(
	IN RTMP_ADAPTER *pAd,
	IN BSS_COEX_CH_RANGE *pCoexChRange);
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */


/* ap_mlme.c */
VOID APMlmePeriodicExec(RTMP_ADAPTER *pAd);

BOOLEAN APMsgTypeSubst(
    IN PRTMP_ADAPTER pAd,
    IN PFRAME_802_11 pFrame, 
    OUT INT *Machine, 
    OUT INT *MsgType);

VOID APQuickResponeForRateUpExec(
    IN PVOID SystemSpecific1, 
    IN PVOID FunctionContext, 
    IN PVOID SystemSpecific2, 
    IN PVOID SystemSpecific3);

VOID APAsicEvaluateRxAnt(RTMP_ADAPTER *pAd);
VOID APAsicRxAntEvalTimeout(RTMP_ADAPTER *pAd);

/* ap.c */
UCHAR get_apidx_by_addr(RTMP_ADAPTER *pAd, UCHAR *addr);

NDIS_STATUS APInitialize(RTMP_ADAPTER *pAd);

INT ap_func_init(RTMP_ADAPTER *pAd);

VOID APShutdown(RTMP_ADAPTER *pAd);
VOID APStartUp(RTMP_ADAPTER *pAd);
VOID APStop(RTMP_ADAPTER *pAd);

VOID APCleanupPsQueue(RTMP_ADAPTER *pAd, QUEUE_HEADER *pQueue);


VOID MacTableMaintenance(RTMP_ADAPTER *pAd);

UINT32 MacTableAssocStaNumGet(RTMP_ADAPTER *pAd);

MAC_TABLE_ENTRY *APSsPsInquiry(
    IN  PRTMP_ADAPTER   pAd, 
    IN  PUCHAR          pAddr, 
    OUT SST             *Sst, 
    OUT USHORT          *Aid,
    OUT UCHAR           *PsMode,
    OUT UCHAR           *Rate); 

#ifdef SYSTEM_LOG_SUPPORT
VOID ApLogEvent(
    IN PRTMP_ADAPTER    pAd,
    IN PUCHAR           pAddr,
    IN USHORT           Event);
#else
#define ApLogEvent(_pAd, _pAddr, _Event)
#endif /* SYSTEM_LOG_SUPPORT */

VOID APUpdateCapabilityAndErpIe(RTMP_ADAPTER *pAd);

BOOLEAN ApCheckAccessControlList(RTMP_ADAPTER *pAd, UCHAR *addr, UCHAR apidx);
VOID ApUpdateAccessControlList(RTMP_ADAPTER *pAd, UCHAR apidx);


#ifdef AP_QLOAD_SUPPORT
VOID QBSS_LoadInit(RTMP_ADAPTER *pAd);
VOID QBSS_LoadAlarmReset(RTMP_ADAPTER *pAd);
VOID QBSS_LoadAlarmResume(RTMP_ADAPTER *pAd);
UINT32 QBSS_LoadBusyTimeGet(RTMP_ADAPTER *pAd);
BOOLEAN QBSS_LoadIsAlarmIssued(RTMP_ADAPTER *pAd);
BOOLEAN QBSS_LoadIsBusyTimeAccepted(RTMP_ADAPTER *pAd, UINT32 BusyTime);
UINT32 QBSS_LoadElementAppend(RTMP_ADAPTER *pAd, UINT8 *buf_p);
VOID QBSS_LoadUpdate(RTMP_ADAPTER *pAd, ULONG UpTime);
VOID QBSS_LoadStatusClear(RTMP_ADAPTER *pAd);

INT	Show_QoSLoad_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
#endif /* AP_QLOAD_SUPPORT */


#ifdef DOT1X_SUPPORT
INT	Set_OwnIPAddr_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
INT	Set_EAPIfName_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
INT	Set_PreAuthIfName_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);

/* Define in ap.c */
BOOLEAN DOT1X_InternalCmdAction(
    IN RTMP_ADAPTER *pAd,
    IN MAC_TABLE_ENTRY *pEntry,
    IN UINT8 cmd);

BOOLEAN DOT1X_EapTriggerAction(RTMP_ADAPTER *pAd, MAC_TABLE_ENTRY *pEntry);
#endif /* DOT1X_SUPPORT */

VOID AP_E2PROM_IOCTL_PostCtrl(RTMP_IOCTL_INPUT_STRUCT *wrq, RTMP_STRING *msg);

VOID IAPP_L2_UpdatePostCtrl(RTMP_ADAPTER *pAd, UINT8 *mac, INT wdev_idx);

#endif  /* __AP_H__ */

