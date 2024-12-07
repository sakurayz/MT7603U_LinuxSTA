/*
 ***************************************************************************
 * MediaTek Inc.
 *
 * All rights reserved. source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek, Inc. is obtained.
 ***************************************************************************

	Module Name:
	mt_ate.c
*/

#include "rt_config.h"

static INT32 MT_ATESetTxPower0(RTMP_ADAPTER *pAd, CHAR Value)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
    struct MT_TX_PWR_CAP *cap = &pAd->chipCap.MTTxPwrCap;
	INT32 Ret = 0;
    UINT32 value = 0;
    CHAR tx_0_pwr;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
	ATECtrl->TxPower0 = Value;
	ATECtrl->TxPower1 = pAd->EEPROMImage[TX1_G_BAND_TARGET_PWR];
	CmdSetTxPowerCtrl(pAd, ATECtrl->Channel);
	return Ret;
}

static INT32 MT_ATESetTxPower1(RTMP_ADAPTER *pAd, CHAR Value)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
	// For TxTone Power	
	ATECtrl->TxPower1 = Value;
	DBGPRINT(RT_DEBUG_OFF, ("%s, power1:0x%x\n", __FUNCTION__,ATECtrl->TxPower1));
	/* Same as Power0 */
    if ( ATECtrl->TxPower0 != ATECtrl->TxPower1)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s: power1 do not same as power0\n", __FUNCTION__));
        Ret = -1;
	}

	return Ret;
}


static INT32 MT_ATEStart(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
	BOOLEAN bCalFree=0;


#ifdef RT_BIG_ENDIAN
	TXD_STRUC *pDestTxD = NULL;
	UCHAR TxHwInfo[TXD_SIZE];
#endif

#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif
	INT32 i,acidx;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

	/*   Stop send TX packets */
	RTMP_OS_NETDEV_STOP_QUEUE(pAd->net_dev);

    /* Reset ATE TX/RX Counter */
    ATECtrl->TxDoneCount = 0;
    ATECtrl->RxTotalCnt = 0;
	ATECtrl->cmd_expire = RTMPMsecsToJiffies(3000);
	RTMP_OS_INIT_COMPLETION(&ATECtrl->cmd_done);
#ifdef CONFIG_ATE
	ATECtrl->TxPower0 = pAd->EEPROMImage[TX0_G_BAND_TARGET_PWR];
	ATECtrl->TxPower1 = pAd->EEPROMImage[TX1_G_BAND_TARGET_PWR];
	DBGPRINT(RT_DEBUG_INFO, ("%s, Init Txpower, Tx0:%x, Tx1:%x\n", __FUNCTION__, ATECtrl->TxPower0, ATECtrl->TxPower1));
#ifdef CONFIG_QA
	AsicGetRxStat(pAd, HQA_RX_RESET_PHY_COUNT);
	ATECtrl->RxMacMdrdyCount = 0;
	ATECtrl->RxMacFCSErrCount = 0;
#endif /* CONFIG_QA */
#endif /* CONFIG_ATE */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_STOP_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);

	AsicSetMacTxRx(pAd, ASIC_MAC_RX, FALSE);
	
	/*  Disable TX PDMA */
	AsicSetWPDMA(pAd, PDMA_TX, 0);




#ifdef RTMP_MAC_USB

	/* Polling TX/RX path until packets empty usb need ??*/
	rtmp_tx_swq_exit(pAd, WCID_ALL);

	while (((pAd->BulkOutPending[0] == TRUE) ||
			(pAd->BulkOutPending[1] == TRUE) || 
			(pAd->BulkOutPending[2] == TRUE) ||
			(pAd->BulkOutPending[3] == TRUE)) && (pAd->BulkFlags != 0))
			/* pAd->BulkFlags != 0 : wait bulk out finish */	
	{
		do 
		{
			/* 
				pAd->BulkOutPending[y] will be set to FALSE
				in RTUSBCancelPendingBulkOutIRP(pAd)
			*/
			/* Rx out of Resouce in ATE */
			//RTUSBCancelPendingBulkOutIRP(pAd);
		} while (FALSE);			

	}

 	RTUSBCleanUpDataBulkOutQueue(pAd);

	/*
		Make sure there are no pending bulk in/out IRPs before we go on.
		pAd->BulkFlags != 0 : wait bulk out finish
	*/	
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		/* Rx out of Resouce in ATE */
				RtmpusecDelay(200);
		NdisZeroMemory(pRxContext->TransferBuffer, MAX_RXBULK_SIZE);

		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;

	}


//	ASSERT(pAd->PendingRx == 0);
	/* Clear ATE Bulk in/out counter and continue setup */
	InterlockedExchange(&pAd->BulkOutRemained, 0);

	/* NdisAcquireSpinLock()/NdisReleaseSpinLock() need only one argument in RT28xx */
	NdisAcquireSpinLock(&pAd->GenericLock);	
	pAd->ContinBulkOut = FALSE;		
	pAd->ContinBulkIn = FALSE;
	NdisReleaseSpinLock(&pAd->GenericLock);

	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_ATE);

#endif /* RTMP_MAC_USB */





	AsicSetWPDMA(pAd, PDMA_TX_RX, 1);

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);


#ifdef CAL_FREE_IC_SUPPORT
	RTMP_CAL_FREE_IC_CHECK(pAd,bCalFree);
	if (bCalFree)
	{
		DBGPRINT(RT_DEBUG_OFF, ("Cal Free IC!!\n"));
		RTMP_CAL_FREE_DATA_GET(pAd);
	}
	else
	{
		DBGPRINT(RT_DEBUG_OFF, ("Non Cal Free IC!!\n"));
	}
#endif /* CAL_FREE_IC_SUPPORT */

	if(ATECtrl->Mode & fATE_TXCONT_ENABLE){
		DBGPRINT(RT_DEBUG_OFF, ("%s ,Stop Continuous Tx\n",__FUNCTION__));	
		AsicStopContinousTx(pAd);
	}

	if((ATECtrl->Mode & fATE_TXCARRSUPP_ENABLE)||
	(ATECtrl->Mode & fATE_TXCARR_ENABLE)){
		DBGPRINT(RT_DEBUG_OFF, ("%s ,Stop Tx Carrier Test\n",__FUNCTION__));
    	AsicSetTxToneTest(pAd, 0, 0);
	}
	ATECtrl->Mode = ATE_START;
	return Ret;
}


static INT32 MT_ATEStop(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif
	UINT32 Index, RingNum;

	RXD_STRUC *pRxD = NULL;
#ifdef RT_BIG_ENDIAN
	RXD_STRUC *pDestRxD;
	UCHAR RxHwInfo[RXD_SIZE];
#endif


#ifdef RTMP_MAC_USB
INT32 i=0,acidx=0,ring_index=0;
#endif /* RTMP_MAC_USB */

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));


	AsicSetMacTxRx(pAd, ASIC_MAC_RXV, FALSE);

	NICInitializeAdapter(pAd, TRUE);

#ifdef RTMP_MAC_USB

	/* Polling TX/RX path until packets empty usb need ??*/
	rtmp_tx_swq_exit(pAd, WCID_ALL);

	while (((pAd->BulkOutPending[0] == TRUE) ||
			(pAd->BulkOutPending[1] == TRUE) || 
			(pAd->BulkOutPending[2] == TRUE) ||
			(pAd->BulkOutPending[3] == TRUE)) && (pAd->BulkFlags != 0))
			/* pAd->BulkFlags != 0 : wait bulk out finish */	
	{
		do 
		{
			/* 
				pAd->BulkOutPending[y] will be set to FALSE
				in RTUSBCancelPendingBulkOutIRP(pAd)
			*/
			/* Rx out of Resouce in ATE */
			//RTUSBCancelPendingBulkOutIRP(pAd);
		} while (FALSE);			

	}

 	RTUSBCleanUpDataBulkOutQueue(pAd);

	/*
		Make sure there are no pending bulk in/out IRPs before we go on.
		pAd->BulkFlags != 0 : wait bulk out finish
	*/	
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		/* Rx out of Resouce in ATE */
				RtmpusecDelay(200);
		NdisZeroMemory(pRxContext->TransferBuffer, MAX_RXBULK_SIZE);

		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;
	}



	/* Clear ATE Bulk in/out counter and continue setup. */
	InterlockedExchange(&pAd->BulkOutRemained, 0);				
	NdisAcquireSpinLock(&pAd->GenericLock);
	pAd->ContinBulkOut = FALSE;		
	pAd->ContinBulkIn = FALSE;
	NdisReleaseSpinLock(&pAd->GenericLock);		
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET);

	RTUSBBulkReceive(pAd);

#endif /* RTMP_MAC_USB */

/* if usb  call this two function , FW will hang~~ */

	AsicSetRxFilter(pAd);


	RTMPEnableRxTx(pAd);

	AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);

	RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_START_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif

	RTMP_OS_EXIT_COMPLETION(&ATECtrl->cmd_done);
	ATECtrl->Mode = ATE_STOP;

	if ((MTK_REV_GTE(pAd, MT7603, MT7603E1)) ||
		(MTK_REV_GTE(pAd, MT7628, MT7628E1)))
	{
		UINT32 Value;
		RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
		Value &= ~CR_RFINTF_CAL_NSS_MASK;
		Value |= CR_RFINTF_CAL_NSS(0x0);
		RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);  
	}	

	return Ret;
}





#ifdef RTMP_MAC_USB
static INT32 MT_ATESetupFrame(RTMP_ADAPTER *pAd, UINT32 TxIdx)
{

	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	TXINFO_STRUC *pTxInfo;
	TXWI_STRUC *pTxWI;
	UCHAR *buf;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
	UINT8 tx_hw_hdr_len = pAd->chipCap.tx_hw_hdr_len;
	MAC_TX_INFO mac_info;
	HTTRANSMIT_SETTING Transmit;
	ATECtrl->HLen = LENGTH_802_11;

	NdisZeroMemory(&pAd->NullFrame, 24);
	NdisZeroMemory(&Transmit, sizeof(Transmit));
	if (pAd->NullContext.InUse == FALSE)
	{
		pAd->NullContext.InUse = TRUE;

		pAd->NullFrame.FC.Type = FC_TYPE_DATA;
		pAd->NullFrame.FC.SubType = SUBTYPE_DATA_NULL;
		pAd->NullFrame.FC.ToDs = 1;
		COPY_MAC_ADDR(pAd->NullFrame.Addr1, ATECtrl->Addr1);
		COPY_MAC_ADDR(pAd->NullFrame.Addr2, ATECtrl->Addr2);
		COPY_MAC_ADDR(pAd->NullFrame.Addr3, ATECtrl->Addr3);
		DBGPRINT(RT_DEBUG_OFF, ("ATECtrl->Addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(ATECtrl->Addr1)));

		DBGPRINT(RT_DEBUG_OFF, ("ATECtrl->Addr2:%02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(ATECtrl->Addr2)));
		DBGPRINT(RT_DEBUG_OFF, ("ATECtrl->Addr3:%02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(ATECtrl->Addr3)));

		buf = &pAd->NullContext.TransferBuffer->field.WirelessPacket[0];
		RTMPZeroMemory(buf, 2048);
		pTxInfo = (TXINFO_STRUC *)buf;
		NdisZeroMemory((UCHAR *)&mac_info, sizeof(mac_info));	
		mac_info.FRAG = FALSE;
		
		mac_info.Ack = FALSE;
		mac_info.NSeq = FALSE;
		mac_info.hdr_len = ATECtrl->HLen;
		mac_info.hdr_pad = 0;
		mac_info.WCID = 0;
		mac_info.Length = ATECtrl->TxLength;
		mac_info.PID = 0;
		mac_info.bss_idx = 0;		
		mac_info.TID = 0;
		mac_info.prot = 0;

	   switch (ATECtrl->TxAntennaSel) {
	        case 0: /* Both */
	           mac_info.AntPri = 0;
	           mac_info.SpeEn = 1;
	           break;
	        case 1: /* TX0 */
	           mac_info.AntPri = 0;
	           mac_info.SpeEn = 0;
	           break;
	        case 2: /* TX1 */
	           mac_info.AntPri = 2; //b'010
	           mac_info.SpeEn = 0;
	           break;
	    }
		Transmit.field.MCS = ATECtrl->Mcs;
		Transmit.field.BW = ATECtrl->BW;
		Transmit.field.ShortGI = ATECtrl->Sgi;
		Transmit.field.STBC = ATECtrl->Stbc;
		Transmit.field.MODE = ATECtrl->PhyMode;

		mac_info.q_idx = Q_IDX_AC0;

		if (ATECtrl->PhyMode == MODE_CCK)
		{
			mac_info.Preamble = LONG_PREAMBLE;

			if (ATECtrl->Mcs == 9)
			{
				Transmit.field.MCS = 0;
				mac_info.Preamble = SHORT_PREAMBLE;	
			}
			else if (ATECtrl->Mcs == 10)
			{
				Transmit.field.MCS = 1;
				mac_info.Preamble = SHORT_PREAMBLE;	
			}
			else if (ATECtrl->Mcs == 11)
			{
				Transmit.field.MCS = 2;
				mac_info.Preamble = SHORT_PREAMBLE;	
			}
		}

		write_tmac_info(pAd, (UCHAR *)buf, &mac_info, &Transmit);
#ifdef MT7603
		// TODO: shiang-7603
		RTMPMoveMemory((VOID *)&buf[tx_hw_hdr_len], (VOID *)&pAd->NullFrame, ATECtrl->TxLength);
		pAd->NullContext.BulkOutSize = tx_hw_hdr_len + ATECtrl->TxLength + 4;	
#else
		RTMPMoveMemory((VOID *)&buf[TXWISize + TXINFO_SIZE], (VOID *)&pAd->NullFrame, ATECtrl->TxLength);
		pAd->NullContext.BulkOutSize = TXINFO_SIZE + TXWISize + TSO_SIZE + ATECtrl->TxLength + 4;
#endif /* MT7603 */
		pAd->NullContext.BulkOutSize = ( pAd->NullContext.BulkOutSize + 3) & (~3);

		printk("iversonate ate tx frame mke pAd->NullContext.BulkOutSize %d \n",pAd->NullContext.BulkOutSize);

	}
	return 0;
}
#endif



static INT32 MT_ATEStartTx(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	UINT32 Value = 0;
	INT32 Ret = 0;
#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif

#ifdef RTMP_MAC_USB
	ULONG			IrqFlags = 0;
#endif /* RTMP_MAC_USB */

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
	/* TxRx switch workaround */
	AsicSetRxPath(pAd, 0);
	AsicSetMacTxRx(pAd, ASIC_MAC_RX, TRUE);
	RtmpusecDelay(10000);
	AsicSetMacTxRx(pAd, ASIC_MAC_RX, FALSE);

	CmdChannelSwitch(pAd, ATECtrl->ControlChl, ATECtrl->Channel, ATECtrl->BW, 
							pAd->CommonCfg.TxStream, pAd->CommonCfg.RxStream);




	AsicSetMacTxRx(pAd, ASIC_MAC_RX_RXV, FALSE);

	/*   Stop send TX packets */
	RTMP_OS_NETDEV_STOP_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_STOP_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);
	
	/*  Disable PDMA */
	AsicSetWPDMA(pAd, PDMA_TX, 0);

	/* Polling TX/RX path until packets empty */

	/* Turn on RX again if set before */
	if (ATECtrl->Mode & ATE_RXFRAME)
		AsicSetMacTxRx(pAd, ASIC_MAC_RX_RXV, TRUE);

	RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);


#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_START_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif
	
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);


	/* Enable PDMA */
	AsicSetWPDMA(pAd, PDMA_TX_RX, 1);

	ATECtrl->Mode |= ATE_TXFRAME;



#ifdef RTMP_MAC_USB



	MT_ATESetupFrame(pAd, 0);

		if (ATECtrl->TxCount == 0)
		{
			InterlockedExchange(&pAd->BulkOutRemained, 0);
		}
		else
		{
			InterlockedExchange(&pAd->BulkOutRemained, ATECtrl->TxCount);
		}
	//	DBGPRINT(RT_DEBUG_TRACE, ("bulk out count = %d\n", atomic_read(&pAd->BulkOutRemained)));

		if (atomic_read(&pAd->BulkOutRemained) == 0)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Send packet continuously\n"));

			/* NdisAcquireSpinLock() == spin_lock_bh() */
			/* NdisAcquireSpinLock only need one argument. */
			NdisAcquireSpinLock(&pAd->GenericLock);
			pAd->ContinBulkOut = TRUE;		
			NdisReleaseSpinLock(&pAd->GenericLock);

			/* BULK_OUT_LOCK() == spin_lock_irqsave() */
			BULK_OUT_LOCK(&pAd->BulkOutLock[0], IrqFlags);
			pAd->BulkOutPending[0] = FALSE;
			BULK_OUT_UNLOCK(&pAd->BulkOutLock[0], IrqFlags);
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Send packets depend on counter\n"));

			NdisAcquireSpinLock(&pAd->GenericLock);
			pAd->ContinBulkOut = FALSE;		
			NdisReleaseSpinLock(&pAd->GenericLock);

			BULK_OUT_LOCK(&pAd->BulkOutLock[0], IrqFlags);
			pAd->BulkOutPending[0] = FALSE;
			BULK_OUT_UNLOCK(&pAd->BulkOutLock[0], IrqFlags);
		}				
		RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_ATE);
		/* Kick bulk out */


		RTUSBKickBulkOut(pAd);
	ATECtrl->Mode |= ATE_TXFRAME;
		/* Let pAd->BulkOutRemained be consumed to zero. */
#endif /* RTMP_MAC_USB */

	/* Low temperature high rate EVM degrade Patch v2 */	
	if ((MTK_REV_GTE(pAd, MT7603, MT7603E1)) ||
		(MTK_REV_GTE(pAd, MT7628, MT7628E1)))
	{
		if (ATECtrl->TxAntennaSel == 0)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x0);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);  
		}
		else if (ATECtrl->TxAntennaSel == 1)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x0);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);  
		}
		else if (ATECtrl->TxAntennaSel == 2)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x1);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);  
		}
	}

	return Ret;
}


static INT32 MT_ATEStartRx(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0, Value;
#ifdef CONFIG_AP_SUPPORT
	INT32 IdBss, MaxNumBss = pAd->ApCfg.BssidNum;
#endif
#ifdef RTMP_MAC_USB
	UINT32			ring_index=0;
#endif /* RTMP_MAC_USB */

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));


#ifdef RTMP_MAC_USB
	AsicSetMacTxRx(pAd, ASIC_MAC_RX, FALSE);
#endif /* RTMP_MAC_USB */



	/*   Stop send TX packets */
	RTMP_OS_NETDEV_STOP_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_STOP_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);

	AsicSetWPDMA(pAd, PDMA_TX, 0);

	
	RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if (MaxNumBss > MAX_MBSSID_NUM(pAd))
			MaxNumBss = MAX_MBSSID_NUM(pAd);

		/*  first IdBss must not be 0 (BSS0), must be 1 (BSS1) */
		for (IdBss = FIRST_MBSSID; IdBss < MAX_MBSSID_NUM(pAd); IdBss++)
		{
			if (pAd->ApCfg.MBSSID[IdBss].wdev.if_dev)
				RTMP_OS_NETDEV_START_QUEUE(pAd->ApCfg.MBSSID[IdBss].wdev.if_dev);
		}
	}
#endif
	
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DEQUEUEPACKET);


	/* Turn on TX again if set before */
	if (ATECtrl->Mode & ATE_TXFRAME)
	{
		AsicSetMacTxRx(pAd, ASIC_MAC_TX, TRUE);
	}
    /* reset counter when iwpriv only */
    if (ATECtrl->bQAEnabled != TRUE)
    {
        ATECtrl->RxTotalCnt = 0;
    }
	pAd->WlanCounters.FCSErrorCount.u.LowPart = 0;

	RTMP_IO_READ32(pAd, RMAC_RFCR, &Value);
	Value |= RM_FRAME_REPORT_EN;
	RTMP_IO_WRITE32(pAd, RMAC_RFCR, Value);


#ifdef RTMP_MAC_USB
	AsicSetMacTxRx(pAd, ASIC_MAC_RX, TRUE);
#endif /* RTMP_MAC_USB */


	/* Enable PDMA */
	AsicSetWPDMA(pAd, PDMA_TX_RX, 1);


	ATECtrl->Mode |= ATE_RXFRAME;

#ifdef RTMP_MAC_USB
    /* Reset Rx RING */
	for (ring_index = 0; ring_index < (RX_RING_SIZE); ring_index++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[ring_index]);


		/* Get the URB from kernel(i.e., host control driver) back to driver. */
		/* Rx out of Resouce in ATE */
		//RTUSB_UNLINK_URB(pRxContext->pUrb);

		/* Sleep 200 microsecs to give cancellation time to work. */
		RtmpusecDelay(200);

		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;


	}
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET);
	pAd->ContinBulkIn = TRUE;
	/* Kick bulk in. */
	RTUSBBulkReceive(pAd);
#endif /* RTMP_MAC_USB */

printk("<-----iverson MT_ATEStartRx  \n ");

	return Ret;
}


static INT32 MT_ATEStopTx(RTMP_ADAPTER *pAd, UINT32 Mode)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
	INT32 acidx;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

	if (Mode == ATE_TXCARR)
	{



	}
	else if (Mode == ATE_TXCARRSUPP)
	{


	}
	else if ((Mode & ATE_TXFRAME) || (Mode == ATE_STOP))
	{
		if (Mode == ATE_TXCONT)
		{


		}

		/*  Disable PDMA */
		AsicSetWPDMA(pAd, PDMA_TX_RX, 0);
#ifdef RTMP_MAC_USB
	for(acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{                        
		PHT_TX_CONTEXT  pHTTXContext = &(pAd->TxContext[acidx]);
		pHTTXContext->IRPPending = FALSE;
		pHTTXContext->NextBulkOutPosition = 0;
		pHTTXContext->ENextBulkOutPosition = 0;
		pHTTXContext->CurWritePosition = 0;
		pHTTXContext->CurWriteRealPos = 0;
		pHTTXContext->BulkOutSize = 0;
		pHTTXContext->bRingEmpty = TRUE;
		pHTTXContext->bCopySavePad = FALSE;
		pAd->BulkOutPending[acidx] = FALSE;
	}
	RTUSBCleanUpDataBulkOutQueue(pAd);

	/* Abort Tx, RX DMA. */
	RtmpDmaEnable(pAd, 0);
	ATE_RTUSBCancelPendingBulkInIRP(pAd);
	RtmpOsMsDelay(500);

	while (((pAd->BulkOutPending[0] == TRUE) ||
			(pAd->BulkOutPending[1] == TRUE) || 
			(pAd->BulkOutPending[2] == TRUE) ||
			(pAd->BulkOutPending[3] == TRUE)) && (pAd->BulkFlags != 0))	
			/* pAd->BulkFlags != 0 : wait bulk out finish */
	{
		do 
		{	
			/* Rx out of Resouce in ATE */
			//RTUSBCancelPendingBulkOutIRP(pAd);
		} while (FALSE);			

		RtmpOsMsDelay(500);
	}

#endif /* RTMP_MAC_USB */


		ATECtrl->Mode &= ~ATE_TXFRAME;

		/* Enable PDMA */
		AsicSetWPDMA(pAd, PDMA_TX_RX, 1);
	}

	return Ret;
}


static INT32 MT_ATEStopRx(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
	INT32 acidx = 0, i = 0;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

	Ret = AsicSetMacTxRx(pAd, ASIC_MAC_RX_RXV, FALSE);

    ATECtrl->Mode &= ~ATE_RXFRAME;
#ifdef RTMP_MAC_USB
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		/* Rx out of Resouce in ATE */
				RtmpusecDelay(200);

//		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;

	}

	pAd->ContinBulkIn = FALSE;
#endif /* RTMP_MAC_USB */


	return Ret;
}


static INT32 MT_ATESetTxAntenna(RTMP_ADAPTER *pAd, CHAR Ant)
{

	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
	UINT32 Value;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
    /* 0: All 1:TX0 2:TX1 */
    ATECtrl->TxAntennaSel = Ant;

	if ((MTK_REV_GTE(pAd, MT7603, MT7603E1)) ||
		(MTK_REV_GTE(pAd, MT7628, MT7628E1))||
		(MTK_REV_GTE(pAd, MT7628, MT7603E2)))
	{
		if (ATECtrl->TxAntennaSel == 0)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x0);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);
			/* Tx both patch, ePA/eLNA/, iPA/eLNA, iPA/iLNA */
			{

				UINT32 RemapBase, RemapOffset;
				UINT32 RestoreValue;
				UINT32 value;
				RTMP_IO_READ32(pAd, MCU_PCIE_REMAP_2, &RestoreValue);
				RemapBase = GET_REMAP_2_BASE(0x81060008) << 19;
				RemapOffset = GET_REMAP_2_OFFSET(0x81060008);
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RemapBase);
				if (MTK_REV_GTE(pAd, MT7603, MT7603E1)||
					MTK_REV_GTE(pAd, MT7603, MT7603E2)){
					value = 0x04852390;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}else if(MTK_REV_GTE(pAd, MT7628, MT7628E1)){
					value = 0x00489523;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RestoreValue);

			}
		}
		else if (ATECtrl->TxAntennaSel == 1)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x0);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);

			/* Tx0 patch, ePA/eLNA/, iPA/eLNA, iPA/iLNA */
			{
				UINT32 RemapBase, RemapOffset;
				UINT32 RestoreValue;
				UINT32 value;
				RTMP_IO_READ32(pAd, MCU_PCIE_REMAP_2, &RestoreValue);
				RemapBase = GET_REMAP_2_BASE(0x81060008) << 19;
				RemapOffset = GET_REMAP_2_OFFSET(0x81060008);
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RemapBase);
				if (MTK_REV_GTE(pAd, MT7603, MT7603E1)||
					MTK_REV_GTE(pAd, MT7603, MT7603E2)){
					value = 0x04852390;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}else if(MTK_REV_GTE(pAd, MT7628, MT7628E1)){
					value = 0x00489523;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RestoreValue);
			}
		}
		else if (ATECtrl->TxAntennaSel == 2)
		{
			RTMP_IO_READ32(pAd, CR_RFINTF_00, &Value);
			Value &= ~CR_RFINTF_CAL_NSS_MASK;
			Value |= CR_RFINTF_CAL_NSS(0x1);
			RTMP_IO_WRITE32(pAd, CR_RFINTF_00, Value);
			/* Tx1 patch, ePA/eLNA/, iPA/eLNA, iPA/iLNA */
			{
				UINT32 RemapBase, RemapOffset;
				UINT32 RestoreValue;
				UINT32 value;
				RTMP_IO_READ32(pAd, MCU_PCIE_REMAP_2, &RestoreValue);
				RemapBase = GET_REMAP_2_BASE(0x81060008) << 19;
				RemapOffset = GET_REMAP_2_OFFSET(0x81060008);
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RemapBase);
				if (MTK_REV_GTE(pAd, MT7603, MT7603E1)||
					MTK_REV_GTE(pAd, MT7603, MT7603E2)){
					value = 0x04856790;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}else if(MTK_REV_GTE(pAd, MT7628, MT7628E1)){
					value = 0x00489567;
					RTMP_IO_WRITE32(pAd, 0x80000 + RemapOffset, value);
				}	
				RTMP_IO_WRITE32(pAd, MCU_PCIE_REMAP_2, RestoreValue);
			}
		}
	}

	return Ret;
}


static INT32 MT_ATESetRxAntenna(RTMP_ADAPTER *pAd, CHAR Ant)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
    ATECtrl->RxAntennaSel = Ant;
    /* set RX path */
    AsicSetRxPath(pAd, (UINT32)ATECtrl->RxAntennaSel);
	return Ret;
}


static INT32 MT_ATESetTxFreqOffset(RTMP_ADAPTER *pAd, UINT32 FreqOffset)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    ATECtrl->RFFreqOffset = FreqOffset;
	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

    AsicSetRfFreqOffset(pAd, ATECtrl->RFFreqOffset);

	return Ret;
}

static INT32 MT_ATESetChannel(RTMP_ADAPTER *pAd, INT16 Value)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

	CmdChannelSwitch(pAd, ATECtrl->ControlChl, ATECtrl->Channel, ATECtrl->BW, 
							pAd->CommonCfg.TxStream, pAd->CommonCfg.RxStream);	

	return Ret;
}


static INT32 MT_ATESetBW(RTMP_ADAPTER *pAd, INT16 Value)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    UINT32 val = 0;

	DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));

	RTMP_IO_READ32(pAd, AGG_BWCR, &val);
	val &= (~0x0c);
	switch (Value)
	{
		case BW_20:
			val |= (0);
			break;
		case BW_40:
			val |= (0x1 << 2);
			break;
		case BW_80:
			val |= (0x2 << 2);
			break;
	}
	RTMP_IO_WRITE32(pAd, AGG_BWCR, val);
    /* TODO: check CMD_CH_PRIV_ACTION_BW_REQ */
	//CmdChPrivilege(pAd, CMD_CH_PRIV_ACTION_BW_REQ, ATECtrl->ControlChl, ATECtrl->Channel,
	//					ATECtrl->BW, pAd->CommonCfg.TxStream, pAd->CommonCfg.RxStream);


	return Ret;
}


static INT32 MT_ATESampleRssi(RTMP_ADAPTER *pAd, RX_BLK *RxBlk)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;

	if (RxBlk->rx_signal.raw_rssi[0] != 0)
	{
		ATECtrl->LastRssi0	= ConvertToRssi(pAd,
			(struct raw_rssi_info *)(&RxBlk->rx_signal.raw_rssi[0]), RSSI_IDX_0);

		ATECtrl->AvgRssi0X8 = (ATECtrl->AvgRssi0X8 - ATECtrl->AvgRssi0)
									+ ATECtrl->LastRssi0;
		ATECtrl->AvgRssi0 = ATECtrl->AvgRssi0X8 >> 3;
	}

	if (RxBlk->rx_signal.raw_rssi[1] != 0)
	{
		ATECtrl->LastRssi1 = ConvertToRssi(pAd,
			(struct raw_rssi_info *)(&RxBlk->rx_signal.raw_rssi[0]), RSSI_IDX_1);

		ATECtrl->AvgRssi1X8 = (ATECtrl->AvgRssi1X8 - ATECtrl->AvgRssi1)
									+ ATECtrl->LastRssi1;
		ATECtrl->AvgRssi1 = ATECtrl->AvgRssi1X8 >> 3;
	}

	ATECtrl->LastSNR0 = RxBlk->rx_signal.raw_snr[0];;
	ATECtrl->LastSNR1 = RxBlk->rx_signal.raw_snr[1];

	ATECtrl->NumOfAvgRssiSample++;

	return Ret;
}

static INT32 MT_ATESetAIFS(RTMP_ADAPTER *pAd, CHAR Value)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    /* Test mode use AC0 for TX */
    AsicSetWmmParam(pAd, WMM_PARAM_AC_0, WMM_PARAM_AIFSN, Value);
    return Ret;
}

static INT32 MT_ATESetTSSI(RTMP_ADAPTER *pAd, CHAR WFSel, CHAR Setting)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    Ret = AsicSetTSSI(pAd, Setting, WFSel);
	return Ret;
}

static INT32 MT_ATESetDPD(RTMP_ADAPTER *pAd, CHAR WFSel, CHAR Setting)
{
    /* !!TEST MODE ONLY!! Normal Mode control by FW and Never disable */
    /* WF0 = 0, WF1 = 1, WF ALL = 2 */
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    Ret = AsicSetDPD(pAd, Setting, WFSel);
    return Ret;
}

static INT32 MT_ATEStartTxTone(RTMP_ADAPTER *pAd, UINT32 Mode)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
	AsicSetTxToneTest(pAd, 1, Mode);
	return Ret;
}
static INT32 MT_ATESetTxTonePower(RTMP_ADAPTER *pAd, INT32 pwr1, INT32 pwr2)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    DBGPRINT(RT_DEBUG_OFF, ("%s, pwr1:%d, pwr2:%d\n", __FUNCTION__, pwr1, pwr2));
	AsicSetTxTonePower(pAd, pwr1, pwr2);
	return Ret;
}

static INT32 MT_ATEStopTxTone(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
	AsicSetTxToneTest(pAd, 0, 0);
    return Ret;
}

static INT32 MT_ATEStartContinousTx(RTMP_ADAPTER *pAd, CHAR WFSel)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;
	INT32 Ret = 0;
    DBGPRINT(RT_DEBUG_OFF, ("%s\n", __FUNCTION__));
    AsicStartContinousTx(pAd, ATECtrl->PhyMode, ATECtrl->BW, ATECtrl->ControlChl, ATECtrl->Mcs, WFSel);
    return Ret;
}


static INT32 MT_RfRegWrite(RTMP_ADAPTER *pAd, UINT32 WFSel, UINT32 Offset, UINT32 Value)
{
	INT32 Ret = 0;

	Ret = CmdRFRegAccessWrite(pAd, WFSel, Offset, Value);

	return Ret;
}


static INT32 MT_RfRegRead(RTMP_ADAPTER *pAd, UINT32 WFSel, UINT32 Offset, UINT32 *Value)
{
	INT32 Ret = 0;

	Ret = CmdRFRegAccessRead(pAd, WFSel, Offset, Value);

	return Ret;
}


static INT32 MT_GetFWInfo(RTMP_ADAPTER *pAd, UCHAR *FWInfo)
{

	RTMP_CHIP_CAP *cap = &pAd->chipCap;

	memcpy(FWInfo, cap->FWImageName + cap->fw_len - 36, 36);

	return 0;
}


static ATE_OPERATION MT_ATEOp = {
	.ATEStart = MT_ATEStart,
	.ATEStop = MT_ATEStop,
	.StartTx = MT_ATEStartTx,
	.StartRx = MT_ATEStartRx,
	.StopTx = MT_ATEStopTx,
	.StopRx = MT_ATEStopRx,
	.SetTxPower0 = MT_ATESetTxPower0,
	.SetTxPower1 = MT_ATESetTxPower1,
	.SetTxAntenna = MT_ATESetTxAntenna,
	.SetRxAntenna = MT_ATESetRxAntenna,
	.SetTxFreqOffset = MT_ATESetTxFreqOffset,
	.SetChannel = MT_ATESetChannel,
	.SetBW = MT_ATESetBW,
	.SampleRssi = MT_ATESampleRssi,
	.SetAIFS = MT_ATESetAIFS,
	.SetTSSI = MT_ATESetTSSI,
	.SetDPD = MT_ATESetDPD,
	.StartTxTone = MT_ATEStartTxTone,
	.SetTxTonePower = MT_ATESetTxTonePower,
	.StopTxTone = MT_ATEStopTxTone,
	.StartContinousTx = MT_ATEStartContinousTx,
	.RfRegWrite = MT_RfRegWrite,
	.RfRegRead = MT_RfRegRead,
	.GetFWInfo = MT_GetFWInfo,
};


INT32 MT_ATEInit(RTMP_ADAPTER *pAd)
{
	ATE_CTRL *ATECtrl = &pAd->ATECtrl;

	ATECtrl->ATEOp = &MT_ATEOp;

	return 0;
}
