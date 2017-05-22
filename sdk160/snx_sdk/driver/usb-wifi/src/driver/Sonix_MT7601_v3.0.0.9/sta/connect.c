/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	connect.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John			2004-08-08			Major modification from RT2560
*/
#include "rt_config.h"
#ifdef DOT11R_FT_SUPPORT
#include "ft.h"
#endif /* DOT11R_FT_SUPPORT */

UCHAR CipherSuiteWpaNoneTkip[] = {
	0x00, 0x50, 0xf2, 0x01,	/* oui */
	0x01, 0x00,		/* Version */
	0x00, 0x50, 0xf2, 0x02,	/* Multicast */
	0x01, 0x00,		/* Number of unicast */
	0x00, 0x50, 0xf2, 0x02,	/* unicast */
	0x01, 0x00,		/* number of authentication method */
	0x00, 0x50, 0xf2, 0x00	/* authentication */
};
UCHAR CipherSuiteWpaNoneTkipLen =
    (sizeof (CipherSuiteWpaNoneTkip) / sizeof (UCHAR));

UCHAR CipherSuiteWpaNoneAes[] = {
	0x00, 0x50, 0xf2, 0x01,	/* oui */
	0x01, 0x00,		/* Version */
	0x00, 0x50, 0xf2, 0x04,	/* Multicast */
	0x01, 0x00,		/* Number of unicast */
	0x00, 0x50, 0xf2, 0x04,	/* unicast */
	0x01, 0x00,		/* number of authentication method */
	0x00, 0x50, 0xf2, 0x00	/* authentication */
};
UCHAR CipherSuiteWpaNoneAesLen =
    (sizeof (CipherSuiteWpaNoneAes) / sizeof (UCHAR));

/* The following MACRO is called after 1. starting an new IBSS, 2. succesfully JOIN an IBSS, */
/* or 3. succesfully ASSOCIATE to a BSS, 4. successfully RE_ASSOCIATE to a BSS */
/* All settings successfuly negotiated furing MLME state machines become final settings */
/* and are copied to pAd->StaActive */
#define COPY_SETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(_pAd)                                 \
{                                                                                       \
	NdisZeroMemory((_pAd)->CommonCfg.Ssid, MAX_LEN_OF_SSID); 							\
	(_pAd)->CommonCfg.SsidLen = (_pAd)->MlmeAux.SsidLen;                                \
	NdisMoveMemory((_pAd)->CommonCfg.Ssid, (_pAd)->MlmeAux.Ssid, (_pAd)->MlmeAux.SsidLen); \
	COPY_MAC_ADDR((_pAd)->CommonCfg.Bssid, (_pAd)->MlmeAux.Bssid);                      \
	(_pAd)->CommonCfg.Channel = (_pAd)->MlmeAux.Channel;                                \
	(_pAd)->CommonCfg.CentralChannel = (_pAd)->MlmeAux.CentralChannel;                  \
	(_pAd)->StaActive.Aid = (_pAd)->MlmeAux.Aid;                                        \
	(_pAd)->StaActive.AtimWin = (_pAd)->MlmeAux.AtimWin;                                \
	(_pAd)->StaActive.CapabilityInfo = (_pAd)->MlmeAux.CapabilityInfo;                  \
	(_pAd)->StaActive.ExtCapInfo = (_pAd)->MlmeAux.ExtCapInfo;                  \
	(_pAd)->CommonCfg.BeaconPeriod = (_pAd)->MlmeAux.BeaconPeriod;                      \
	(_pAd)->StaActive.CfpMaxDuration = (_pAd)->MlmeAux.CfpMaxDuration;                  \
	(_pAd)->StaActive.CfpPeriod = (_pAd)->MlmeAux.CfpPeriod;                            \
	(_pAd)->StaActive.SupRateLen = (_pAd)->MlmeAux.SupRateLen;                          \
	NdisMoveMemory((_pAd)->StaActive.SupRate, (_pAd)->MlmeAux.SupRate, (_pAd)->MlmeAux.SupRateLen);\
	(_pAd)->StaActive.ExtRateLen = (_pAd)->MlmeAux.ExtRateLen;                          \
	NdisMoveMemory((_pAd)->StaActive.ExtRate, (_pAd)->MlmeAux.ExtRate, (_pAd)->MlmeAux.ExtRateLen);\
	NdisMoveMemory(&(_pAd)->CommonCfg.APEdcaParm, &(_pAd)->MlmeAux.APEdcaParm, sizeof(EDCA_PARM));\
	NdisMoveMemory(&(_pAd)->CommonCfg.APQosCapability, &(_pAd)->MlmeAux.APQosCapability, sizeof(QOS_CAPABILITY_PARM));\
	NdisMoveMemory(&(_pAd)->CommonCfg.APQbssLoad, &(_pAd)->MlmeAux.APQbssLoad, sizeof(QBSS_LOAD_PARM));\
	COPY_MAC_ADDR((_pAd)->MacTab.Content[BSSID_WCID].Addr, (_pAd)->MlmeAux.Bssid);      \
	(_pAd)->MacTab.Content[BSSID_WCID].PairwiseKey.CipherAlg = (_pAd)->StaCfg.PairCipher;\
	COPY_MAC_ADDR((_pAd)->MacTab.Content[BSSID_WCID].PairwiseKey.BssId, (_pAd)->MlmeAux.Bssid);\
	(_pAd)->MacTab.Content[BSSID_WCID].RateLen = (_pAd)->StaActive.SupRateLen + (_pAd)->StaActive.ExtRateLen;\
}

/*
	==========================================================================
	Description:

	IRQL = PASSIVE_LEVEL

	==========================================================================
*/
VOID MlmeCntlInit(
	IN PRTMP_ADAPTER pAd,
	IN STATE_MACHINE *S,
	OUT STATE_MACHINE_FUNC Trans[])
{
	/* Control state machine differs from other state machines, the interface */
	/* follows the standard interface */
	pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID MlmeCntlMachinePerformAction(
	IN PRTMP_ADAPTER pAd,
	IN STATE_MACHINE *S,
	IN MLME_QUEUE_ELEM *Elem)
{
	switch (pAd->Mlme.CntlMachine.CurrState) {
	case CNTL_IDLE:
		CntlIdleProc(pAd, Elem);
		break;
	case CNTL_WAIT_DISASSOC:
		CntlWaitDisassocProc(pAd, Elem);
		break;
	case CNTL_WAIT_JOIN:
		CntlWaitJoinProc(pAd, Elem);
		break;

		/* CNTL_WAIT_REASSOC is the only state in CNTL machine that does */
		/* not triggered directly or indirectly by "RTMPSetInformation(OID_xxx)". */
		/* Therefore not protected by NDIS's "only one outstanding OID request" */
		/* rule. Which means NDIS may SET OID in the middle of ROAMing attempts. */
		/* Current approach is to block new SET request at RTMPSetInformation() */
		/* when CntlMachine.CurrState is not CNTL_IDLE */
	case CNTL_WAIT_REASSOC:
		CntlWaitReassocProc(pAd, Elem);
		break;

	case CNTL_WAIT_START:
		CntlWaitStartProc(pAd, Elem);
		break;
	case CNTL_WAIT_AUTH:
		CntlWaitAuthProc(pAd, Elem);
		break;
	case CNTL_WAIT_AUTH2:
		CntlWaitAuthProc2(pAd, Elem);
		break;
	case CNTL_WAIT_ASSOC:
		CntlWaitAssocProc(pAd, Elem);
		break;

	case CNTL_WAIT_OID_LIST_SCAN:
		if (Elem->MsgType == MT2_SCAN_CONF) {
			USHORT	Status = MLME_SUCCESS;
				
			NdisMoveMemory(&Status, Elem->Msg, sizeof(USHORT));
				
			/* Resume TxRing after SCANING complete. We hope the out-of-service time */
			/* won't be too long to let upper layer time-out the waiting frames */
			RTMPResumeMsduTransmission(pAd);

			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

			/* scan completed, init to not FastScan */
			pAd->StaCfg.bImprovedScan = FALSE;

#if 0 // already on scan_ch_restore() for ra0 ps wakeup
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) && (INFRA_ON(pAd)) 
				&& ((pAd->LatchRfRegs.Channel == pAd->CommonCfg.Channel) ||(pAd->LatchRfRegs.Channel == pAd->CommonCfg.CentralChannel))
#ifdef CONFIG_MULTI_CHANNEL
				&& (pAd->Multi_Channel_Enable == FALSE)
#endif /* CONFIG_MULTI_CHANNEL */
)
			{
				RTMPSendNullFrame(pAd, 
					pAd->CommonCfg.TxRate, 
					(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
					PWR_ACTIVE);

				DBGPRINT(RT_DEBUG_TRACE, ("MT2_SCAN_CONF INFRA_ON -- Send PSM Data frame for off channel RM, SCAN_IN_PROGRESS=%d!\n",
											RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)));

				OS_WAIT(20);
				DBGPRINT(RT_DEBUG_TRACE, ("%s -- Send null frame pAd->StaCfg.Psm=%d\n", __FUNCTION__,pAd->StaCfg.Psm));

			}
#endif


//use for normal concurrent case!
#ifdef P2P_SUPPORT
		if ((P2P_CLI_ON(pAd))
			&& ((pAd->LatchRfRegs.Channel == pAd->ApCliMlmeAux.Channel) || (pAd->LatchRfRegs.Channel == pAd->ApCliMlmeAux.CentralChannel)))
		{
						RTMPP2PSendNullFrame(pAd, 
										  pAd->CommonCfg.TxRate, 
										  (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
										  pAd->CommonCfg.bAPSDForcePowerSave ? PWR_SAVE : pAd->StaCfg.Psm);
			DBGPRINT(RT_DEBUG_TRACE, ("MT2_SCAN_CONF P2P_CLI_ON -- Send PSM Data frame for off channel RM, SCAN_IN_PROGRESS=%d!\n",
											RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)));
			OS_WAIT(20);
			DBGPRINT(RT_DEBUG_TRACE, ("%s -- Send null frame pAd->StaCfg.Psm=%d\n", __FUNCTION__,pAd->StaCfg.Psm));
		}
#endif

			
#ifdef LED_CONTROL_SUPPORT
			/* */
			/* Set LED status to previous status. */
			/* */
			if (pAd->LedCntl.bLedOnScanning) {
				pAd->LedCntl.bLedOnScanning = FALSE;
				RTMPSetLED(pAd, pAd->LedCntl.LedStatus);
			}
#endif /* LED_CONTROL_SUPPORT */

#ifdef DOT11N_DRAFT3
			/* AP sent a 2040Coexistence mgmt frame, then station perform a scan, and then send back the respone. */
			if ((pAd->CommonCfg.BSSCoexist2040.field.InfoReq == 1)
			    && INFRA_ON(pAd)
			    && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SCAN_2040)) {
				Update2040CoexistFrameAndNotify(pAd, BSSID_WCID,
								TRUE);
			}
#endif /* DOT11N_DRAFT3 */
#if 0   
			// driver auto connect should be no need for supplicant support
                                if (pAd->IndicateMediaState != NdisMediaStateConnected && (pAd->StaCfg.WpaSupplicantUP != WPA_SUPPLICANT_ENABLE_WITH_WEB_UI) )
                                {
                                        BssTableSsidSort(pAd, &pAd->MlmeAux.SsidBssTab, (PCHAR)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
                                        pAd->MlmeAux.BssIdx = 0;
                                        IterateOnBssTab(pAd);
                                }
#endif 


				if (Status == MLME_SUCCESS)
				{
#ifdef WIDI_SUPPORT
					if (pAd->MlmeAux.OldChannel == 0)
#endif /* WIDI_SUPPORT */
					{
						/* 
							Maintain Scan Table 
							MaxBeaconRxTimeDiff: 120 seconds
							MaxSameBeaconRxTimeCount: 1
						*/
						MaintainBssTable(pAd, &pAd->ScanTab, 120, 2);
					}
					
#ifdef P2P_SUPPORT
				if (!((pAd->MlmeAux.ScanType == SCAN_P2P_SEARCH) || (pAd->MlmeAux.ScanType == SCAN_P2P)))
#endif /* P2P_SUPPORT */
				{
					RTMPSendWirelessEvent(pAd, IW_SCAN_COMPLETED_EVENT_FLAG, NULL, BSS0, 0);

#ifdef WPA_SUPPLICANT_SUPPORT
					RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_SCAN, -1, NULL, NULL, 0);
#endif /* WPA_SUPPLICANT_SUPPORT */
				}

#ifdef P2P_SUPPORT
				if (pAd->P2pCfg.P2pCounter.bStartScan &&
					((pAd->MlmeAux.ScanType == SCAN_P2P) || (pAd->MlmeAux.ScanType == SCAN_P2P_SEARCH)))
				{
#ifdef DPA_S
					if (pAd->MlmeAux.ScanType == SCAN_P2P)
						RtmpOSWrielessEventSend(pAd->p2p_dev, RT_WLAN_EVENT_CUSTOM, P2P_NOTIF_DISCOVER_START_SEARCH_LISTEN, NULL, NULL, 0);
					if (pAd->MlmeAux.ScanType == SCAN_P2P_SEARCH)
						RtmpOSWrielessEventSend(pAd->p2p_dev, RT_WLAN_EVENT_CUSTOM, P2P_NOTIF_DISCOVER_SEARCH_LISTEN_ITERATION, NULL, NULL, 0);
#endif /* DPA_S */

					if (P2P_GO_ON(pAd))
					{
						P2PSetNextScanTimer(pAd, 10);
					}
					else
					{
						if (INFRA_ON(pAd))
							RtmpOsMsDelay(110);
#ifdef CONFIG_MULTI_CHANNEL
						if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) && (INFRA_ON(pAd)) 
							&& ((pAd->LatchRfRegs.Channel == pAd->CommonCfg.Channel) ||(pAd->LatchRfRegs.Channel == pAd->CommonCfg.CentralChannel))
							&& (pAd->Multi_Channel_Enable == TRUE))
						{
										RTMPSendNullFrame(pAd, 
														  pAd->CommonCfg.TxRate, 
														  (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
														  PWR_SAVE);

							DBGPRINT(RT_DEBUG_TRACE, ("-- Send null frame before listen\n"));
							OS_WAIT(200);
							
						}
#endif /*CONFIG_MULTI_CHANNEL*/

						P2PSetListenTimer(pAd, 0);
					}
				}
				pAd->P2pCfg.bPeriodicListen = TRUE;
#endif /* P2P_SUPPORT */
			}
#ifdef IWSC_SUPPORT
			if (pAd->StaCfg.BssType == BSS_ADHOC)
			{
				MlmeEnqueue(pAd, IWSC_STATE_MACHINE, IWSC_MT2_MLME_SCAN_DONE, 0, NULL, 0);
				RTMP_MLME_HANDLER(pAd);
			}
#endif /* IWSC_SUPPORT */
		}

#ifdef ED_MONITOR
				if (pAd->ed_chk)
				{
					if (pAd->ScanTab.BssNr > pAd->ed_ap_scaned)
					{
						DBGPRINT(RT_DEBUG_ERROR, ("@@@ %s : pAd->ScanTab.BssNr =%u, pAd->ed_ap_scaned=%u, go to ed_monitor_exit()!! \n", __FUNCTION__,pAd->ScanTab.BssNr, pAd->ed_ap_scaned));
						ed_monitor_exit(pAd);
					}
				}
#endif

		break;

	case CNTL_WAIT_OID_DISASSOC:
		if (Elem->MsgType == MT2_DISASSOC_CONF) {
			LinkDown(pAd, FALSE);
/* 
for android system , if connect ap1 and want to change to ap2 , 
when disassoc from ap1 ,and send even_scan will direct connect to ap2 , not need to wait ui to scan and connect
*/
#if 0
#ifdef ANDROID_SUPPORT
			RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_SCAN, -1, NULL, NULL, 0);
#endif /* ANDROID_SUPPORT */			
#endif
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
		}
		break;

	case CNTL_WAIT_SCAN_FOR_CONNECT:
		if (Elem->MsgType == MT2_SCAN_CONF) {
			USHORT	Status = MLME_SUCCESS;
			NdisMoveMemory(&Status, Elem->Msg, sizeof(USHORT));
			/* Resume TxRing after SCANING complete. We hope the out-of-service time */
			/* won't be too long to let upper layer time-out the waiting frames */
			RTMPResumeMsduTransmission(pAd);
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			BssTableSsidSort(pAd, &pAd->MlmeAux.SsidBssTab, (PCHAR)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			pAd->MlmeAux.BssIdx = 0;
			IterateOnBssTab(pAd);
		}
		break;
#if 0		
#ifdef RTMP_MAC_USB
		/* */
		/* This state is for that we want to connect to an AP but */
		/* it didn't find on BSS List table. So we need to scan the air first, */
		/* after that we can try to connect to the desired AP if available. */
		/* */
	case CNTL_WAIT_SCAN_FOR_CONNECT:
		if (Elem->MsgType == MT2_SCAN_CONF) {
			/* Resume TxRing after SCANING complete. We hope the out-of-service time */
			/* won't be too long to let upper layer time-out the waiting frames */
			RTMPResumeMsduTransmission(pAd);
#ifdef CCX_SUPPORT
			if (pAd->StaCfg.CCXReqType != MSRN_TYPE_UNUSED) {
				/* Cisco scan request is finished, prepare beacon report */
				MlmeEnqueue(pAd, AIRONET_STATE_MACHINE,
					    MT2_AIRONET_SCAN_DONE, 0, NULL, 0);
			}
#endif /* CCX_SUPPORT */
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

			/* */
			/* Check if we can connect to. */
			/* */
			BssTableSsidSort(pAd, &pAd->MlmeAux.SsidBssTab,
					 (CHAR *) pAd->MlmeAux.
					 AutoReconnectSsid,
					 pAd->MlmeAux.AutoReconnectSsidLen);

#ifdef WSC_STA_SUPPORT
#ifdef WSC_LED_SUPPORT
			if ((pAd->MlmeAux.SsidBssTab.BssNr == 0)
			    && (pAd->StaCfg.BssType == BSS_INFRA)) {
				LEDConnectionStart(pAd);
				LEDConnectionCompletion(pAd, FALSE);
			}
#endif /* WSC_LED_SUPPORT */
#endif /* WSC_STA_SUPPORT */

			if (pAd->MlmeAux.SsidBssTab.BssNr > 0) {
				MlmeAutoReconnectLastSSID(pAd);
			}
		}
		break;
#endif /* RTMP_MAC_USB */
#endif
	default:
		DBGPRINT_ERR(("!ERROR! CNTL - Illegal message type(=%ld)",
			      Elem->MsgType));
		break;
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlIdleProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_DISASSOC_REQ_STRUCT DisassocReq;

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

	switch (Elem->MsgType) {
	case OID_802_11_SSID:
		CntlOidSsidProc(pAd, Elem);
		break;

	case OID_802_11_BSSID:
		CntlOidRTBssidProc(pAd, Elem);
		break;

	case OID_802_11_BSSID_LIST_SCAN:
		CntlOidScanProc(pAd, Elem);
		break;

	case OID_802_11_DISASSOCIATE:
		DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid,
				 REASON_DISASSOC_STA_LEAVING);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
			    sizeof (MLME_DISASSOC_REQ_STRUCT), &DisassocReq, 0);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
#ifdef WPA_SUPPLICANT_SUPPORT
		if ((pAd->StaCfg.WpaSupplicantUP & 0x7F) !=
		    WPA_SUPPLICANT_ENABLE_WITH_WEB_UI)
#endif /* WPA_SUPPLICANT_SUPPORT */
		{
			/* Set the AutoReconnectSsid to prevent it reconnect to old SSID */
			/* Since calling this indicate user don't want to connect to that SSID anymore. */
			pAd->MlmeAux.AutoReconnectSsidLen = 32;
			NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid,
				       pAd->MlmeAux.AutoReconnectSsidLen);
		}
		break;

	case MT2_MLME_ROAMING_REQ:
		CntlMlmeRoamingProc(pAd, Elem);
		break;

	case OID_802_11_MIC_FAILURE_REPORT_FRAME:
		WpaMicFailureReportFrame(pAd, Elem);
		break;

#ifdef QOS_DLS_SUPPORT
	case RT_OID_802_11_SET_DLS_PARAM:
		CntlOidDLSSetupProc(pAd, Elem);
		break;
#endif /* QOS_DLS_SUPPORT */

#ifdef DOT11Z_TDLS_SUPPORT
	case RT_OID_802_11_SET_TDLS_PARAM:
		TDLS_CntlOidTDLSRequestProc(pAd, Elem);
		break;
#endif /* DOT11Z_TDLS_SUPPORT */

	default:
		DBGPRINT(RT_DEBUG_TRACE,
			 ("CNTL - Illegal message in CntlIdleProc(MsgType=%ld)\n",
			  Elem->MsgType));
		break;
	}
}

VOID CntlOidScanProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_SCAN_REQ_STRUCT ScanReq;
	ULONG BssIdx = BSS_NOT_FOUND;
/*	BSS_ENTRY                  CurrBss; */
	BSS_ENTRY *pCurrBss = NULL;

#ifdef RALINK_ATE
/* Disable scanning when ATE is running. */
	if (ATE_ON(pAd))
		return;
#endif /* RALINK_ATE */
#ifdef P2P_SUPPORT
	if (pAd->P2pCfg.P2pCounter.bStartScan == TRUE)
		return;
#endif /* P2P_SUPPORT */

#ifdef RELEASE_EXCLUDE
	DBGPRINT(RT_DEBUG_INFO, ("CNTL - SCAN starts\n"));
#endif /* RELEASE_EXCLUDE */

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **) & pCurrBss, sizeof (BSS_ENTRY));
	if (pCurrBss == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s: Allocate memory fail!!!\n", __FUNCTION__));
		return;
	}

	/* record current BSS if network is connected. */
	/* 2003-2-13 do not include current IBSS if this is the only STA in this IBSS. */
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)) {
		BssIdx =
		    BssSsidTableSearch(&pAd->ScanTab, pAd->CommonCfg.Bssid,
				       (PUCHAR) pAd->CommonCfg.Ssid,
				       pAd->CommonCfg.SsidLen,
				       pAd->CommonCfg.Channel);
		if (BssIdx != BSS_NOT_FOUND) {
			NdisMoveMemory(pCurrBss, &pAd->ScanTab.BssEntry[BssIdx],
				       sizeof (BSS_ENTRY));
		}
	}

#if 0
	/* clean up previous SCAN result, add current BSS back to table if any */
	BssTableInit(&pAd->ScanTab);
	if (BssIdx != BSS_NOT_FOUND) {
		/* DDK Note: If the NIC is associated with a particular BSSID and SSID */
		/*    that are not contained in the list of BSSIDs generated by this scan, the */
		/*    BSSID description of the currently associated BSSID and SSID should be */
		/*    appended to the list of BSSIDs in the NIC's database. */
		/* To ensure this, we append this BSS as the first entry in SCAN result */
		NdisMoveMemory(&pAd->ScanTab.BssEntry[0], pCurrBss,
			       sizeof (BSS_ENTRY));
		pAd->ScanTab.BssNr = 1;
	}
#endif

#ifdef WIDI_SUPPORT
	if (pAd->StaCfg.bWIDI)
		ScanParmFill(pAd, &ScanReq, (PSTRING) Elem->Msg, Elem->MsgLen, BSS_ANY, SCAN_PASSIVE);
	else
#endif /* WIDI_SUPPORT */
	ScanParmFill(pAd, &ScanReq, (PSTRING) Elem->Msg, Elem->MsgLen, BSS_ANY,
		     SCAN_ACTIVE);
	MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
		    sizeof (MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
	pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;

	if (pCurrBss != NULL)
		os_free_mem(NULL, pCurrBss);
}

/*
	==========================================================================
	Description:
		Before calling this routine, user desired SSID should already been
		recorded in CommonCfg.Ssid[]
	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlOidSsidProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PNDIS_802_11_SSID pOidSsid = (NDIS_802_11_SSID *) Elem->Msg;
	MLME_DISASSOC_REQ_STRUCT DisassocReq;


#ifdef DOT11R_FT_SUPPORT
	pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain = FALSE;
	pAd->StaCfg.Dot11RCommInfo.FtRspSuccess = 0;
#endif /* DOT11R_FT_SUPPORT */

	/* Step 1. record the desired user settings to MlmeAux */
	NdisZeroMemory(pAd->MlmeAux.Ssid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->MlmeAux.Ssid, pOidSsid->Ssid, pOidSsid->SsidLength);
	pAd->MlmeAux.SsidLen = (UCHAR) pOidSsid->SsidLength;
	if (pAd->StaCfg.BssType == BSS_INFRA)
		NdisZeroMemory(pAd->MlmeAux.Bssid, MAC_ADDR_LEN);
	pAd->MlmeAux.BssType = pAd->StaCfg.BssType;

	pAd->StaCfg.bAutoConnectByBssid = FALSE;

	/*save connect info*/
	NdisZeroMemory(pAd->StaCfg.ConnectinfoSsid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->StaCfg.ConnectinfoSsid, pOidSsid->Ssid, pOidSsid->SsidLength);
	pAd->StaCfg.ConnectinfoSsidLen = pOidSsid->SsidLength;
	pAd->StaCfg.ConnectinfoBssType = pAd->StaCfg.BssType;
	
#ifdef WSC_STA_SUPPORT
	/* for M8 */
	NdisZeroMemory(pAd->CommonCfg.Ssid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->CommonCfg.Ssid, pOidSsid->Ssid, pOidSsid->SsidLength);
	pAd->CommonCfg.SsidLen = (UCHAR) pOidSsid->SsidLength;
#endif /* WSC_STA_SUPPORT */


	/* Update Reconnect Ssid, that user desired to connect. */
	NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.Ssid,
		       pAd->MlmeAux.SsidLen);
	pAd->MlmeAux.AutoReconnectSsidLen = pAd->MlmeAux.SsidLen;

	/*
		step 2. 
		find all matching BSS in the lastest SCAN result (inBssTab)
		and log them into MlmeAux.SsidBssTab for later-on iteration. Sort by RSSI order
	*/
	BssTableSsidSort(pAd, &pAd->MlmeAux.SsidBssTab,
			 (PCHAR) pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);

	DBGPRINT(RT_DEBUG_TRACE,
		 ("CntlOidSsidProc():CNTL - %d BSS of %d BSS match the desire ",
		  pAd->MlmeAux.SsidBssTab.BssNr, pAd->ScanTab.BssNr));
	if (pAd->MlmeAux.SsidLen == MAX_LEN_OF_SSID)
		hex_dump("\nSSID", pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
	else
		DBGPRINT(RT_DEBUG_TRACE,
			 ("(%d)SSID - %s\n", pAd->MlmeAux.SsidLen,
			  pAd->MlmeAux.Ssid));

	if (INFRA_ON(pAd) &&
	    OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) &&
	    (pAd->CommonCfg.SsidLen == pAd->MlmeAux.SsidBssTab.BssEntry[0].SsidLen)
	    && NdisEqualMemory(pAd->CommonCfg.Ssid, pAd->MlmeAux.SsidBssTab.BssEntry[0].Ssid, pAd->CommonCfg.SsidLen)
	    && MAC_ADDR_EQUAL(pAd->CommonCfg.Bssid, pAd->MlmeAux.SsidBssTab.BssEntry[0].Bssid)) {
		/* Case 1. already connected with an AP who has the desired SSID */
		/*         with highest RSSI */

		/* Add checking Mode "LEAP" for CCX 1.0 */
		if (((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
		     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
		     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
		     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#ifdef WAPI_SUPPORT
		     || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAIPSK)
		     || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAICERT)
#endif /* WAPI_SUPPORT */
		    ) &&
		    (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)) {
			/*
				case 1.1 For WPA, WPA-PSK, 
				if port is not secured, we have to redo connection process
			*/
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CntlOidSsidProc():CNTL - disassociate with current AP...\n"));
			DisassocParmFill(pAd, &DisassocReq,
					 pAd->CommonCfg.Bssid,
					 REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
				    MT2_MLME_DISASSOC_REQ,
				    sizeof (MLME_DISASSOC_REQ_STRUCT),
				    &DisassocReq, 0);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		} else if (pAd->bConfigChanged == TRUE) {
			/* case 1.2 Important Config has changed, we have to reconnect to the same AP */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CntlOidSsidProc():CNTL - disassociate with current AP Because config changed...\n"));
			DisassocParmFill(pAd, &DisassocReq,
					 pAd->CommonCfg.Bssid,
					 REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
				    MT2_MLME_DISASSOC_REQ,
				    sizeof (MLME_DISASSOC_REQ_STRUCT),
				    &DisassocReq, 0);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		} else {
			/* case 1.3. already connected to the SSID with highest RSSI. */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CntlOidSsidProc():CNTL - already with this BSSID. ignore this SET_SSID request\n"));
			/*
				(HCT 12.1) 1c_wlan_mediaevents required
				media connect events are indicated when associating with the same AP
			*/
			if (INFRA_ON(pAd)) {
				/*
					Since MediaState already is NdisMediaStateConnected
					We just indicate the connect event again to meet the WHQL required.
				*/
				RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);
				pAd->ExtraInfo = GENERAL_LINK_UP;	/* Update extra information to link is up */
			}

			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
			RtmpOSWrielessEventSend(pAd->net_dev,
						RT_WLAN_EVENT_CGIWAP, -1,
						&pAd->MlmeAux.Bssid[0], NULL,
						0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
		}
	} else if (INFRA_ON(pAd)) {
		/*
			For RT61
			[88888] OID_802_11_SSID should have returned NDTEST_WEP_AP2(Returned: )
			RT61 may lost SSID, and not connect to NDTEST_WEP_AP2 and will connect to NDTEST_WEP_AP2 by Autoreconnect
			But media status is connected, so the SSID not report correctly.
		*/
		if (!SSID_EQUAL(pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen)) {
			/* Different SSID means not Roaming case, so we let LinkDown() to Indicate a disconnect event. */
			pAd->MlmeAux.CurrReqIsFromNdis = TRUE;
		}

		/*
			case 2. active INFRA association existent
			Roaming is done within miniport driver, nothing to do with configuration
			utility. so upon a new SET(OID_802_11_SSID) is received, we just
			disassociate with the current associated AP,
			then perform a new association with this new SSID, no matter the
			new/old SSID are the same or not.
		*/
		DBGPRINT(RT_DEBUG_TRACE,
			 ("CntlOidSsidProc():CNTL - disassociate with current AP...\n"));
		DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid,
				 REASON_DISASSOC_STA_LEAVING);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
			    sizeof (MLME_DISASSOC_REQ_STRUCT), &DisassocReq, 0);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;

#ifdef CONFIG_MULTI_CHANNEL
	if (P2P_CLI_ON(pAd) && (pAd->Multi_Channel_Enable == TRUE))
	{	
		DBGPRINT(RT_DEBUG_TRACE,
				 ("disassociate with current AP and P2P_CLI_ON set pAd->StaCfg.bAutoReconnect = TRUE\n"));
		pAd->StaCfg.bAutoReconnect = TRUE;
	}
#endif /*CONFIG_MULTI_CHANNEL*/

	} else {
		if (ADHOC_ON(pAd)) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CntlOidSsidProc():CNTL - drop current ADHOC\n"));
			LinkDown(pAd, FALSE);
			OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
			RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);
			pAd->ExtraInfo = GENERAL_LINK_DOWN;
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CntlOidSsidProc():NDIS_STATUS_MEDIA_DISCONNECT Event C!\n"));
		}

		if ((pAd->MlmeAux.SsidBssTab.BssNr == 0) &&
		    (pAd->StaCfg.bAutoReconnect == TRUE) &&
		    (((pAd->MlmeAux.BssType == BSS_INFRA) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
		     || ((pAd->MlmeAux.BssType == BSS_ADHOC) && !pAd->StaCfg.bNotFirstScan))
		    && (MlmeValidateSSID(pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen) == TRUE)
		    ) {
			MLME_SCAN_REQ_STRUCT ScanReq;
			
#ifdef WAC_SUPPORT
			if (pAd->StaCfg.EasyConfigInfo.bEnableWAC
				&& (pAd->StaCfg.EasyConfigInfo.DoAutoWAC == 0x00))
			{
				if (WAC_FindWACAP(pAd))
					return;
			}
#endif /* WAC_SUPPORT */
			if (pAd->MlmeAux.BssType == BSS_ADHOC)
				pAd->StaCfg.bNotFirstScan = TRUE;
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - No matching BSS, start a new scan\n"));
#if 0
			BssTableInit(&pAd->ScanTab);
#endif

			if((pAd->StaCfg.ConnectinfoChannel != 0) && (pAd->StaCfg.Connectinfoflag == TRUE))
			{
				ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid,
					pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_ACTIVE);
				MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_FORCE_SCAN_REQ,
					sizeof (MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
				pAd->Mlme.CntlMachine.CurrState =	CNTL_WAIT_SCAN_FOR_CONNECT;
			}
			else	
			{
#ifdef WIDI_SUPPORT
				if (pAd->StaCfg.bWIDI)
				{
					if (pAd->MlmeAux.OldChannel > 0)
						pAd->MlmeAux.Channel = pAd->MlmeAux.OldChannel;
					ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid,
						 		pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_PASSIVE);
				}
				else			
#endif /* WIDI_SUPPORT */
				ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid,
					     pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_ACTIVE);
				MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
					    sizeof (MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
			}
			
			/* Reset Missed scan number */
			NdisGetSystemUpTime(&pAd->StaCfg.LastScanTime);
		} else {
#ifdef WSC_STA_SUPPORT
#ifdef WSC_LED_SUPPORT
			/* LED indication. */
			if (pAd->MlmeAux.BssType == BSS_INFRA) {
				LEDConnectionStart(pAd);
			}
#endif /* WSC_LED_SUPPORT */
#endif /* WSC_STA_SUPPORT */

			if ((pAd->CommonCfg.CountryRegion & 0x7f) == REGION_33_BG_BAND)
			{
				BSS_ENTRY *entry;
				// TODO: shiang-6590, check this "SavedPhyMode"!
				entry = &pAd->MlmeAux.SsidBssTab.BssEntry[pAd->MlmeAux.BssIdx];
				if ((entry->Channel == 14) && (pAd->CommonCfg.SavedPhyMode = 0xff)) {
					pAd->CommonCfg.SavedPhyMode = pAd->CommonCfg.PhyMode;
					RTMPSetPhyMode(pAd, WMODE_B);
				} else if (pAd->CommonCfg.SavedPhyMode != 0xff) {
					RTMPSetPhyMode(pAd, pAd->CommonCfg.SavedPhyMode);
					pAd->CommonCfg.SavedPhyMode = 0xFF;
				} else {
					DBGPRINT(RT_DEBUG_ERROR, ("%s():SavedPhyMode=0x%x! Channel=%d\n",
								__FUNCTION__, pAd->CommonCfg.SavedPhyMode, entry->Channel));
				}
			}
			pAd->MlmeAux.BssIdx = 0;
			IterateOnBssTab(pAd);
		}
	}
}

#ifdef WSC_STA_SUPPORT
/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWscIterate(
	IN PRTMP_ADAPTER pAd)
{
#ifdef WSC_LED_SUPPORT
	UCHAR WPSLEDStatus;
#endif /* WSC_LED_SUPPORT */

	/* Connect to the WPS-enabled AP by its BSSID directly. */
	/* Note: When WscState is equal to WSC_STATE_START, */
	/*       pAd->StaCfg.WscControl.WscAPBssid has been filled with valid BSSID. */
	if ((pAd->StaCfg.WscControl.WscState >= WSC_STATE_START) &&
	    (pAd->StaCfg.WscControl.WscStatus != STATUS_WSC_SCAN_AP)) {
		/* Set WSC state to WSC_STATE_START */
		pAd->StaCfg.WscControl.WscState = WSC_STATE_START;
		pAd->StaCfg.WscControl.WscStatus = STATUS_WSC_START_ASSOC;

		if (pAd->StaCfg.BssType == BSS_INFRA) {
			MlmeEnqueue(pAd,
				    MLME_CNTL_STATE_MACHINE,
				    OID_802_11_BSSID,
				    MAC_ADDR_LEN,
				    pAd->StaCfg.WscControl.WscBssid, 0);
		} else {
			MlmeEnqueue(pAd,
				    MLME_CNTL_STATE_MACHINE,
				    OID_802_11_SSID,
				    sizeof (NDIS_802_11_SSID),
				    (VOID *) & pAd->StaCfg.WscControl.WscSsid,
				    0);
		}

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

		RTMP_MLME_HANDLER(pAd);
	}
#ifdef WSC_LED_SUPPORT
	/* The protocol is connecting to a partner. */
	WPSLEDStatus = LED_WPS_IN_PROCESS;
	RTMPSetLED(pAd, WPSLEDStatus);
#endif /* WSC_LED_SUPPORT */
}
#endif /* WSC_STA_SUPPORT */

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlOidRTBssidProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	ULONG BssIdx;
	PUCHAR pOidBssid = (PUCHAR) Elem->Msg;
	MLME_DISASSOC_REQ_STRUCT DisassocReq;
	MLME_JOIN_REQ_STRUCT JoinReq;
	PBSS_ENTRY pInBss = NULL;


#ifdef WSC_STA_SUPPORT
	PWSC_CTRL pWpsCtrl = &pAd->StaCfg.WscControl;
#endif /* WSC_STA_SUPPORT */

#ifdef RALINK_ATE
/* No need to perform this routine when ATE is running. */
	if (ATE_ON(pAd))
		return;
#endif /* RALINK_ATE */

	/* record user desired settings */
	COPY_MAC_ADDR(pAd->MlmeAux.Bssid, pOidBssid);
	pAd->MlmeAux.BssType = pAd->StaCfg.BssType;

	/*save connect info*/
	NdisZeroMemory(pAd->StaCfg.ConnectinfoBssid, MAC_ADDR_LEN);
	NdisMoveMemory(pAd->StaCfg.ConnectinfoBssid, pOidBssid, MAC_ADDR_LEN);
	DBGPRINT(RT_DEBUG_TRACE, ("ANDROID IOCTL::SIOCSIWAP %02x:%02x:%02x:%02x:%02x:%02x\n",
		pAd->StaCfg.ConnectinfoBssid[0], pAd->StaCfg.ConnectinfoBssid[1], pAd->StaCfg.ConnectinfoBssid[2],
		pAd->StaCfg.ConnectinfoBssid[3], pAd->StaCfg.ConnectinfoBssid[4], pAd->StaCfg.ConnectinfoBssid[5]));

	/* find the desired BSS in the latest SCAN result table */
	BssIdx = BssTableSearch(&pAd->ScanTab, pOidBssid, pAd->MlmeAux.Channel);

#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.WpaSupplicantUP & WPA_SUPPLICANT_ENABLE_WPS) ;
	else
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.WpaSupplicantUP & WPA_SUPPLICANT_ENABLE) ;
	else
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */
	if (
#ifdef WSC_STA_SUPPORT
		(pWpsCtrl->WscConfMode == WSC_DISABLE) && 
#endif /* WSC_STA_SUPPORT */
		(BssIdx != BSS_NOT_FOUND))
	{
		pInBss = &pAd->ScanTab.BssEntry[BssIdx];

		/*
			If AP's SSID has been changed, STA cannot connect to this AP.
		*/
		if (SSID_EQUAL(pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, pInBss->Ssid, pInBss->SsidLen) == FALSE)
			BssIdx = BSS_NOT_FOUND;

		if (pAd->StaCfg.AuthMode <= Ndis802_11AuthModeAutoSwitch) {
			if (pAd->StaCfg.WepStatus != pInBss->WepStatus)
				BssIdx = BSS_NOT_FOUND;
		} else {
			/* Check AuthMode and AuthModeAux for matching, in case AP support dual-mode */
			if ((pAd->StaCfg.AuthMode != pInBss->AuthMode) &&
			    (pAd->StaCfg.AuthMode != pInBss->AuthModeAux))
				BssIdx = BSS_NOT_FOUND;
		}
	}

#ifdef WSC_STA_SUPPORT
	if ((pWpsCtrl->WscConfMode != WSC_DISABLE) &&
		((pWpsCtrl->WscStatus == STATUS_WSC_START_ASSOC) || (pWpsCtrl->WscStatus == STATUS_WSC_LINK_UP)) &&
		(pAd->StaCfg.bSkipAutoScanConn == TRUE))
		pAd->StaCfg.bSkipAutoScanConn = FALSE;
#endif /* WSC_STA_SUPPORT */

	if (BssIdx == BSS_NOT_FOUND) {
		if (((pAd->StaCfg.BssType == BSS_INFRA) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))) ||
		    (pAd->StaCfg.bNotFirstScan == FALSE)) {
			MLME_SCAN_REQ_STRUCT ScanReq;

			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - BSSID not found. reply NDIS_STATUS_NOT_ACCEPTED\n"));
			if (pAd->StaCfg.BssType == BSS_ADHOC)
				pAd->StaCfg.bNotFirstScan = TRUE;

			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - BSSID not found. start a new scan\n"));
#if 0
			BssTableInit(&pAd->ScanTab);
#endif

			if((pAd->StaCfg.ConnectinfoChannel  != 0)&& (pAd->StaCfg.Connectinfoflag == TRUE))
			{
				DBGPRINT(RT_DEBUG_OFF, ("CntlOidRTBssidProc BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
					PRINT_MAC(pAd->StaCfg.ConnectinfoBssid)));
				pAd->CommonCfg.Channel = pAd->StaCfg.ConnectinfoChannel;
				MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_FORCE_JOIN_REQ,
					sizeof (MLME_JOIN_REQ_STRUCT), &JoinReq, 0);
				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
			}
			else	{
				ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid,
				     pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_ACTIVE);
				MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
					    sizeof (MLME_SCAN_REQ_STRUCT), &ScanReq, 0);
				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
			}
			/* Reset Missed scan number */
			NdisGetSystemUpTime(&pAd->StaCfg.LastScanTime);
		}
		else
		{
			MLME_START_REQ_STRUCT StartReq;
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - BSSID not found. start a new ADHOC (Ssid=%s)...\n",
				  pAd->MlmeAux.Ssid));
			StartParmFill(pAd, &StartReq, (PCHAR)pAd->MlmeAux.Ssid,
				      pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ,
				    sizeof (MLME_START_REQ_STRUCT), &StartReq, 0);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}
		return;
	}

	pInBss = &pAd->ScanTab.BssEntry[BssIdx];
	/* Update Reconnect Ssid, that user desired to connect. */
	NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, MAX_LEN_OF_SSID);
	pAd->MlmeAux.AutoReconnectSsidLen = pInBss->SsidLen;
	NdisMoveMemory(pAd->MlmeAux.AutoReconnectSsid, pInBss->Ssid, pInBss->SsidLen);

#ifdef WAC_SUPPORT
	if (pAd->StaCfg.EasyConfigInfo.bEnableWAC
		&& (pInBss->WpsAP & 0x04)
		&& (pAd->StaCfg.EasyConfigInfo.DoAutoWAC == 0x01))
	{
		pAd->StaCfg.EasyConfigInfo.bDoAutoWps = TRUE;
		pAd->StaCfg.EasyConfigInfo.bWACAP = TRUE;
		pAd->StaCfg.EasyConfigInfo.CurrentStatus = OFC_PROCEED_SECOND_PHASE;
	}
#endif /* WAC_SUPPORT */

	/* copy the matched BSS entry from ScanTab to MlmeAux.SsidBssTab. Why? */
	/* Because we need this entry to become the JOIN target in later on SYNC state machine */
	pAd->MlmeAux.BssIdx = 0;
	pAd->MlmeAux.SsidBssTab.BssNr = 1;
	NdisMoveMemory(&pAd->MlmeAux.SsidBssTab.BssEntry[0], pInBss,
		       sizeof (BSS_ENTRY));

	{
		if (INFRA_ON(pAd)) {
			if(!MAC_ADDR_EQUAL(pAd->StaCfg.ConnectinfoBssid,pAd->CommonCfg.Bssid)) //auto disconnect reconnect for many times after sending link command 
			{
				/* disassoc from current AP first */
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - disassociate with current AP ...\n"));
				DisassocParmFill(pAd, &DisassocReq,
						 pAd->CommonCfg.Bssid,
						 REASON_DISASSOC_STA_LEAVING);
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
					    MT2_MLME_DISASSOC_REQ,
					    sizeof (MLME_DISASSOC_REQ_STRUCT),
					    &DisassocReq, 0);
	
				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
			}
		} else {
			if (ADHOC_ON(pAd)) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - drop current ADHOC\n"));
				LinkDown(pAd, FALSE);
				OPSTATUS_CLEAR_FLAG(pAd,
						    fOP_STATUS_MEDIA_STATE_CONNECTED);
				RTMP_IndicateMediaState(pAd,
							NdisMediaStateDisconnected);
				pAd->ExtraInfo = GENERAL_LINK_DOWN;
				DBGPRINT(RT_DEBUG_TRACE,
					 ("NDIS_STATUS_MEDIA_DISCONNECT Event C!\n"));
			}

			pInBss = &pAd->MlmeAux.SsidBssTab.BssEntry[0];

			pAd->StaCfg.PairCipher = pAd->StaCfg.WepStatus;
#ifdef WPA_SUPPLICANT_SUPPORT
			if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif /* WPA_SUPPLICANT_SUPPORT */
			pAd->StaCfg.GroupCipher = pAd->StaCfg.WepStatus;

			/* Check cipher suite, AP must have more secured cipher than station setting */
			/* Set the Pairwise and Group cipher to match the intended AP setting */
			/* We can only connect to AP with less secured cipher setting */
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA)
			    || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)) {
				pAd->StaCfg.GroupCipher = pInBss->WPA.GroupCipher;

				if (pAd->StaCfg.WepStatus == pInBss->WPA.PairCipher)
					pAd->StaCfg.PairCipher = pInBss->WPA.PairCipher;
				else if (pInBss->WPA.PairCipherAux != Ndis802_11WEPDisabled)
					pAd->StaCfg.PairCipher = pInBss->WPA.PairCipherAux;
				else	/* There is no PairCipher Aux, downgrade our capability to TKIP */
					pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
			} else
			    if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2)
				|| (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)) {
				pAd->StaCfg.GroupCipher = pInBss->WPA2.GroupCipher;

				if (pAd->StaCfg.WepStatus == pInBss->WPA2.PairCipher)
					pAd->StaCfg.PairCipher = pInBss->WPA2.PairCipher;
				else if (pInBss->WPA2.PairCipherAux != Ndis802_11WEPDisabled)
					pAd->StaCfg.PairCipher = pInBss->WPA2.PairCipherAux;
				else	/* There is no PairCipher Aux, downgrade our capability to TKIP */
					pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;

				/* RSN capability */
				pAd->StaCfg.RsnCapability = pInBss->WPA2.RsnCapability;
			}
#ifdef WAPI_SUPPORT
			else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAICERT)
				 || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAIPSK)) {
				pAd->StaCfg.GroupCipher = pInBss->WAPI.GroupCipher;
				pAd->StaCfg.PairCipher = pInBss->WAPI.PairCipher;
			}
#endif /* WAPI_SUPPORT */

			/* Set Mix cipher flag */
			pAd->StaCfg.bMixCipher = (pAd->StaCfg.PairCipher == pAd->StaCfg.GroupCipher) ? FALSE : TRUE;

			/* No active association, join the BSS immediately */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - joining %02x:%02x:%02x:%02x:%02x:%02x ...\n",
				  PRINT_MAC(pOidBssid)));

			JoinParmFill(pAd, &JoinReq, pAd->MlmeAux.BssIdx);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_JOIN_REQ,
				    sizeof (MLME_JOIN_REQ_STRUCT), &JoinReq, 0);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
		}
	}
}

/* Roaming is the only external request triggering CNTL state machine */
/* despite of other "SET OID" operation. All "SET OID" related oerations */
/* happen in sequence, because no other SET OID will be sent to this device */
/* until the the previous SET operation is complete (successful o failed). */
/* So, how do we quarantee this ROAMING request won't corrupt other "SET OID"? */
/* or been corrupted by other "SET OID"? */
/* */
/* IRQL = DISPATCH_LEVEL */
VOID CntlMlmeRoamingProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
#ifdef DOT11R_FT_SUPPORT
	MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[BSSID_WCID];
	BSS_ENTRY *pTargetAP = &pAd->MlmeAux.RoamTab.BssEntry[0];
#endif /* DOT11R_FT_SUPPORT */

	DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Roaming in MlmeAux.RoamTab...\n"));

#ifdef DOT11R_FT_SUPPORT
	if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
	    pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain &&
	    (pAd->CommonCfg.Channel != pTargetAP->Channel)) {
		if (pEntry->MdIeInfo.FtCapPlc.field.FtOverDs) {
			MLME_FT_REQ_STRUCT FtReq;
			DBGPRINT(RT_DEBUG_TRACE,
				 ("Dot11r: pBss->Channel = %d. Use OTD.\n",
				  pTargetAP->Channel));
			FT_OTD_ActParmFill(pAd, &FtReq, pTargetAP->Bssid,
					   pTargetAP->AuthMode,
					   &pAd->StaCfg.Dot11RCommInfo.MdIeInfo,
					   &pAd->StaCfg.Dot11RCommInfo.FtIeInfo,
					   pAd->StaCfg.RSNIE_Len,
					   pAd->StaCfg.RSN_IE);

			MlmeEnqueue(pAd, FT_OTD_ACT_STATE_MACHINE,
				    FT_OTD_MT2_MLME_REQ,
				    sizeof (MLME_FT_REQ_STRUCT), &FtReq, 0);
		} else {
			DBGPRINT(RT_DEBUG_ERROR, ("CNTL - Shall not haapen!!\n"));
		}
	}
	else
#endif /* DOT11R_FT_SUPPORT */
	{
		/*Let BBP register at 20MHz to do (fast) roaming. */
		rtmp_bbp_set_bw(pAd, BW_20);

		NdisMoveMemory(&pAd->MlmeAux.SsidBssTab, &pAd->MlmeAux.RoamTab,
			       sizeof (pAd->MlmeAux.RoamTab));
		pAd->MlmeAux.SsidBssTab.BssNr = pAd->MlmeAux.RoamTab.BssNr;

		BssTableSortByRssi(&pAd->MlmeAux.SsidBssTab);
		pAd->MlmeAux.BssIdx = 0;
		IterateOnBssTab(pAd);
	}
}

#ifdef QOS_DLS_SUPPORT
/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlOidDLSSetupProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PRT_802_11_DLS pDLS = (PRT_802_11_DLS) Elem->Msg;
	MLME_DLS_REQ_STRUCT MlmeDlsReq;
	INT i;
	USHORT reason = REASON_UNSPECIFY;

	DBGPRINT(RT_DEBUG_TRACE,
		 ("CNTL - (OID set %02x:%02x:%02x:%02x:%02x:%02x with Valid=%d, Status=%d, TimeOut=%d, CountDownTimer=%d)\n",
		  pDLS->MacAddr[0], pDLS->MacAddr[1], pDLS->MacAddr[2],
		  pDLS->MacAddr[3], pDLS->MacAddr[4], pDLS->MacAddr[5],
		  pDLS->Valid, pDLS->Status, pDLS->TimeOut,
		  pDLS->CountDownTimer));

	if (!pAd->CommonCfg.bDLSCapable)
		return;

	/* DLS will not be supported when Adhoc mode */
	if (INFRA_ON(pAd)) {
		for (i = 0; i < MAX_NUM_OF_DLS_ENTRY; i++) {
			if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid
			    && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH)
			    && (pDLS->TimeOut ==
				pAd->StaCfg.DLSEntry[i].TimeOut)
			    && MAC_ADDR_EQUAL(pDLS->MacAddr,
					      pAd->StaCfg.DLSEntry[i].
					      MacAddr)) {
				/* 1. Same setting, just drop it */
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - setting unchanged\n"));
				break;
			} else if (!pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid
				   && (pAd->StaCfg.DLSEntry[i].Status ==
				       DLS_FINISH)
				   && MAC_ADDR_EQUAL(pDLS->MacAddr,
						     pAd->StaCfg.DLSEntry[i].
						     MacAddr)) {
				/* 2. Disable DLS link case, just tear down DLS link */
				reason = REASON_QOS_UNWANTED_MECHANISM;
				pAd->StaCfg.DLSEntry[i].Valid = FALSE;
				pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_TEAR_DOWN,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - start tear down procedure\n"));
				break;
			} else if ((i < MAX_NUM_OF_DLS_ENTRY) && pDLS->Valid
				   && !pAd->StaCfg.DLSEntry[i].Valid) {
				/* 3. Enable case, start DLS setup procedure */
				NdisMoveMemory(&pAd->StaCfg.DLSEntry[i], pDLS,
					       sizeof (RT_802_11_DLS_UI));

				/*Update countdown timer */
				pAd->StaCfg.DLSEntry[i].CountDownTimer =
				    pAd->StaCfg.DLSEntry[i].TimeOut;
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_REQ,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - DLS setup case\n"));
				break;
			} else if ((i < MAX_NUM_OF_DLS_ENTRY) && pDLS->Valid
				   && pAd->StaCfg.DLSEntry[i].Valid
				   && (pAd->StaCfg.DLSEntry[i].Status ==
				       DLS_FINISH)
				   && !MAC_ADDR_EQUAL(pDLS->MacAddr,
						      pAd->StaCfg.DLSEntry[i].
						      MacAddr)) {
				/* 4. update mac case, tear down old DLS and setup new DLS */
				reason = REASON_QOS_UNWANTED_MECHANISM;
				pAd->StaCfg.DLSEntry[i].Valid = FALSE;
				pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_TEAR_DOWN,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				NdisMoveMemory(&pAd->StaCfg.DLSEntry[i], pDLS,
					       sizeof (RT_802_11_DLS_UI));
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_REQ,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - DLS tear down and restart case\n"));
				break;
			} else if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid
				   && MAC_ADDR_EQUAL(pDLS->MacAddr,
						     pAd->StaCfg.DLSEntry[i].
						     MacAddr)
				   && (pAd->StaCfg.DLSEntry[i].TimeOut !=
				       pDLS->TimeOut)) {
				/* 5. update timeout case, start DLS setup procedure (no tear down) */
				pAd->StaCfg.DLSEntry[i].TimeOut = pDLS->TimeOut;
				/*Update countdown timer */
				pAd->StaCfg.DLSEntry[i].CountDownTimer =
				    pAd->StaCfg.DLSEntry[i].TimeOut;
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_REQ,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - DLS update timeout case\n"));
				break;
			} else if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid
				   && (pAd->StaCfg.DLSEntry[i].Status !=
				       DLS_FINISH)
				   && MAC_ADDR_EQUAL(pDLS->MacAddr,
						     pAd->StaCfg.DLSEntry[i].
						     MacAddr)) {
				/* 6. re-setup case, start DLS setup procedure (no tear down) */
				DlsParmFill(pAd, &MlmeDlsReq,
					    &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE,
					    MT2_MLME_DLS_REQ,
					    sizeof (MLME_DLS_REQ_STRUCT),
					    &MlmeDlsReq, 0);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - DLS retry setup procedure\n"));
				break;
			} else {
				DBGPRINT(RT_DEBUG_WARN,
					 ("CNTL - DLS not changed in entry - %d - Valid=%d, Status=%d, TimeOut=%d\n",
					  i, pAd->StaCfg.DLSEntry[i].Valid,
					  pAd->StaCfg.DLSEntry[i].Status,
					  pAd->StaCfg.DLSEntry[i].TimeOut));
			}
		}
	}
}
#endif /* QOS_DLS_SUPPORT */

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitDisassocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_START_REQ_STRUCT StartReq;

	if (Elem->MsgType == MT2_DISASSOC_CONF) {
		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Dis-associate successful\n"));

		RTMPSendWirelessEvent(pAd, IW_DISASSOC_EVENT_FLAG, NULL, BSS0,
				      0);

		LinkDown(pAd, FALSE);

		/* case 1. no matching BSS, and user wants ADHOC, so we just start a new one */
		if ((pAd->MlmeAux.SsidBssTab.BssNr == 0)
		    && (pAd->StaCfg.BssType == BSS_ADHOC)) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - No matching BSS, start a new ADHOC (Ssid=%s)...\n",
				  pAd->MlmeAux.Ssid));
			StartParmFill(pAd, &StartReq, (PCHAR) pAd->MlmeAux.Ssid,
				      pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ,
				    sizeof (MLME_START_REQ_STRUCT), &StartReq,
				    0);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}
		/* case 2. try each matched BSS */
		else {
			/*
			   Some customer would set AP1 & AP2 same SSID, AuthMode & EncrypType but different WPAPSK,
			   therefore we need to try next AP here.
			 */
			/*pAd->MlmeAux.BssIdx = 0;*/
			pAd->MlmeAux.BssIdx++;

#ifdef WSC_STA_SUPPORT
			if (pAd->StaCfg.WscControl.WscState >= WSC_STATE_START)
				CntlWscIterate(pAd);
			else if (((pAd->StaCfg.WscControl.bWscTrigger == FALSE)
#ifdef STA_EASY_CONFIG_SETUP
				  || (pAd->StaCfg.EasyConfigInfo.bDoAutoWps ==
				      FALSE)
#endif /* STA_EASY_CONFIG_SETUP */
				 )
				 && (pAd->StaCfg.WscControl.WscState !=
				     WSC_STATE_INIT))
#endif /* WSC_STA_SUPPORT */
				IterateOnBssTab(pAd);
#ifdef WSC_STA_SUPPORT
			else
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
#endif /* WSC_STA_SUPPORT */
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitJoinProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Reason;
	MLME_AUTH_REQ_STRUCT AuthReq;

	if (Elem->MsgType == MT2_JOIN_CONF) {
		NdisMoveMemory(&Reason, Elem->Msg, sizeof (USHORT));
		if (Reason == MLME_SUCCESS) {
			/* 1. joined an IBSS, we are pretty much done here */
			if (pAd->MlmeAux.BssType == BSS_ADHOC) {
				/* */
				/* 5G bands rules of Japan: */
				/* Ad hoc must be disabled in W53(ch52,56,60,64) channels. */
				/* */

				if ((pAd->CommonCfg.bIEEE80211H == 1) &&
				    RadarChannelCheck(pAd, pAd->CommonCfg.Channel))
				{
					pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
					DBGPRINT(RT_DEBUG_TRACE,
						 ("CNTL - Channel=%d, Join adhoc on W53(52,56,60,64) Channels are not accepted\n",
						  pAd->CommonCfg.Channel));
					return;
				}

				LinkUp(pAd, BSS_ADHOC);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - join the IBSS = %02x:%02x:%02x:%02x:%02x:%02x ...\n",
					  PRINT_MAC(pAd->CommonCfg.Bssid)));

				RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);
				pAd->ExtraInfo = GENERAL_LINK_UP;

				RTMPSendWirelessEvent(pAd, IW_JOIN_IBSS_FLAG, NULL, BSS0, 0);
			}
			/* 2. joined a new INFRA network, start from authentication */
			else
			{
#ifdef DOT11R_FT_SUPPORT
				if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
				    pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain) {
					MLME_FT_OTA_AUTH_REQ_STRUCT FtOtaAuthReq;

					FT_OTA_AuthParmFill(pAd,
							    &FtOtaAuthReq,
							    pAd->MlmeAux.Bssid,
							    AUTH_MODE_FT,
							    &pAd->StaCfg.Dot11RCommInfo);

					MlmeEnqueue(pAd,
						    FT_OTA_AUTH_STATE_MACHINE,
						    FT_OTA_MT2_MLME_AUTH_REQ,
						    sizeof(MLME_FT_OTA_AUTH_REQ_STRUCT),
						    &FtOtaAuthReq, 0);
				} else
#endif /* DOT11R_FT_SUPPORT */
				{
#ifdef WEPAUTO_OPEN_FIRST
					/* Only Ndis802_11AuthModeShared try shared key first, Ndis802_11AuthModeAutoSwitch use open first */
					if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared)
#else
					/* either Ndis802_11AuthModeShared or Ndis802_11AuthModeAutoSwitch, try shared key first */
					if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared)
					    || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch))
#endif
					{
						AuthParmFill(pAd, &AuthReq,
							     pAd->MlmeAux.Bssid,
							     AUTH_MODE_KEY);
					} else {
						AuthParmFill(pAd, &AuthReq,
							     pAd->MlmeAux.Bssid,
							     AUTH_MODE_OPEN);
					}
					MlmeEnqueue(pAd, AUTH_STATE_MACHINE,
						    MT2_MLME_AUTH_REQ,
						    sizeof(MLME_AUTH_REQ_STRUCT),
						    &AuthReq, 0);
				}

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH;
			}
		}
		else
		{
#ifdef STA_EASY_CONFIG_SETUP
			if (pAd->StaCfg.EasyConfigInfo.bDoAutoWps) {
				pAd->Mlme.AutoProvisionMachine.CurrState = INFRA_AUTO_PROVISION;
				MlmeEnqueue(pAd, AUTO_PROVISION_STATE_MACHINE,
					    MT2_CONN_AP, 0, NULL, 0);
				RTMP_MLME_HANDLER(pAd);
				return;
			}
#endif /* STA_EASY_CONFIG_SETUP */
			/* 3. failed, try next BSS */
			pAd->MlmeAux.BssIdx++;
			IterateOnBssTab(pAd);
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitStartProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Result;
	RT_PHY_INFO *rt_phy_info;


	if (Elem->MsgType == MT2_START_CONF) {
		NdisMoveMemory(&Result, Elem->Msg, sizeof (USHORT));
		if (Result == MLME_SUCCESS) {
			/* */
			/* 5G bands rules of Japan: */
			/* Ad hoc must be disabled in W53(ch52,56,60,64) channels. */
			/* */

			if ((pAd->CommonCfg.bIEEE80211H == 1) &&
			    RadarChannelCheck(pAd, pAd->CommonCfg.Channel)
			    ) {
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - Channel=%d, Start adhoc on W53(52,56,60,64) Channels are not accepted\n",
					  pAd->CommonCfg.Channel));
				return;
			}

#ifdef DOT11_N_SUPPORT
			rt_phy_info = &pAd->StaActive.SupportedPhyInfo;
			NdisZeroMemory(&rt_phy_info->MCSSet[0], 16);
			if (WMODE_CAP_N(pAd->CommonCfg.PhyMode)
			    && (pAd->StaCfg.bAdhocN == TRUE)
			    && (!pAd->CommonCfg.HT_DisallowTKIP
				|| !IS_INVALID_HT_SECURITY(pAd->StaCfg.WepStatus)))
			{
				N_ChannelCheck(pAd);
				SetCommonHT(pAd);
				pAd->MlmeAux.CentralChannel = get_cent_ch_by_htinfo(pAd,
												&pAd->CommonCfg.AddHTInfo,
												&pAd->CommonCfg.HtCapability);
				NdisMoveMemory(&pAd->MlmeAux.AddHtInfo,
					       &pAd->CommonCfg.AddHTInfo,
					       sizeof (ADD_HT_INFO_IE));
				RTMPCheckHt(pAd, BSSID_WCID,
					    &pAd->CommonCfg.HtCapability,
					    &pAd->CommonCfg.AddHTInfo);
				rt_phy_info->bHtEnable = TRUE;
				NdisMoveMemory(&rt_phy_info->MCSSet[0],
					       &pAd->CommonCfg.HtCapability.MCSSet[0], 16);
				COPY_HTSETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);
#ifdef DOT11_VHT_AC
				if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode) &&
					pAd->MlmeAux.vht_cap_len) {
					RT_VHT_CAP *rt_vht_cap = &pAd->StaActive.SupVhtCap;
					
					COPY_VHT_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);
					rt_vht_cap->vht_bw = BW_80;
					rt_vht_cap->sgi_80m = pAd->MlmeAux.vht_cap.vht_cap.sgi_80M;
					rt_vht_cap->vht_txstbc = pAd->MlmeAux.vht_cap.vht_cap.tx_stbc;
					rt_vht_cap->vht_rxstbc = pAd->MlmeAux.vht_cap.vht_cap.rx_stbc;
					rt_vht_cap->vht_htc = pAd->MlmeAux.vht_cap.vht_cap.htc_vht_cap;
				}
#endif /* DOT11_VHT_AC */
			}
			else
#endif /* DOT11_N_SUPPORT */
			{
				pAd->StaActive.SupportedPhyInfo.bHtEnable = FALSE;
			}
			pAd->StaCfg.bAdhocCreator = TRUE;
			LinkUp(pAd, BSS_ADHOC);
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			/* Before send beacon, driver need do radar detection */

			if ((pAd->CommonCfg.Channel > 14)
			    && (pAd->CommonCfg.bIEEE80211H == 1)
			    && RadarChannelCheck(pAd, pAd->CommonCfg.Channel)) {
				pAd->Dot11_H.RDMode = RD_SILENCE_MODE;
				pAd->Dot11_H.RDCount = 0;
			}

			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - start a new IBSS = %02x:%02x:%02x:%02x:%02x:%02x ...\n",
				  PRINT_MAC(pAd->CommonCfg.Bssid)));

			RTMPSendWirelessEvent(pAd, IW_START_IBSS_FLAG, NULL, BSS0, 0);
		} else {
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Start IBSS fail. BUG!!!!!\n"));
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitAuthProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Reason;
	MLME_ASSOC_REQ_STRUCT AssocReq;
	MLME_AUTH_REQ_STRUCT AuthReq;

#ifdef DOT11R_FT_SUPPORT
	if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
	    Elem->MsgType == MT2_FT_OTD_CONF) {
		DBGPRINT(RT_DEBUG_TRACE, ("FT_TEMP - CntlWaitFtProc\n"));
		NdisMoveMemory(&Reason, Elem->Msg, sizeof (USHORT));
		if (Reason == MLME_SUCCESS) {
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - FT OTD AUTH OK\n"));
			AssocParmFill(pAd, &AssocReq, pAd->MlmeAux.Bssid,
				      pAd->MlmeAux.CapabilityInfo,
				      ASSOC_TIMEOUT,
				      pAd->StaCfg.DefaultListenCount);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
				    MT2_MLME_REASSOC_REQ,
				    sizeof (MLME_ASSOC_REQ_STRUCT), &AssocReq,
				    0);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_REASSOC;
		}
	} else
#endif /* DOT11R_FT_SUPPORT */
	if (Elem->MsgType == MT2_AUTH_CONF) {
		NdisMoveMemory(&Reason, Elem->Msg, sizeof (USHORT));
		if (Reason == MLME_SUCCESS) {
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH OK\n"));
			AssocParmFill(pAd, &AssocReq, pAd->MlmeAux.Bssid,
				      pAd->MlmeAux.CapabilityInfo,
				      ASSOC_TIMEOUT,
				      pAd->StaCfg.DefaultListenCount);

#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    (pAd->StaCfg.Dot11RCommInfo.FtRspSuccess != 0)) {
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
					    MT2_MLME_REASSOC_REQ,
					    sizeof (MLME_ASSOC_REQ_STRUCT),
					    &AssocReq, 0);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_REASSOC;
			} else
#endif /* DOT11R_FT_SUPPORT */
			{
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
					    MT2_MLME_ASSOC_REQ,
					    sizeof (MLME_ASSOC_REQ_STRUCT),
					    &AssocReq, 0);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_ASSOC;
			}
		} else {
			/* This fail may because of the AP already keep us in its MAC table without */
			/* ageing-out. The previous authentication attempt must have let it remove us. */
			/* so try Authentication again may help. For D-Link DWL-900AP+ compatibility. */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - AUTH FAIL, try again...\n"));
#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    (pAd->StaCfg.Dot11RCommInfo.FtRspSuccess ==
			     FT_OTA_RESPONSE)) {
				MLME_FT_OTA_AUTH_REQ_STRUCT FtOtaAuthReq;

				FT_OTA_AuthParmFill(pAd,
						    &FtOtaAuthReq,
						    pAd->MlmeAux.Bssid,
						    AUTH_MODE_FT,
						    &pAd->StaCfg.Dot11RCommInfo);

				MlmeEnqueue(pAd, FT_OTA_AUTH_STATE_MACHINE,
					    FT_OTA_MT2_MLME_AUTH_REQ,
					    sizeof(MLME_FT_OTA_AUTH_REQ_STRUCT),
					    &FtOtaAuthReq, 0);
			} else
#endif /* DOT11R_FT_SUPPORT */
			{
#ifdef WEPAUTO_OPEN_FIRST
				/* Only Ndis802_11AuthModeShared try shared key first, Ndis802_11AuthModeAutoSwitch use open first */
				if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared)
#else
				/* either Ndis802_11AuthModeShared or Ndis802_11AuthModeAutoSwitch, try shared key first */
				if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared)
				    || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch))
#endif				
				{
					AuthParmFill(pAd, &AuthReq,
						     pAd->MlmeAux.Bssid,
						     AUTH_MODE_KEY);
				} else {
					AuthParmFill(pAd, &AuthReq,
						     pAd->MlmeAux.Bssid,
						     AUTH_MODE_OPEN);
				}
				MlmeEnqueue(pAd, AUTH_STATE_MACHINE,
					    MT2_MLME_AUTH_REQ,
					    sizeof (MLME_AUTH_REQ_STRUCT),
					    &AuthReq, 0);

			}
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH2;
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitAuthProc2(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Reason;
	MLME_ASSOC_REQ_STRUCT AssocReq;
	MLME_AUTH_REQ_STRUCT AuthReq;

	if (Elem->MsgType == MT2_AUTH_CONF) {
		NdisMoveMemory(&Reason, Elem->Msg, sizeof (USHORT));
		if (Reason == MLME_SUCCESS) {
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH OK\n"));
			AssocParmFill(pAd, &AssocReq, pAd->MlmeAux.Bssid,
				      pAd->MlmeAux.CapabilityInfo,
				      ASSOC_TIMEOUT,
				      pAd->StaCfg.DefaultListenCount);
#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    (pAd->StaCfg.Dot11RCommInfo.FtRspSuccess != 0)) {
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
					    MT2_MLME_REASSOC_REQ,
					    sizeof (MLME_ASSOC_REQ_STRUCT),
					    &AssocReq, 0);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_REASSOC;
			} else
#endif /* DOT11R_FT_SUPPORT */
			{
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE,
					    MT2_MLME_ASSOC_REQ,
					    sizeof (MLME_ASSOC_REQ_STRUCT),
					    &AssocReq, 0);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_ASSOC;
			}
		}
		else
		{
#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    (pAd->StaCfg.Dot11RCommInfo.FtRspSuccess == FT_OTA_RESPONSE)) {
				/* failed, try next BSS */
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - FT_OTA_AUTH FAIL, give up; try next BSS\n"));
				RTMP_STA_ENTRY_MAC_RESET(pAd, BSSID_WCID);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				pAd->MlmeAux.BssIdx++;
				IterateOnBssTab(pAd);
			} else
#endif /* DOT11R_FT_SUPPORT */

#ifdef WEPAUTO_OPEN_FIRST
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch)
				    && (pAd->MlmeAux.Alg == Ndis802_11AuthModeOpen)) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - AUTH OPEN FAIL, try SHARE system...\n"));
				AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid,
					     Ndis802_11AuthModeShared);
#else
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch)
				    && (pAd->MlmeAux.Alg == Ndis802_11AuthModeShared)) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - AUTH FAIL, try OPEN system...\n"));
				AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid,
					     Ndis802_11AuthModeOpen);
#endif
				MlmeEnqueue(pAd, AUTH_STATE_MACHINE,
					    MT2_MLME_AUTH_REQ,
					    sizeof (MLME_AUTH_REQ_STRUCT),
					    &AuthReq, 0);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH2;
			} else {
				/* not success, try next BSS */
				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - AUTH FAIL, give up; try next BSS\n"));
				RTMP_STA_ENTRY_MAC_RESET(pAd, BSSID_WCID);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				pAd->MlmeAux.BssIdx++;
				IterateOnBssTab(pAd);
			}
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitAssocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Reason;

	if (Elem->MsgType == MT2_ASSOC_CONF) {
		NdisMoveMemory(&Reason, Elem->Msg, sizeof (USHORT));
		if (Reason == MLME_SUCCESS) {
			RTMPSendWirelessEvent(pAd, IW_ASSOC_EVENT_FLAG, NULL, BSS0, 0);

			LinkUp(pAd, BSS_INFRA);
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - Association successful on BSS #%ld\n", pAd->MlmeAux.BssIdx));
		} else {
			/* not success, try next BSS */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - Association fails on BSS #%ld\n", pAd->MlmeAux.BssIdx));
			RTMP_STA_ENTRY_MAC_RESET(pAd, BSSID_WCID);
			pAd->MlmeAux.BssIdx++;
			IterateOnBssTab(pAd);
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID CntlWaitReassocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT Result;

	if (Elem->MsgType == MT2_REASSOC_CONF) {
		NdisMoveMemory(&Result, Elem->Msg, sizeof (USHORT));
		if (Result == MLME_SUCCESS) {
			/* send wireless event - for association */
			RTMPSendWirelessEvent(pAd, IW_ASSOC_EVENT_FLAG, NULL,
					      BSS0, 0);

#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain &&
			    (pAd->StaCfg.Dot11RCommInfo.FtRspSuccess ==
			     FT_OTD_RESPONSE))
				InitChannelRelatedValue(pAd);
#endif /* DOT11R_FT_SUPPORT */

			/* */
			/* NDIS requires a new Link UP indication but no Link Down for RE-ASSOC */
			/* */
			LinkUp(pAd, BSS_INFRA);

			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - Re-assocition successful on BSS #%ld\n",
				  pAd->MlmeAux.RoamIdx));
		} else {
			/* reassoc failed, try to pick next BSS in the BSS Table */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - Re-assocition fails on BSS #%ld\n",
				  pAd->MlmeAux.RoamIdx));
#ifdef DOT11R_FT_SUPPORT
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
				pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain &&
				(pAd->StaCfg.Dot11RCommInfo.FtRspSuccess == FT_OTD_RESPONSE)
			)
			{
				NDIS_802_11_SSID ApSsid;

				NdisZeroMemory(&ApSsid, sizeof (NDIS_802_11_SSID));
				ApSsid.SsidLength = pAd->MlmeAux.SsidLen;
				NdisMoveMemory(ApSsid.Ssid, pAd->MlmeAux.Ssid, ApSsid.SsidLength);
				MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE,
					    OID_802_11_SSID,
					    sizeof (NDIS_802_11_SSID),
					    (VOID *) & ApSsid, 0);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			}
			else
#endif /* DOT11R_FT_SUPPORT */
			{
				pAd->MlmeAux.RoamIdx++;
				IterateOnBssTab2(pAd);
			}
		}
	}
}

VOID AdhocTurnOnQos(
	IN PRTMP_ADAPTER pAd)
{
#define AC0_DEF_TXOP		0
#define AC1_DEF_TXOP		0
#define AC2_DEF_TXOP		94
#define AC3_DEF_TXOP		47

	/* Turn on QOs if use HT rate. */
	if (pAd->CommonCfg.APEdcaParm.bValid == FALSE) {
		pAd->CommonCfg.APEdcaParm.bValid = TRUE;
		pAd->CommonCfg.APEdcaParm.Aifsn[0] = 3;
		pAd->CommonCfg.APEdcaParm.Aifsn[1] = 7;
		pAd->CommonCfg.APEdcaParm.Aifsn[2] = 1;
		pAd->CommonCfg.APEdcaParm.Aifsn[3] = 1;

		pAd->CommonCfg.APEdcaParm.Cwmin[0] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[1] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[2] = 3;
		pAd->CommonCfg.APEdcaParm.Cwmin[3] = 2;

		pAd->CommonCfg.APEdcaParm.Cwmax[0] = 10;
		pAd->CommonCfg.APEdcaParm.Cwmax[1] = 6;
		pAd->CommonCfg.APEdcaParm.Cwmax[2] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmax[3] = 3;

		pAd->CommonCfg.APEdcaParm.Txop[0] = 0;
		pAd->CommonCfg.APEdcaParm.Txop[1] = 0;
		pAd->CommonCfg.APEdcaParm.Txop[2] = AC2_DEF_TXOP;
		pAd->CommonCfg.APEdcaParm.Txop[3] = AC3_DEF_TXOP;
	}
	AsicSetEdcaParm(pAd, &pAd->CommonCfg.APEdcaParm);
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID LinkUp(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR BssType)
{
	ULONG Now;
	UINT32 Data;
	BOOLEAN Cancelled;
	UCHAR idx = 0;
	MAC_TABLE_ENTRY *pEntry = NULL;
#ifdef WSC_STA_SUPPORT
	UCHAR tmpWscSsid[MAX_LEN_OF_SSID] = { 0 };
	UCHAR tmpWscSsidLen = 0;
#endif /* WSC_STA_SUPPORT */

	/* Init ChannelQuality to prevent DEAD_CQI at initial LinkUp */
	pAd->Mlme.ChannelQuality = 50;

	/* init to not doing improved scan */
	pAd->StaCfg.bImprovedScan = FALSE;
	pAd->StaCfg.bNotFirstScan = TRUE;
	pAd->StaCfg.bAutoConnectByBssid = FALSE;

#ifdef RTMP_MAC_USB
	/* Within 10 seconds after link up, don't allow to go to sleep. */
	pAd->CountDowntoPsm = STAY_10_SECONDS_AWAKE;
#endif /* RTMP_MAC_USB */

#ifdef DOT11R_FT_SUPPORT
	pAd->StaCfg.Dot11RCommInfo.FtRspSuccess = 0;
	if ((BssType == BSS_INFRA) && (pAd->StaCfg.Dot11RCommInfo.bFtSupport)) {
		if (pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain == FALSE) {
			FT_SET_MDID(pAd->StaCfg.Dot11RCommInfo.MdIeInfo.MdId,
				    pAd->MlmeAux.MdIeInfo.MdId);
			NdisZeroMemory(pAd->StaCfg.Dot11RCommInfo.R0khId,
				       FT_ROKH_ID_LEN);
			NdisMoveMemory(pAd->StaCfg.Dot11RCommInfo.R0khId,
				       pAd->MlmeAux.FtIeInfo.R0khId,
				       pAd->MlmeAux.FtIeInfo.R0khIdLen);
			pAd->StaCfg.Dot11RCommInfo.R0khIdLen = pAd->MlmeAux.FtIeInfo.R0khIdLen;

			if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK) {
				/* Update PMK. */
				if (pAd->StaCfg.WpaPassPhraseLen == 64) {
					AtoH(pAd->StaCfg.WpaPassPhrase, pAd->StaCfg.PMK, 32);
				} else {
					UCHAR keyMaterial[40] = { 0 };
					RtmpPasswordHash(pAd->StaCfg.WpaPassPhrase,
							 pAd->MlmeAux.Ssid,
							 pAd->MlmeAux.SsidLen,
							 keyMaterial);
					NdisMoveMemory(pAd->StaCfg.PMK, keyMaterial, 32);
				}
			}
		}
		else
		{
			/* Update Reconnect Ssid, that user desired to connect. */
			NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, MAX_LEN_OF_SSID);
			NdisMoveMemory(pAd->MlmeAux.AutoReconnectSsid,
				       pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			pAd->MlmeAux.AutoReconnectSsidLen = pAd->MlmeAux.SsidLen;
		}
	}
#endif /* DOT11R_FT_SUPPORT */

	pEntry = &pAd->MacTab.Content[BSSID_WCID];

	/* */
	/* ASSOC - DisassocTimeoutAction */
	/* CNTL - Dis-associate successful */
	/* !!! LINK DOWN !!! */
	/* [88888] OID_802_11_SSID should have returned NDTEST_WEP_AP2(Returned: ) */
	/* */
	/* To prevent DisassocTimeoutAction to call Link down after we link up, */
	/* cancel the DisassocTimer no matter what it start or not. */
	/* */
	RTMPCancelTimer(&pAd->MlmeAux.DisassocTimer, &Cancelled);

#ifdef WSC_STA_SUPPORT
	tmpWscSsidLen = pAd->CommonCfg.SsidLen;
	NdisMoveMemory(tmpWscSsid, pAd->CommonCfg.Ssid, tmpWscSsidLen);
#endif /* WSC_STA_SUPPORT */
	COPY_SETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);

#ifdef DOT11_N_SUPPORT
	COPY_HTSETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);
#ifdef MESH_SUPPORT
	NdisMoveMemory(&pAd->CommonCfg.HtCapability, &pAd->MlmeAux.HtCapability, sizeof (HT_CAPABILITY_IE));
	NdisMoveMemory(&pAd->CommonCfg.AddHTInfo, &pAd->MlmeAux.AddHtInfo, sizeof (ADD_HT_INFO_IE));
#endif /* MESH_SUPPORT */
#endif /* DOT11_N_SUPPORT */

#ifdef P2P_SUPPORT
	{
		PAPCLI_STRUCT pApCliEntry = NULL;

		pApCliEntry = &pAd->ApCfg.ApCliTab[BSS0];

		if (P2P_GO_ON(pAd) || (pApCliEntry->Valid == TRUE))
		{
			pAd->CommonCfg.CentralChannel = pAd->MlmeAux.ConCurrentCentralChannel;
#ifdef DOT11_N_SUPPORT
			if (pAd->MlmeAux.bBwFallBack == TRUE)
				pAd->StaActive.SupportedHtPhy.ChannelWidth = BW_20;
#endif /* DOT11_N_SUPPORT */
		}
	}
#endif /* P2P_SUPPORT */

	if (BssType == BSS_ADHOC) {
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_ADHOC_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_INFRA_ON);

#ifdef CARRIER_DETECTION_SUPPORT	/* Roger sync Carrier */
		/* No carrier detection when adhoc */
		/* CarrierDetectionStop(pAd); */
		pAd->CommonCfg.CarrierDetect.CD_State = CD_NORMAL;
#endif /* CARRIER_DETECTION_SUPPORT */

#ifdef DOT11_N_SUPPORT
		if (WMODE_CAP_N(pAd->CommonCfg.PhyMode)
		    && (pAd->StaCfg.bAdhocN == TRUE))
			AdhocTurnOnQos(pAd);
#endif /* DOT11_N_SUPPORT */

		InitChannelRelatedValue(pAd);
	} else {
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_INFRA_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_ADHOC_ON);
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
	}
	#ifdef CONFIG_MSTARTV_CUS_SUPPORT
	if(!(P2P_GO_ON(pAd) || P2P_CLI_ON(pAd)))
	{
		pAd->CommonCfg.REGBACapability.field.RxBAWinLimit = 64;
		pAd->CommonCfg.BACapability.field.RxBAWinLimit = 64;
		DBGPRINT(RT_DEBUG_OFF,("#####only station mode ,we switch ba win size to %d\n",64));
	}
	#endif
	DBGPRINT(RT_DEBUG_TRACE, ("!!!%s LINK UP !!! \n", (BssType == BSS_ADHOC ? "ADHOC" : "Infra")));
			
	DBGPRINT(RT_DEBUG_TRACE,
		 ("!!! LINK UP !!! (BssType=%d, AID=%d, ssid=%s, Channel=%d, CentralChannel = %d)\n",
		  BssType, pAd->StaActive.Aid, pAd->CommonCfg.Ssid,
		  pAd->CommonCfg.Channel, pAd->CommonCfg.CentralChannel));

#ifdef CONFIG_MULTI_CHANNEL
	if (pAd->StaCfg.BW == BW_20)
	{
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
	}
#endif /* CONFIG_MULTI_CHANNEL */

#ifdef DOT11_N_SUPPORT
	DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !!! (Density =%d, )\n", pAd->MacTab.Content[BSSID_WCID].MpduDensity));
#endif /* DOT11_N_SUPPORT */

	/*
		We cannot move AsicSetBssid to PeerBeaconAtJoinAction because 
		PeerBeaconAtJoinAction wouldn't be invoked in roaming case.
	*/
#ifdef MESH_SUPPORT
	if (!MESH_ON(pAd))
#endif /* MESH_SUPPORT */
		AsicSetBssid(pAd, pAd->CommonCfg.Bssid);

#ifdef STREAM_MODE_SUPPORT
	/*  Enable stream mode for BSSID MAC Address */
	pEntry->StreamModeMACReg = TX_CHAIN_ADDR1_L;
	AsicSetStreamMode(pAd, &pAd->CommonCfg.Bssid[0], 1, TRUE);
#endif /* STREAM_MODE_SUPPORT */

	AsicSetSlotTime(pAd, TRUE);
	AsicSetEdcaParm(pAd, &pAd->CommonCfg.APEdcaParm);

#ifdef WMM_ACM_SUPPORT
	/* reset the ACM flag after association ok;
	   different ACM configuration for different APs */
	ACMP_EnableFlagReset(pAd,
			     pAd->CommonCfg.APEdcaParm.bACM[0],
			     pAd->CommonCfg.APEdcaParm.bACM[1],
			     pAd->CommonCfg.APEdcaParm.bACM[2],
			     pAd->CommonCfg.APEdcaParm.bACM[3]);
#endif /* WMM_ACM_SUPPORT */

	/*
		Call this for RTS protectionfor legacy rate, we will always enable RTS threshold, 
		but normally it will not hit
	*/
	AsicUpdateProtect(pAd, 0, (OFDMSETPROTECT | CCKSETPROTECT), TRUE, FALSE);

#ifdef DOT11_N_SUPPORT
	if ((pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE)) {
		/* Update HT protectionfor based on AP's operating mode. */
		if (pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent == 1) {
			AsicUpdateProtect(pAd,
					  pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_SETPROTECT, FALSE,
					  TRUE);
		} else
			AsicUpdateProtect(pAd,
					  pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_SETPROTECT, FALSE,
					  FALSE);
	}
#endif /* DOT11_N_SUPPORT */

/*
	NdisZeroMemory(&pAd->DrsCounters, sizeof (COUNTER_DRS));
*/

	NdisGetSystemUpTime(&Now);
	pAd->StaCfg.LastBeaconRxTime = Now;	/* last RX timestamp */

	if ((pAd->CommonCfg.TxPreamble != Rt802_11PreambleLong) &&
	    CAP_IS_SHORT_PREAMBLE_ON(pAd->StaActive.CapabilityInfo)) {
#ifdef RELEASE_EXCLUDE
		DBGPRINT(RT_DEBUG_INFO, ("CNTL - !!! Set to short preamble!!!\n"));
#endif /* RELEASE_EXCLUDE */
		MlmeSetTxPreamble(pAd, Rt802_11PreambleShort);
	}

	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);

	pAd->Dot11_H.RDMode = RD_NORMAL_MODE;

	if (pAd->StaCfg.WepStatus <= Ndis802_11WEPDisabled) 
	{
#ifdef WPA_SUPPLICANT_SUPPORT
		if (pAd->StaCfg.WpaSupplicantUP &&
		    (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) &&
		    (pAd->StaCfg.IEEE8021X == TRUE)) ;
		else
#endif /* WPA_SUPPLICANT_SUPPORT */
		{
			pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
		}
	}

	if (BssType == BSS_ADHOC) {
		MakeIbssBeacon(pAd);

		if ((pAd->CommonCfg.Channel > 14)
		    && (pAd->CommonCfg.bIEEE80211H == 1)
		    && RadarChannelCheck(pAd, pAd->CommonCfg.Channel)) {
			;	/*Do nothing */
		} 
		else
		{
			AsicEnableIbssSync(pAd);
		}

		/* In ad hoc mode, use MAC table from index 1. */
		/* p.s ASIC use all 0xff as termination of WCID table search.To prevent it's 0xff-ff-ff-ff-ff-ff, Write 0 here. */
		RTMP_IO_WRITE32(pAd, MAC_WCID_BASE, 0x00);
		RTMP_IO_WRITE32(pAd, 0x1808, 0x00);

		/* If WEP is enabled, add key material and cipherAlg into Asic */
		/* Fill in Shared Key Table(offset: 0x6c00) and Shared Key Mode(offset: 0x7000) */

		if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) {
			UCHAR CipherAlg;

			for (idx = 0; idx < SHARE_KEY_NUM; idx++) {
				CipherAlg = pAd->SharedKey[BSS0][idx].CipherAlg;

				if (pAd->SharedKey[BSS0][idx].KeyLen > 0) {
					/* Set key material and cipherAlg to Asic */
					AsicAddSharedKeyEntry(pAd, BSS0, idx,
							      &pAd->SharedKey[BSS0][idx]);

					if (idx == pAd->StaCfg.DefaultKeyId) {
						INT cnt;

						/* Generate 3-bytes IV randomly for software encryption using */
						for (cnt = 0; cnt < LEN_WEP_TSC; cnt++)
							pAd->SharedKey[BSS0][idx].TxTsc[cnt] = RandomByte(pAd);

						/* Update WCID attribute table and IVEIV table for this group key table */
						RTMPSetWcidSecurityInfo(pAd,
									BSS0,
									idx,
									CipherAlg,
									MCAST_WCID,
									SHAREDKEYTABLE);
					}
				}

			}
		}
		/* If WPANone is enabled, add key material and cipherAlg into Asic */
		/* Fill in Shared Key Table(offset: 0x6c00) and Shared Key Mode(offset: 0x7000) */
		else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone) {
			pAd->StaCfg.DefaultKeyId = 0;	/* always be zero */

			NdisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof (CIPHER_KEY));
			pAd->SharedKey[BSS0][0].KeyLen = LEN_TK;
			NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pAd->StaCfg.PMK, LEN_TK);

			if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled) {
				NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic,
					       &pAd->StaCfg.PMK[16], LEN_TKIP_MIC);
				NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic,
					       &pAd->StaCfg.PMK[16], LEN_TKIP_MIC);
			}

			/* Decide its ChiperAlg */
			if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
			else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
			else {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("Unknow Cipher (=%d), set Cipher to AES\n",
					  pAd->StaCfg.PairCipher));
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
			}

			/* Set key material and cipherAlg to Asic */
			AsicAddSharedKeyEntry(pAd,
					      BSS0, 0, &pAd->SharedKey[BSS0][0]);

			/* Update WCID attribute table and IVEIV table for this group key table */
			RTMPSetWcidSecurityInfo(pAd,
						BSS0, 0,
						pAd->SharedKey[BSS0][0].CipherAlg, MCAST_WCID,
						SHAREDKEYTABLE);
		}
#ifdef ADHOC_WPA2PSK_SUPPORT
		else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#ifdef WPA_SUPPLICANT_SUPPORT
			 && (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif /* WPA_SUPPLICANT_SUPPORT */
		    ) {
			USHORT Wcid = 0;

			pAd->StaCfg.DefaultKeyId = 0;	/* always be zero */

			NdisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof (CIPHER_KEY));
			pAd->SharedKey[BSS0][0].KeyLen = LEN_TK;
			NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pAd->StaCfg.PMK, LEN_TK);
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;

			/* Generate GMK and GNonce randomly */
			GenRandom(pAd, pAd->CommonCfg.Bssid, pAd->StaCfg.GMK);
			GenRandom(pAd, pAd->CommonCfg.Bssid, pAd->StaCfg.GNonce);

			/* Derive GTK per BSSID */
			WpaDeriveGTK(pAd->StaCfg.GMK,
				     pAd->StaCfg.GNonce,
				     pAd->CurrentAddress,
				     pAd->StaCfg.GTK, LEN_TKIP_GTK);

			if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled) {
				NdisZeroMemory(&pAd->StaCfg.TxGTK, sizeof (CIPHER_KEY));
				NdisMoveMemory(pAd->StaCfg.TxGTK.Key, pAd->StaCfg.GTK, LEN_TK);
				pAd->StaCfg.TxGTK.CipherAlg = CIPHER_AES;
				pAd->StaCfg.TxGTK.KeyLen = LEN_TK;

				/* Add Pair-wise key to Asic */
				GET_GroupKey_WCID(pAd, Wcid, BSS0);
				AsicAddPairwiseKeyEntry(pAd,
							(UCHAR) Wcid,
							&pAd->StaCfg.TxGTK);

				RTMPSetWcidSecurityInfo(pAd,
							BSS0, 0,
							pAd->StaCfg.TxGTK.CipherAlg, (UCHAR) Wcid,
							PAIRWISEKEYTABLE);
			}
		}
#endif /* ADHOC_WPA2PSK_SUPPORT */
	}
	else
	{		/* BSS_INFRA */
		/* Check the new SSID with last SSID */
		while (Cancelled == TRUE) {
			if (pAd->CommonCfg.LastSsidLen == pAd->CommonCfg.SsidLen) {
				if (RTMPCompareMemory(pAd->CommonCfg.LastSsid, pAd->CommonCfg.Ssid, pAd->CommonCfg.LastSsidLen) == 0) {
					/* Link to the old one no linkdown is required. */
					break;
				}
			}
#ifndef DPA_S
			/* Send link down event before set to link up */
			RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);
			pAd->ExtraInfo = GENERAL_LINK_DOWN;
			DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event AA!\n"));
#endif /* !DPA_S */
			break;
		}

		/*
			On WPA mode, Remove All Keys if not connect to the last BSSID
			Key will be set after 4-way handshake
		*/
		if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA) {
			/* Remove all WPA keys */
#ifdef PCIE_PS_SUPPORT
			RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
#endif /* PCIE_PS_SUPPORT */
/*
 		 for dhcp,issue ,wpa_supplicant ioctl too fast , at link_up, it will add key before driver remove key  
		 move to assoc.c 
 */
/*			RTMPWPARemoveAllKeys(pAd);*/
			pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilter8021xWEP;

#if 0				/* Remove by AlbertY, this routine shall be processed in WPA handshaking */
			/* Fixed connection failed with Range Maximizer - 515 AP (Marvell Chip) when security is WPAPSK/TKIP */
			/* If IV related values are too large in GroupMsg2, AP would ignore this message. */
			IV = 1;
			IV |= (pAd->StaCfg.DefaultKeyId << 30);
			AsicUpdateWCIDIVEIV(pAd, BSSID_WCID, IV, 0);
#endif

#ifdef SOFT_ENCRYPT
			/* There are some situation to need to encryption by software
			   1. The Client support PMF. It shall ony support AES cipher.
			   2. The Client support WAPI.
			   If use RT3883 or later, HW can handle the above.
			 */
#ifdef WAPI_SUPPORT
			if (!(IS_HW_WAPI_SUPPORT(pAd)) 
                                && (pAd->StaCfg.WepStatus == Ndis802_11EncryptionSMS4Enabled))
                        {
                                CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_SOFTWARE_ENCRYPT);
                        }
#endif /* WAPI_SUPPORT */

#ifdef DOT11W_PMF_SUPPORT
                        {
                                RTMP_CHIP_CAP *pChipCap = &pAd->chipCap;
                                if ((pChipCap->FlgPMFEncrtptMode == PMF_ENCRYPT_MODE_0) 
                                        && CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_PMF_CAPABLE))
                                {
                                        CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_SOFTWARE_ENCRYPT);
                                }
			}
#endif /* DOT11W_PMF_SUPPORT */

#endif /* SOFT_ENCRYPT */

		}
#ifdef DOT11R_FT_SUPPORT
		else {
			/*
			   11R only supports OPEN/NONE and WPA2PSK/TKIPAES now.
			 */
			if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
			    pAd->MlmeAux.MdIeInfo.Len &&
			    (pAd->StaCfg.AuthMode == Ndis802_11AuthModeOpen) &&
			    (pAd->StaCfg.WepStatus == Ndis802_11WEPDisabled))
				pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain = TRUE;
		}
#endif /* DOT11R_FT_SUPPORT */

		/* NOTE: */
		/* the decision of using "short slot time" or not may change dynamically due to */
		/* new STA association to the AP. so we have to decide that upon parsing BEACON, not here */

		/* NOTE: */
		/* the decision to use "RTC/CTS" or "CTS-to-self" protection or not may change dynamically */
		/* due to new STA association to the AP. so we have to decide that upon parsing BEACON, not here */

		ComposePsPoll(pAd);
		ComposeNullFrame(pAd);

#ifdef MESH_SUPPORT
		if (MESH_ON(pAd)) {
			AsicEnableMESHSync(pAd);
			if ((pAd->CommonCfg.Channel != pAd->MeshTab.MeshChannel)
#ifdef DOT11_N_SUPPORT
			    || (pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset != pAd->MeshTab.ExtChOffset)
#endif /* DOT11_N_SUPPORT */
			    ) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("Channel change , old channel:%d new channel:%d !! \n",
					  pAd->CommonCfg.Channel,
					  pAd->MeshTab.MeshChannel));
#ifdef DOT11_N_SUPPORT
				DBGPRINT(RT_DEBUG_TRACE,
					 ("Old ExtCh:%d new ExtCh:%d !! \n",
					  pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset,
					  pAd->MeshTab.ExtChOffset));
#endif /* DOT11_N_SUPPORT */
				if (pAd->MeshTab.UCGEnable == TRUE) {
					AsicSwitchChannel(pAd, pAd->MeshTab.MeshChannel, FALSE);
					AsicLockChannel(pAd, pAd->MeshTab.MeshChannel);
					pAd->MeshTab.MeshChannel = pAd->CommonCfg.Channel;
#ifdef DOT11_N_SUPPORT
					pAd->MeshTab.ExtChOffset = pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset;
#endif /* DOT11_N_SUPPORT */
					MlmeEnqueue(pAd,
						    MESH_CTRL_STATE_MACHINE,
						    MESH_CTRL_UCG_EVT, 0, NULL,
						    0);
				} else {
					pAd->MeshTab.MeshChannel = pAd->CommonCfg.Channel;
#ifdef DOT11_N_SUPPORT
					pAd->MeshTab.ExtChOffset = pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset;
#endif /* DOT11_N_SUPPORT */
					MeshMakeBeacon(pAd, MESH_BEACON_IDX(pAd));
					MeshUpdateBeaconFrame(pAd, MESH_BEACON_IDX(pAd));
				}

				TearDownAllMeshLink(pAd);

			}
		} else
#endif /* MESH_SUPPORT */
#ifdef P2P_SUPPORT
#ifdef CONFIG_MULTI_CHANNEL
		if (!((pAd->Multi_Channel_Enable == TRUE) && P2P_CLI_ON(pAd)))
#endif /* CONFIG_MULTI_CHANNEL */
		pAd->P2pCfg.GroupOpChannel = pAd->CommonCfg.Channel;

		if (P2P_GO_ON(pAd))
		{
			AsicEnableP2PGoSync(pAd);
			if ((pAd->CommonCfg.Channel != pAd->P2PChannel))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Channel change , old channel:%d new channel:%d !! \n"
							,pAd->P2PChannel,pAd->CommonCfg.Channel));
#ifdef DOT11_N_SUPPORT
				DBGPRINT(RT_DEBUG_TRACE, ("Old ExtCh:%d new ExtCh:%d !! \n"
							,pAd->P2PExtChOffset,pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset));
#endif /* DOT11_N_SUPPORT */
				P2P_GoStop(pAd);
				P2P_GoStartUp(pAd, MAIN_MBSSID);
			}
		}
		else if (P2P_CLI_ON(pAd))
		{
			AsicEnableBssSync(pAd);

			if ((pAd->CommonCfg.Channel != pAd->P2PChannel)
#ifdef CONFIG_MULTI_CHANNEL
 && (pAd->Multi_Channel_Enable == FALSE)
#endif /* CONFIG_MULTI_CHANNEL */
)
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Channel change , old channel:%d new channel:%d !! \n"
							,pAd->P2PChannel,pAd->CommonCfg.Channel));
#ifdef DOT11_N_SUPPORT
				DBGPRINT(RT_DEBUG_TRACE, ("Old ExtCh:%d new ExtCh:%d !! \n"
							,pAd->P2PExtChOffset,pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset));
#endif /* DOT11_N_SUPPORT */
				MlmeEnqueue(pAd, APCLI_CTRL_STATE_MACHINE, APCLI_CTRL_DISCONNECT_REQ, 0, NULL, 0);
			}
		}
		else
#endif /* P2P_SUPPORT */
			AsicEnableBssSync(pAd);


		/* 
			Add BSSID to WCID search table 
			We cannot move this to PeerBeaconAtJoinAction because PeerBeaconAtJoinAction wouldn't be invoked in roaming case.
		*/		
		AsicUpdateRxWCIDTable(pAd, BSSID_WCID, pAd->CommonCfg.Bssid);

		/* If WEP is enabled, add paiewise and shared key */
#ifdef WPA_SUPPLICANT_SUPPORT
		if (((pAd->StaCfg.WpaSupplicantUP) &&
		     (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) &&
		     (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED)) ||
		    ((pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE) &&
		     (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)))
#else
		if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)
#endif /* WPA_SUPPLICANT_SUPPORT */
		{
			UCHAR CipherAlg;

			for (idx = 0; idx < SHARE_KEY_NUM; idx++) {
				CipherAlg = pAd->SharedKey[BSS0][idx].CipherAlg;

				if (pAd->SharedKey[BSS0][idx].KeyLen > 0) {
					/* Set key material and cipherAlg to Asic */
					AsicAddSharedKeyEntry(pAd, BSS0, idx,
							      &pAd->SharedKey[BSS0][idx]);

					if (idx == pAd->StaCfg.DefaultKeyId) {
						/* STA doesn't need to set WCID attribute for group key */
#if 0
						/* Assign group key info */
						RTMPAddWcidAttributeEntry(pAd,
									  BSS0,
									  idx,
									  CipherAlg,
									  NULL);
#endif

						/* Assign pairwise key info to Asic */
						pEntry->Aid = BSSID_WCID;
						RTMPSetWcidSecurityInfo(pAd,
									BSS0,
									idx,
									CipherAlg,
									pEntry->Aid,
									SHAREDKEYTABLE);
					}
				}
			}
		}
#if 0 /* Move to OPSTATUS_SET_FLAG(pAd, fOP_STATUS_INFRA_ON) code segment */
		/* only INFRASTRUCTURE mode need to indicate connectivity immediately; ADHOC mode */
		/* should wait until at least 2 active nodes in this BSSID. */
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
#endif
		/* For GUI ++ */
		if (pAd->StaCfg.AuthMode < Ndis802_11AuthModeWPA) 
		{
#ifdef WPA_SUPPLICANT_SUPPORT
			if (((pAd->StaCfg.WpaSupplicantUP) && (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) && (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED)) 
				|| ((pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE) && (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)) 
				|| (pAd->StaCfg.WepStatus == Ndis802_11WEPDisabled))
#endif /* WPA_SUPPLICANT_SUPPORT */
			{
				pAd->ExtraInfo = GENERAL_LINK_UP;
				RTMP_IndicateMediaState(pAd, NdisMediaStateConnected);
			}
		} else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
			   (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
		{
#ifdef WPA_SUPPLICANT_SUPPORT
			if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif /* WPA_SUPPLICANT_SUPPORT */
				RTMPSetTimer(&pAd->Mlme.LinkDownTimer, LINK_DOWN_TIMEOUT);

#ifdef WSC_STA_SUPPORT
			if (pAd->StaCfg.WscControl.WscProfileRetryTimerRunning) {
				RTMPSetTimer(&pAd->StaCfg.WscControl.WscProfileRetryTimer,
					     WSC_PROFILE_RETRY_TIME_OUT);
			}
#endif /* WSC_STA_SUPPORT */
		}

#if 0 /* Move to StaAddMacTableEntry */
		/* Add BSSID in my MAC Table. */
		NdisAcquireSpinLock(&pAd->MacTabLock);
		/* add this MAC entry into HASH table */
		if (pEntry) {
			HashIdx = MAC_ADDR_HASH_INDEX(pAd->CommonCfg.Bssid);
			if (pAd->MacTab.Hash[HashIdx] == NULL) {
				pAd->MacTab.Hash[HashIdx] = pEntry;
			} else {
				pCurrEntry = pAd->MacTab.Hash[HashIdx];
				while (pCurrEntry->pNext != NULL) {
					pCurrEntry = pCurrEntry->pNext;
				}
				pCurrEntry->pNext = pEntry;
			}
		}
		RTMPMoveMemory(pEntry->Addr, pAd->CommonCfg.Bssid,
			       MAC_ADDR_LEN);
		pEntry->Aid = BSSID_WCID;
		pEntry->pAd = pAd;
		SET_ENTRY_CLIENT(pEntry);
#ifdef P2P_SUPPORT
		if (!(P2P_GO_ON(pAd) || P2P_CLI_ON(pAd)))
#endif /* P2P_SUPPORT */
		pAd->MacTab.Size = 1;	/* infra mode always set MACtab size =1. */
		pEntry->Sst = SST_ASSOC;
		pEntry->AuthState = SST_ASSOC;
		pEntry->AuthMode = pAd->StaCfg.AuthMode;
		pEntry->WepStatus = pAd->StaCfg.WepStatus;
		if (pEntry->AuthMode < Ndis802_11AuthModeWPA) {
			pEntry->WpaState = AS_NOTUSE;
			pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
		} else {
			pEntry->WpaState = AS_PTKSTART;
			pEntry->PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
		}
#endif

#ifdef WAC_QOS_PRIORITY
		if (pAd->MlmeAux.bForcePriority)
			pEntry->bSamsungForcePriority = TRUE;
#endif /* WAC_QOS_PRIORITY */

#ifdef DOT11R_FT_SUPPORT
		NdisAcquireSpinLock(&pAd->MacTabLock);
		pEntry->MdIeInfo.Len = 0;
		if (pAd->StaCfg.Dot11RCommInfo.bFtSupport &&
		    pAd->MlmeAux.MdIeInfo.Len &&
		    (pAd->StaCfg.WepStatus == Ndis802_11WEPDisabled
		     || pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)) {
			pEntry->MdIeInfo.Len = pAd->MlmeAux.MdIeInfo.Len;
			FT_SET_MDID(pEntry->MdIeInfo.MdId,
				    pAd->MlmeAux.MdIeInfo.MdId);
			pEntry->MdIeInfo.FtCapPlc.word =
			    pAd->MlmeAux.MdIeInfo.FtCapPlc.word;
			if (pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain == TRUE) {
				RTMPCancelTimer(&pAd->Mlme.LinkDownTimer, &Cancelled);

				if (pEntry->WepStatus != Ndis802_11WEPDisabled) {
					NdisMoveMemory(pEntry->PTK,
						       pAd->MacTab.Content[MCAST_WCID].PTK,
						       64);
					NdisReleaseSpinLock(&pAd->MacTabLock);

					WpaStaGroupKeySetting(pAd);
					WpaStaPairwiseKeySetting(pAd);
				} else
					NdisReleaseSpinLock(&pAd->MacTabLock);
			} else
				NdisReleaseSpinLock(&pAd->MacTabLock);
		}
#endif /* DOT11R_FT_SUPPORT */
#if 0
		else
			NdisReleaseSpinLock(&pAd->MacTabLock);
#endif
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !!!  ClientStatusFlags=%lx)\n",
			  pAd->MacTab.Content[BSSID_WCID].ClientStatusFlags));

#ifdef WIDI_SUPPORT						
		/*
			Send L2SDTA Association status here
		*/
		if (pAd->StaCfg.WscControl.bWscTrigger)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("WPS is in Prgress; Not sending this LINK UP\n"));
		}
		else
		{
			if ((pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPA2PSK)
				&& (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPAPSK))
			{
				WidiUpdateStateToDaemon(pAd, MIN_NET_DEVICE_FOR_MBSSID, WIDI_MSG_TYPE_ASSOC_STATUS,
								pEntry->Addr,
#ifdef WIDI_GEN4_SUPPORT
								0, 0, 0,
#endif /* WIDI_GEN4_SUPPORT */
								NULL, 0,	WIDI_ASSOCIATED);
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Auth Mode is WPA2PSK or WPAPSK; Not sending this LINK UP\n"));
			}
		}	
#endif /* WIDI_SUPPORT */

#ifdef WSC_STA_SUPPORT
		/* 
		   1. WSC initial connect to AP, set the correct parameters
		   2. When security of Marvell WPS AP is OPEN/NONE, Marvell AP will send EAP-Req(ID) to STA immediately.
		   STA needs to receive this EAP-Req(ID) on time, because Marvell AP will not send again after STA sends EAPOL-Start.
		 */
		if ((pAd->StaCfg.WscControl.WscConfMode != WSC_DISABLE) &&
		    (pAd->StaCfg.WscControl.bWscTrigger
#ifdef STA_EASY_CONFIG_SETUP
		     || pAd->StaCfg.EasyConfigInfo.bDoAutoWps
#endif /* STA_EASY_CONFIG_SETUP */
		    )) {
#ifdef DOT11R_FT_SUPPORT
			pAd->StaCfg.Dot11RCommInfo.bInMobilityDomain = FALSE;
			pAd->StaCfg.Dot11RCommInfo.FtRspSuccess = 0;
#endif /* DOT11R_FT_SUPPORT */
			RTMPCancelTimer(&pAd->Mlme.LinkDownTimer, &Cancelled);

			pAd->StaCfg.WscControl.WscState = WSC_STATE_LINK_UP;
			pAd->StaCfg.WscControl.WscStatus = WSC_STATE_LINK_UP;
			NdisZeroMemory(pAd->CommonCfg.Ssid, MAX_LEN_OF_SSID);
			NdisMoveMemory(pAd->CommonCfg.Ssid, tmpWscSsid,
				       tmpWscSsidLen);
			pAd->CommonCfg.SsidLen = tmpWscSsidLen;
		} else
			WscStop(pAd,
#ifdef CONFIG_AP_SUPPORT
							FALSE,
#endif /* CONFIG_AP_SUPPORT */
						&pAd->StaCfg.WscControl);
#endif /* WSC_STA_SUPPORT */

		MlmeUpdateTxRates(pAd, TRUE, BSS0);
#ifdef DOT11_N_SUPPORT
		MlmeUpdateHtTxRates(pAd, BSS0);
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !! (StaActive.bHtEnable =%d)\n",
			  pAd->StaActive.SupportedPhyInfo.bHtEnable));
#endif /* DOT11_N_SUPPORT */

#ifdef WAPI_SUPPORT
		if (pEntry->WepStatus == Ndis802_11EncryptionSMS4Enabled) {
			RTMPInitWapiRekeyTimerAction(pAd, pEntry);
		}
#endif /* WAPI_SUPPORT */

		if (pAd->CommonCfg.bAggregationCapable) {
			if ((pAd->CommonCfg.bPiggyBackCapable)
			    && (pAd->MlmeAux.APRalinkIe & 0x00000003) == 3) {
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_PIGGYBACK_INUSED);
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE);
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_PIGGYBACK_CAPABLE);
				RTMPSetPiggyBack(pAd, TRUE);
				DBGPRINT(RT_DEBUG_TRACE, ("Turn on Piggy-Back\n"));
			} else if (pAd->MlmeAux.APRalinkIe & 0x00000001) {
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE);
				DBGPRINT(RT_DEBUG_TRACE, ("Ralink Aggregation\n"));
			}
		}

		if (pAd->MlmeAux.APRalinkIe != 0x0) {
#ifdef DOT11_N_SUPPORT
			if (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_RDG_CAPABLE)) {
				AsicEnableRDG(pAd);
			}
#endif /* DOT11_N_SUPPORT */
			OPSTATUS_SET_FLAG(pAd, fCLIENT_STATUS_RALINK_CHIPSET);
			CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_RALINK_CHIPSET);
		} else {
			OPSTATUS_CLEAR_FLAG(pAd, fCLIENT_STATUS_RALINK_CHIPSET);
			CLIENT_STATUS_CLEAR_FLAG(pEntry, fCLIENT_STATUS_RALINK_CHIPSET);
		}
	}

#ifdef WSC_STA_SUPPORT
#ifdef WSC_LED_SUPPORT
	/* LED indication. */
	/* LEDConnectionCompletion(pAd, TRUE); */
	/*If we call LEDConnectionCompletion, it will enqueue cmdthread to send asic mcu command, */
	/*this may be happen that sending the mcu command with LED_NORMAL_CONNECTION_WITH_SECURITY will be later than */
	/*sending the mcu command with LED_LINK_UP. */
	/*It will cause Tx power LED be unusual after scanning, since firmware only do Tx power LED in link up state. */
	/*if (pAd->StaCfg.WscControl.bWPSSession == FALSE) */
	if (pAd->StaCfg.WscControl.WscConfMode == WSC_DISABLE) {
		if (LED_MODE(pAd) == WPS_LED_MODE_9) {	/* LED mode 9. */
			/* The AP uses OPEN-NONE. */
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeOpen)
			    && (pAd->StaCfg.WepStatus == Ndis802_11WEPDisabled)) {
				RTMPSetLED(pAd, LED_NORMAL_CONNECTION_WITHOUT_SECURITY);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("%s: LED_NORMAL_CONNECTION_WITHOUT_SECURITY\n",
					  __FUNCTION__));
			} else {	/* The AP uses an encryption algorithm. */

				RTMPSetLED(pAd, LED_NORMAL_CONNECTION_WITH_SECURITY);
				DBGPRINT(RT_DEBUG_TRACE,
					 ("%s: LED_NORMAL_CONNECTION_WITH_SECURITY\n",
					  __FUNCTION__));
			}
		}
	}
#endif /* WSC_LED_SUPPORT */
#endif /* WSC_STA_SUPPORT */

#ifdef DOT11_N_SUPPORT
	DBGPRINT(RT_DEBUG_TRACE,
		 ("NDIS_STATUS_MEDIA_CONNECT Event B!.BACapability = %x. ClientStatusFlags = %lx\n",
		  pAd->CommonCfg.BACapability.word,
		  pAd->MacTab.Content[BSSID_WCID].ClientStatusFlags));
#endif /* DOT11_N_SUPPORT */

#ifdef LED_CONTROL_SUPPORT
	/* Set LED */
	RTMPSetLED(pAd, LED_LINK_UP);
#endif /* LED_CONTROL_SUPPORT */

	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;
	pAd->bConfigChanged = FALSE;	/* Reset config flag */
	pAd->ExtraInfo = GENERAL_LINK_UP;	/* Update extra information to link is up */

	/* Set asic auto fall back */
	{
		UCHAR TableSize = 0;

		MlmeSelectTxRateTable(pAd, pEntry,
				      &pEntry->pTable, &TableSize,
				      &pAd->CommonCfg.TxRateIndex);
		AsicUpdateAutoFallBackTable(pAd, pEntry->pTable);
	}

	NdisAcquireSpinLock(&pAd->MacTabLock);
	pEntry->HTPhyMode.word = pAd->StaCfg.HTPhyMode.word;
	pEntry->MaxHTPhyMode.word = pAd->StaCfg.HTPhyMode.word;
	if (pAd->StaCfg.bAutoTxRateSwitch == FALSE) {
		pEntry->bAutoTxRateSwitch = FALSE;
#ifdef DOT11_N_SUPPORT
		if (pEntry->HTPhyMode.field.MCS == 32)
			pEntry->HTPhyMode.field.ShortGI = GI_800;

		if ((pEntry->HTPhyMode.field.MCS > MCS_7)
		    || (pEntry->HTPhyMode.field.MCS == 32))
			pEntry->HTPhyMode.field.STBC = STBC_NONE;
#endif /* DOT11_N_SUPPORT */
		/* If the legacy mode is set, overwrite the transmit setting of this entry. */
		if (pEntry->HTPhyMode.field.MODE <= MODE_OFDM)
			RTMPUpdateLegacyTxSetting((UCHAR) pAd->StaCfg.DesiredTransmitSetting.field.FixedTxMode, pEntry);
	} else
		pEntry->bAutoTxRateSwitch = TRUE;
	NdisReleaseSpinLock(&pAd->MacTabLock);

	/*  Let Link Status Page display first initial rate. */
	pAd->LastTxRate = (USHORT) (pEntry->HTPhyMode.word);

#if defined(RT2883) || defined(RT3883)
	/* TODO: why we don't set it for RT2883/RT3883 */
	if (IS_RT2883(pAd) || IS_RT3883(pAd)) {
		/* do nothing for DAC. */
	} else
#endif /* defined(RT2883) || defined(RT3883) */
	/* Select DAC according to HT or Legacy */
	if (pAd->StaActive.SupportedPhyInfo.MCSSet[0] != 0x00)
	{
#ifdef RT3593
		if (IS_RT3593(pAd))
			rtmp_bbp_set_txdac(pAd, pAd->Antenna.field.TxPath - 1);
		else
#endif /* RT3593 */
		{
			if (pAd->Antenna.field.TxPath >= 2)
				rtmp_bbp_set_txdac(pAd, 2);
			else
				rtmp_bbp_set_txdac(pAd, 0);
		}
	} else {
		rtmp_bbp_set_txdac(pAd, 0);
	}

#ifdef DOT11_N_SUPPORT
	if ((pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE)) {
		if (pEntry->MaxRAmpduFactor == 0) {
			/* If HT AP doesn't support MaxRAmpduFactor = 1, we need to set max PSDU to 0. */
			/* Because our Init value is 1 at MACRegTable. */
			RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x000A0fff);	/* Default set to 0x1fff. But if one peer can't support 0x1fff, we need to change to 0xfff */
		} else if (pEntry->MaxRAmpduFactor == 1) {
			RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x000A1fff);
		} else if (pEntry->MaxRAmpduFactor == 2) {
			RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x000A2fff);
		} else if (pEntry->MaxRAmpduFactor == 3) {
			RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x000A3fff);
		}
		DBGPRINT(RT_DEBUG_TRACE,
			 ("!!!MaxRAmpduFactor= %d \n",
			  pEntry->MaxRAmpduFactor));
	}
#endif /* DOT11_N_SUPPORT */

#ifdef RELEASE_EXCLUDE
	/* Patch for Marvel AP to gain high throughput, need to set as following,
		1. Set txop in register-EDCA_AC0_CFG as 0x60
		2. Set EnTXWriteBackDDONE in register-WPDMA_GLO_CFG as zero
		3. PBF_MAX_PCNT as 0x1F3FBF9F
		4. kick per two packets when dequeue
	*/
#endif /* RELEASE_EXCLUDE */
	/* Txop can only be modified when RDG is off, WMM is disable and TxBurst is enable */
	/* if 1. Legacy AP WMM on,  or 2. 11n AP, AMPDU disable.  Force turn off burst no matter what bEnableTxBurst is. */
#ifdef DOT11_N_SUPPORT
	if (!((pAd->CommonCfg.RxStream == 1) && (pAd->CommonCfg.TxStream == 1))
	    && (pAd->StaCfg.bForceTxBurst == FALSE)
	    &&
	    (((pAd->StaActive.SupportedPhyInfo.bHtEnable == FALSE)
	      && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
	     || ((pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE)
		 && (pAd->CommonCfg.BACapability.field.Policy == BA_NOTUSE)))) {
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data &= 0xFFFFFF00;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);

#ifndef RLT_MAC
		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3F7F9F);
#endif
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 1\n"));
	} else
#endif /* DOT11_N_SUPPORT */
	if (pAd->CommonCfg.bEnableTxBurst) {
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data &= 0xFFFFFF00;
		Data |= 0x60;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);
		pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = TRUE;
#ifdef RTMP_MAC
		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3FBF9F);
#endif
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 2\n"));
	} else {
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data &= 0xFFFFFF00;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);
#ifdef RTMP_MAC
		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3F7F9F);
#endif
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 3\n"));
	}

#ifdef DOT11_N_SUPPORT
	/* Re-check to turn on TX burst or not. */
	if ((pAd->CommonCfg.IOTestParm.bLastAtheros == TRUE)
	    && ((STA_WEP_ON(pAd)) || (STA_TKIP_ON(pAd)))) {
		pAd->CommonCfg.IOTestParm.bNextDisableRxBA = TRUE;
		if (pAd->CommonCfg.bEnableTxBurst) {
			UINT32 MACValue = 0;
			/* Force disable TXOP value in this case. The same action in MLMEUpdateProtect too */
			/* I didn't change PBF_MAX_PCNT setting. */
			RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &MACValue);
			MACValue &= 0xFFFFFF00;
			RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, MACValue);
			pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = FALSE;
		}
	} else {
		pAd->CommonCfg.IOTestParm.bNextDisableRxBA = FALSE;
	}
#endif /* DOT11_N_SUPPORT */

	pAd->CommonCfg.IOTestParm.bLastAtheros = FALSE;

#ifdef WPA_SUPPLICANT_SUPPORT
	/*
	   If STA connects to different AP, STA couldn't send EAPOL_Start for WpaSupplicant.
	 */
	if ((pAd->StaCfg.BssType == BSS_INFRA) &&
	    (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) &&
	    (NdisEqualMemory(pAd->CommonCfg.Bssid, pAd->CommonCfg.LastBssid, MAC_ADDR_LEN) == FALSE) &&
	     (pAd->StaCfg.bLostAp == TRUE)) {
		pAd->StaCfg.bLostAp = FALSE;
	}
#endif /* WPA_SUPPLICANT_SUPPORT */
	/*
	   Need to check this COPY. This COPY is from Windows Driver.
	 */
#ifdef DOT11V_WNM_SUPPORT
	WNM_Init(pAd);
#endif /* DOT11V_WNM_SUPPORT */

	COPY_MAC_ADDR(pAd->CommonCfg.LastBssid, pAd->CommonCfg.Bssid);
	DBGPRINT(RT_DEBUG_TRACE,
		 ("!!!pAd->bNextDisableRxBA= %d \n",
		  pAd->CommonCfg.IOTestParm.bNextDisableRxBA));
	/* BSSID add in one MAC entry too.  Because in Tx, ASIC need to check Cipher and IV/EIV, BAbitmap */
	/* Pther information in MACTab.Content[BSSID_WCID] is not necessary for driver. */
	/* Note: As STA, The MACTab.Content[BSSID_WCID]. PairwiseKey and Shared Key for BSS0 are the same. */

	NdisAcquireSpinLock(&pAd->MacTabLock);
	pEntry->PortSecured = pAd->StaCfg.PortSecured;
	NdisReleaseSpinLock(&pAd->MacTabLock);

	/* */
	/* Patch Atheros AP TX will breakdown issue. */
	/* AP Model: DLink DWL-8200AP */
	/* */
	if (INFRA_ON(pAd) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED)
	    && STA_TKIP_ON(pAd)) {
		RTMP_IO_WRITE32(pAd, RX_PARSER_CFG, 0x01);
	} else {
		RTMP_IO_WRITE32(pAd, RX_PARSER_CFG, 0x00);
	}

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
	/*RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_GO_TO_SLEEP_NOW); */

#ifdef WSC_STA_SUPPORT
	/* WSC initial connect to AP, jump to Wsc start action and set the correct parameters */
	if ((pAd->StaCfg.WscControl.WscConfMode != WSC_DISABLE) &&
	    (pAd->StaCfg.WscControl.bWscTrigger
#ifdef STA_EASY_CONFIG_SETUP
	     || (pAd->StaCfg.EasyConfigInfo.bDoAutoWps
		 && (pAd->StaCfg.WscControl.WscConfMode == WSC_ENROLLEE))
#endif /* STA_EASY_CONFIG_SETUP */
	    )) {
		RTMPusecDelay(100000);	/* 100 ms */
		if (pAd->StaCfg.BssType == BSS_INFRA) {
			NdisMoveMemory(pAd->StaCfg.WscControl.WscPeerMAC,
				       pAd->CommonCfg.Bssid, MAC_ADDR_LEN);
			NdisMoveMemory(pAd->StaCfg.WscControl.EntryAddr,
				       pAd->CommonCfg.Bssid, MAC_ADDR_LEN);
			WscSendEapolStart(pAd,
					  pAd->StaCfg.WscControl.WscPeerMAC, STA_MODE);
		}
#ifdef STA_EASY_CONFIG_SETUP
		if (pAd->StaCfg.EasyConfigInfo.bDoAutoWps) {
			RTMPCancelTimer(&pAd->StaCfg.EasyConfigInfo.InfraConnTimer, &Cancelled);
			RTMPSetTimer(&pAd->StaCfg.EasyConfigInfo.InfraConnTimer, WSC_TWO_MINS_TIME_OUT);
		}
#endif /* STA_EASY_CONFIG_SETUP */
	}
#endif /* WSC_STA_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	if (INFRA_ON(pAd)) {
		if ((pAd->CommonCfg.bBssCoexEnable == TRUE)
		    && (pAd->CommonCfg.Channel <= 14)
		    && (pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE)
		    && (pAd->MlmeAux.ExtCapInfo.BssCoexistMgmtSupport == 1)) {
			OPSTATUS_SET_FLAG(pAd, fOP_STATUS_SCAN_2040);
			BuildEffectedChannelList(pAd);
			DBGPRINT(RT_DEBUG_TRACE,
				 ("LinkUP AP supports 20/40 BSS COEX, Dot11BssWidthTriggerScanInt[%d]\n",
				  pAd->CommonCfg.Dot11BssWidthTriggerScanInt));
		} else {
			DBGPRINT(RT_DEBUG_TRACE, ("not supports 20/40 BSS COEX !!! \n"));
			DBGPRINT(RT_DEBUG_TRACE,
				 ("pAd->CommonCfg Info: bBssCoexEnable=%d, Channel=%d, CentralChannel=%d, PhyMode=%d\n",
					pAd->CommonCfg.bBssCoexEnable, pAd->CommonCfg.Channel,
					pAd->CommonCfg.CentralChannel, pAd->CommonCfg.PhyMode));
			DBGPRINT(RT_DEBUG_TRACE,
				 ("pAd->StaActive.SupportedHtPhy.bHtEnable=%d\n",
				  pAd->StaActive.SupportedPhyInfo.bHtEnable));
			DBGPRINT(RT_DEBUG_TRACE,
				 ("pAd->MlmeAux.ExtCapInfo.BssCoexstSup=%d\n",
				  pAd->MlmeAux.ExtCapInfo.BssCoexistMgmtSupport));
		}
	}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

#ifdef WPA_SUPPLICANT_SUPPORT
	/*
	   When AuthMode is WPA2-Enterprise and AP reboot or STA lost AP, 
	   WpaSupplicant would not send EapolStart to AP after STA re-connect to AP again.
	   In this case, driver would send EapolStart to AP.
	 */
	if ((pAd->StaCfg.BssType == BSS_INFRA) &&
	    (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) &&
	    (NdisEqualMemory
	     (pAd->CommonCfg.Bssid, pAd->CommonCfg.LastBssid, MAC_ADDR_LEN))
	    && (pAd->StaCfg.bLostAp == TRUE)) {
		WpaSendEapolStart(pAd, pAd->CommonCfg.Bssid);
		pAd->StaCfg.bLostAp = FALSE;
	}
#endif /* WPA_SUPPLICANT_SUPPORT */

#ifdef CONFIG_MULTI_CHANNEL
if (pAd->Multi_Channel_Enable == TRUE)
	RtmpPrepareHwNullFrame(pAd,
						&pAd->MacTab.Content[BSSID_WCID],
						FALSE,
						FALSE,
						0,
						OPMODE_STA,
						PWR_SAVE,
						0,
						0);
#endif /* CONFIG_MULTI_CHANNEL */

#ifdef MT7601
	if ( IS_MT7601(pAd) )
	{
#ifdef DPD_CALIBRATION_SUPPORT
		/* DPD-Calibration */
		DBGPRINT(RT_DEBUG_TRACE,("TX DPD Calibration\n"));
		AndesCalibrationOP(pAd, ANDES_CALIBRATION_DPD, pAd->chipCap.CurrentTemperature);
#endif /* DPD_CALIBRATION_SUPPORT */

		// MT7601_RXDC_CAL
		MT7601_RXDC_CAL(pAd);
 	}
#endif /* MT7601 */

#ifdef THREADX
#ifdef EBS_RTIP
	dc_log_printf("IFACEID=%d", RtmpIfaceId);
	net_ipsource_signal(DC_NET_DRV_LINK_STATUS_UP, RtmpIfaceId);
	/*ipsource_signal(DC_NET_DRV_LINK_STATUS_UP, ifaceId); */
#endif /* EBS_RTIP */

#endif /* THREADX */
	pAd->MacTab.MsduLifeTime = 5; /* default 5 seconds */

#ifdef CONFIG_MULTI_CHANNEL
	if (pAd->Multi_Channel_Enable == TRUE)
	{
		RTMP_IO_READ32(pAd, WMM_CTRL, &Data);
		Data &= 0x7fffffff;/* bit 31 set to 0 */	/*  WMM Channel switch to EDCA1 */
		RTMP_IO_WRITE32(pAd, WMM_CTRL, Data);

		if ((pAd->StaCfg.WscControl.WscConfMode != WSC_DISABLE) &&
		    (pAd->StaCfg.WscControl.bWscTrigger
		    )) 
		{
				DBGPRINT(RT_DEBUG_TRACE,
					 ("WSC trigger not set Multi-channel!!\n"));
		}
		else if (P2P_CLI_ON(pAd)  
			&& pEntry->PortSecured == WPA_802_1X_PORT_SECURED
			&& pEntry->WepStatus == Ndis802_11WEPDisabled)
		{
			RTMPSetTimer(&pAd->Mlme.MCCTimer, pAd->Mlme.EDCAToHCCATimerValue);
		}
		pAd->Mlme.StaStayTick = 0;
		if (P2P_CLI_ON(pAd))
			pAd->StaCfg.bAutoReconnect = FALSE;
	}
#endif /* CONFIG_MULTI_CHANNEL */


#ifdef MICROWAVE_OVEN_SUPPORT
	pAd->CommonCfg.MO_Cfg.bEnable = TRUE;
#endif /* MICROWAVE_OVEN_SUPPORT */

}


/*
	==========================================================================

	Routine	Description:
		Disconnect current BSSID

	Arguments:
		pAd				- Pointer to our adapter
		IsReqFromAP		- Request from AP
		
	Return Value:		
		None
		
	IRQL = DISPATCH_LEVEL

	Note:
		We need more information to know it's this requst from AP.
		If yes! we need to do extra handling, for example, remove the WPA key.
		Otherwise on 4-way handshaking will faied, since the WPA key didn't be
		remove while auto reconnect.
		Disconnect request from AP, it means we will start afresh 4-way handshaking 
		on WPA mode.

	==========================================================================
*/
VOID LinkDown(
	IN PRTMP_ADAPTER pAd,
	IN BOOLEAN IsReqFromAP)
{
	UCHAR i;

	/* Do nothing if monitor mode is on */
	if (MONITOR_ON(pAd))
		return;

#ifdef CONFIG_MULTI_CHANNEL
	if (pAd->Multi_Channel_Enable == TRUE)
		MultiChannelTimerStop(pAd);
#endif /* CONFIG_MULTI_CHANNEL */


#ifdef RALINK_ATE
	/* Nothing to do in ATE mode. */
	if (ATE_ON(pAd))
		return;
#endif /* RALINK_ATE */

#ifdef PCIE_PS_SUPPORT
	/* Not allow go to sleep within linkdown function. */
	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
#endif /* PCIE_PS_SUPPORT */

#ifdef WMM_ACM_SUPPORT
	ACMP_StationDelete(pAd, ACMR_STA_ENTRY_GET(pAd, pAd->CommonCfg.Bssid));
#endif /* WMM_ACM_SUPPORT */

	RTMPSendWirelessEvent(pAd, IW_STA_LINKDOWN_EVENT_FLAG, NULL, BSS0, 0);

	DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN !!!\n"));
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);

#ifdef WIFI_P2P_CONCURRENT_FAST_SCAN
    /* reset to not doing improved scan when both STA & P2P are LinkDown */
    if (!P2P_GO_ON(pAd) && !P2P_CLI_ON(pAd))
        pAd->StaCfg.bImprovedScan = FALSE;
#else
	/* reset to not doing improved scan */
	pAd->StaCfg.bImprovedScan = FALSE;
#endif

#ifdef PCIE_PS_SUPPORT

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_ADVANCE_POWER_SAVE_PCIE_DEVICE)) {
		BOOLEAN Cancelled;
		pAd->Mlme.bPsPollTimerRunning = FALSE;
		RTMPCancelTimer(&pAd->Mlme.PsPollTimer, &Cancelled);
	}

	pAd->bPCIclkOff = FALSE;
#endif /* PCIE_PS_SUPPORT */
#ifdef RELEASE_EXCLUDE
/*
The variable, fRTMP_PS_SET_PCI_CLK_OFF_COMMAND, comes form Windows source codes.
They use it to indicate whether or not the PCIe device is going power-saving.
In Window's codes, RTMPPCIeLinkCtrlSetting will turn on this flag and turn off in RTMPPCIeLinkCtrlRestore.
*/
#endif /* RELEASE_EXCLUDE */
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)
/*||	RTMP_TEST_PSFLAG(pAd, fRTMP_PS_SET_PCI_CLK_OFF_COMMAND) */
	    || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF)) {
		AUTO_WAKEUP_STRUC AutoWakeupCfg;
		AsicForceWakeup(pAd, TRUE);
		AutoWakeupCfg.word = 0;
		RTMP_IO_WRITE32(pAd, AUTO_WAKEUP_CFG, AutoWakeupCfg.word);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_DOZE);
	}

	if (ADHOC_ON(pAd)) {	/* Adhoc mode link down */
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN 1!!!\n"));

#ifdef ADHOC_WPA2PSK_SUPPORT
		/* In an IBSS, a STA's SME responds to Deauthenticate frames from a STA by */
		/* deleting the PTKSA associated with that STA. (Spec. P802.11i/D10 P.19) */
		if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK) {
			for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++) {
				if (IS_ENTRY_CLIENT(&pAd->MacTab.Content[i]))
					MlmeDeAuthAction(pAd,
							 &pAd->MacTab.
							 Content[i],
							 REASON_DEAUTH_STA_LEAVING,
							 FALSE);
			}
		}
#endif /* ADHOC_WPA2PSK_SUPPORT */

		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_ADHOC_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
		RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);
		pAd->ExtraInfo = GENERAL_LINK_DOWN;
		BssTableDeleteEntry(&pAd->ScanTab, pAd->CommonCfg.Bssid,
				    pAd->CommonCfg.Channel);
		DBGPRINT(RT_DEBUG_TRACE,
			 ("!!! MacTab.Size=%d !!!\n", pAd->MacTab.Size));
	} else {		/* Infra structure mode */

		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN 2!!!\n"));

#ifdef QOS_DLS_SUPPORT
		/* DLS tear down frame must be sent before link down */
		/* send DLS-TEAR_DOWN message */
		if (pAd->CommonCfg.bDLSCapable) {
			/* tear down local dls table entry */
			for (i = 0; i < MAX_NUM_OF_INIT_DLS_ENTRY; i++) {
				if (pAd->StaCfg.DLSEntry[i].Valid
				    && (pAd->StaCfg.DLSEntry[i].Status ==
					DLS_FINISH)) {
					pAd->StaCfg.DLSEntry[i].Status =
					    DLS_NONE;
					RTMPSendDLSTearDownFrame(pAd,
								 pAd->StaCfg.
								 DLSEntry[i].
								 MacAddr);
				}
			}

			/* tear down peer dls table entry */
			for (i = MAX_NUM_OF_INIT_DLS_ENTRY;
			     i < MAX_NUM_OF_DLS_ENTRY; i++) {
				if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH)) {
					pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
					RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
				}
			}
		}
#endif /* QOS_DLS_SUPPORT */

#ifdef DOT11Z_TDLS_SUPPORT
		if (IS_TDLS_SUPPORT(pAd)) {
			TDLS_LinkTearDown(pAd, TRUE);
		}
#endif /* DOT11Z_TDLS_SUPPORT */

#ifdef STA_EASY_CONFIG_SETUP
		if (IsReqFromAP) {
			/*
			   STA cannot know WEP key is valid or invalid in TX path(AP may change WEP key).
			   Change WepStatus to NONE for forcing STA to do auto provision again.
			 */
			if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)
				pAd->StaCfg.WepStatus = Ndis802_11WEPDisabled;

			pAd->Mlme.AutoProvisionMachine.CurrState =
			    INFRA_AUTO_PROVISION;
			MlmeEnqueue(pAd, AUTO_PROVISION_STATE_MACHINE,
				    MT2_CONN_AP, 0, NULL, 0);
			RTMP_MLME_HANDLER(pAd);
		}
		pAd->StaCfg.EasyConfigInfo.bDoAutoWps = FALSE;
#ifdef WAC_SUPPORT
		pAd->StaCfg.EasyConfigInfo.bWACAP = FALSE;
#endif /* WAC_SUPPORT */
#endif /* STA_EASY_CONFIG_SETUP */

		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_INFRA_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

#ifdef DOT11V_WNM_SUPPORT
		WNM_Init(pAd);
#endif /* DOT11V_WNM_SUPPORT */

		/* Saved last SSID for linkup comparison */
		pAd->CommonCfg.LastSsidLen = pAd->CommonCfg.SsidLen;
		NdisMoveMemory(pAd->CommonCfg.LastSsid, pAd->CommonCfg.Ssid,
			       pAd->CommonCfg.LastSsidLen);
		COPY_MAC_ADDR(pAd->CommonCfg.LastBssid, pAd->CommonCfg.Bssid);
		if (pAd->MlmeAux.CurrReqIsFromNdis == TRUE) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("NDIS_STATUS_MEDIA_DISCONNECT Event A!\n"));
			pAd->MlmeAux.CurrReqIsFromNdis = FALSE;
		} else {
			if ((pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED)
			    || (pAd->StaCfg.AuthMode < Ndis802_11AuthModeWPA)) {
				/* */
				/* If disassociation request is from NDIS, then we don't need to delete BSSID from entry. */
				/* Otherwise lost beacon or receive De-Authentication from AP, */
				/* then we should delete BSSID from BssTable. */
				/* If we don't delete from entry, roaming will fail. */
				/* */
				BssTableDeleteEntry(&pAd->ScanTab,
						    pAd->CommonCfg.Bssid,
						    pAd->CommonCfg.Channel);
			}
		}

		/* restore back to - */
		/*      1. long slot (20 us) or short slot (9 us) time */
		/*      2. turn on/off RTS/CTS and/or CTS-to-self protection */
		/*      3. short preamble */
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);

#ifdef EXT_BUILD_CHANNEL_LIST
		/* Country IE of the AP will be evaluated and will be used. */
		if (pAd->StaCfg.IEEE80211dClientMode != Rt802_11_D_None) {
			NdisMoveMemory(&pAd->CommonCfg.CountryCode[0],
				       &pAd->StaCfg.StaOriCountryCode[0], 2);
			pAd->CommonCfg.Geography = pAd->StaCfg.StaOriGeography;
			BuildChannelListEx(pAd);
		}
#endif /* EXT_BUILD_CHANNEL_LIST */

	}

#ifdef WAPI_SUPPORT
	if (pAd->StaCfg.WepStatus == Ndis802_11EncryptionSMS4Enabled) {
		/* Clear Replay counter */
		/*NdisZeroMemory(pAd->StaCfg.rx_iv, LEN_WAPI_TSC); */

		/* Cancel rekey timer */
		RTMPCancelWapiRekeyTimerAction(pAd,
					       &pAd->MacTab.
					       Content[BSSID_WCID]);
	}
#endif /* WAPI_SUPPORT */

	for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++) {
		if (IS_ENTRY_CLIENT(&pAd->MacTab.Content[i])
#ifdef P2P_SUPPORT
			&& (!IS_P2P_GO_ENTRY(&pAd->MacTab.Content[i]))
#endif /* P2P_SUPPORT */
			)
			MacTableDeleteEntry(pAd, pAd->MacTab.Content[i].Aid,
					    pAd->MacTab.Content[i].Addr);
	}

	AsicSetSlotTime(pAd, TRUE);	/*FALSE); */
#ifdef P2P_SUPPORT
	if ((!P2P_GO_ON(pAd)) && (!P2P_CLI_ON(pAd)))
#endif /* P2P_SUPPORT */
	AsicSetEdcaParm(pAd, NULL);

#ifdef LED_CONTROL_SUPPORT
	/* Set LED */
	RTMPSetLED(pAd, LED_LINK_DOWN);
	pAd->LedCntl.LedIndicatorStrength = 0xF0;
	RTMPSetSignalLED(pAd, -100);	/* Force signal strength Led to be turned off, firmware is not done it. */
#endif /* LED_CONTROL_SUPPORT */

#ifdef MESH_SUPPORT
	if (MESH_ON(pAd) && !ADHOC_ON(pAd))
		AsicEnableMESHSync(pAd);
	else
#endif /* MESH_SUPPORT */
#ifdef P2P_SUPPORT
	if (!P2P_GO_ON(pAd))
#endif /* P2P_SUPPORT */
		AsicDisableSync(pAd);

	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;

#ifdef DOT11_N_SUPPORT
	NdisZeroMemory(&pAd->MlmeAux.HtCapability, sizeof (HT_CAPABILITY_IE));
	NdisZeroMemory(&pAd->MlmeAux.AddHtInfo, sizeof (ADD_HT_INFO_IE));
	pAd->MlmeAux.HtCapabilityLen = 0;
	pAd->MlmeAux.NewExtChannelOffset = 0xff;

	DBGPRINT(RT_DEBUG_TRACE, ("LinkDownCleanMlmeAux.ExtCapInfo!\n"));
	NdisZeroMemory((PUCHAR) (&pAd->MlmeAux.ExtCapInfo),
		       sizeof (EXT_CAP_INFO_ELEMENT));
#endif /* DOT11_N_SUPPORT */

	/* Reset WPA-PSK state. Only reset when supplicant enabled */
	if (pAd->StaCfg.WpaState != SS_NOTUSE) {
		pAd->StaCfg.WpaState = SS_START;
		/* Clear Replay counter */
		NdisZeroMemory(pAd->StaCfg.ReplayCounter, 8);

#ifdef QOS_DLS_SUPPORT
		if (pAd->CommonCfg.bDLSCapable)
			NdisZeroMemory(pAd->StaCfg.DlsReplayCounter, 8);
#endif /* QOS_DLS_SUPPORT */
	}

	/* */
	/* if link down come from AP, we need to remove all WPA keys on WPA mode. */
	/* otherwise will cause 4-way handshaking failed, since the WPA key not empty. */
	/* */
	if ((IsReqFromAP) && (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)) {
		/* Remove all WPA keys */
		RTMPWPARemoveAllKeys(pAd);
	}

	/* 802.1x port control */
#ifdef WPA_SUPPLICANT_SUPPORT
	/* Prevent clear PortSecured here with static WEP */
	/* NetworkManger set security policy first then set SSID to connect AP. */
	if (pAd->StaCfg.WpaSupplicantUP &&
	    (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) &&
	    (pAd->StaCfg.IEEE8021X == FALSE)) {
		pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
	} else
#endif /* WPA_SUPPLICANT_SUPPORT */
	{
		pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
		pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
	}

	NdisAcquireSpinLock(&pAd->MacTabLock);
	NdisZeroMemory(&pAd->MacTab.Content[BSSID_WCID], sizeof(MAC_TABLE_ENTRY));
	pAd->MacTab.Content[BSSID_WCID].PortSecured = pAd->StaCfg.PortSecured;
	NdisReleaseSpinLock(&pAd->MacTabLock);

	pAd->StaCfg.MicErrCnt = 0;

	RTMP_IndicateMediaState(pAd, NdisMediaStateDisconnected);
	/* Update extra information to link is up */
	pAd->ExtraInfo = GENERAL_LINK_DOWN;

	pAd->StaActive.SupportedPhyInfo.bHtEnable = FALSE;

#ifdef RTMP_MAC_USB
	pAd->bUsbTxBulkAggre = FALSE;
#endif /* RTMP_MAC_USB */

	/* Clean association information */
	NdisZeroMemory(&pAd->StaCfg.AssocInfo,
		       sizeof (NDIS_802_11_ASSOCIATION_INFORMATION));
	pAd->StaCfg.AssocInfo.Length =
	    sizeof (NDIS_802_11_ASSOCIATION_INFORMATION);
	pAd->StaCfg.ReqVarIELen = 0;
	pAd->StaCfg.ResVarIELen = 0;


	/* Reset RSSI value after link down */
	NdisZeroMemory((PUCHAR) (&pAd->StaCfg.RssiSample),
		       sizeof (pAd->StaCfg.RssiSample));

	/* Restore MlmeRate */
	pAd->CommonCfg.MlmeRate = pAd->CommonCfg.BasicMlmeRate;
	pAd->CommonCfg.RtsRate = pAd->CommonCfg.BasicMlmeRate;

#ifdef DOT11_N_SUPPORT
	// TODO: shiang-6590, why we need to fallback to BW_20 here? How about the BW_10?
	if (pAd->CommonCfg.BBPCurrentBW != BW_20) {
#ifdef P2P_SUPPORT
		PAPCLI_STRUCT pApCliEntry = NULL;
				
		pApCliEntry = &pAd->ApCfg.ApCliTab[BSS0];
		
		if (!P2P_GO_ON(pAd) && (pApCliEntry->Valid == FALSE))
#endif /* P2P_SUPPORT */
		{
			rtmp_bbp_set_bw(pAd, BW_20);
		}
	}
#endif /* DOT11_N_SUPPORT */

#if defined(RT2883) || defined(RT3883)
	if (IS_RT2883(pAd) || IS_RT3883(pAd)) {
		/* do nothing for DAC */
	} else
#endif /* defined(RT2883) || defined(RT3883) */
	{
		/* Reset DAC */
#ifdef RT3593
		if (IS_RT3593(pAd))
			rtmp_bbp_set_txdac(pAd, pAd->Antenna.field.TxPath - 1);
		else
#endif /* RT3593 */
		{
			if (pAd->Antenna.field.TxPath >= 2)
				rtmp_bbp_set_txdac(pAd, 2);
			else
				rtmp_bbp_set_txdac(pAd, 0);
		}
	}

	/* After Link down, reset piggy-back setting in ASIC. Disable RDG. */
	RTMPSetPiggyBack(pAd, FALSE);
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_PIGGYBACK_INUSED);

#ifdef DOT11_N_SUPPORT
	pAd->CommonCfg.BACapability.word = pAd->CommonCfg.REGBACapability.word;
#endif /* DOT11_N_SUPPORT */

	/* Restore all settings in the following. */
	AsicUpdateProtect(pAd, 0,
			  (ALLN_SETPROTECT | CCKSETPROTECT | OFDMSETPROTECT),
			  TRUE, FALSE);
#ifdef DOT11_N_SUPPORT
	AsicDisableRDG(pAd);
#endif /* DOT11_N_SUPPORT */
	pAd->CommonCfg.IOTestParm.bCurrentAtheros = FALSE;
	pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = FALSE;

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SCAN_2040);
	pAd->CommonCfg.BSSCoexist2040.word = 0;
	TriEventInit(pAd);
	for (i = 0; i < (pAd->ChannelListNum - 1); i++) {
		pAd->ChannelList[i].bEffectedChannel = FALSE;
	}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

	RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x1fff);
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
/* Allow go to sleep after linkdown steps. */
#ifdef  PCIE_PS_SUPPORT

	RTMP_SET_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
#endif /* PCIE_PS_SUPPORT */

#ifdef WIDI_SUPPORT
	/*
		Send L2SDTA Disassociation status here
	*/
	if (pAd->StaCfg.WscControl.bWscTrigger)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WPS is in Prgress; Not sending this LINK DOWN\n"));
	}
	else
	{
		/* LastBssid is zero; Not sending this LINK DOWN */
		if (!MAC_ADDR_EQUAL(pAd->CommonCfg.LastBssid, ZERO_MAC_ADDR))
		{
			WidiUpdateStateToDaemon(pAd, MIN_NET_DEVICE_FOR_MBSSID, WIDI_MSG_TYPE_ASSOC_STATUS,
				pAd->CommonCfg.LastBssid,
#ifdef WIDI_GEN4_SUPPORT
				0, 0, 0,
#endif /* WIDI_GEN4_SUPPORT */			
				NULL, 0, WIDI_DISASSOCIATED);
		}
		else
			DBGPRINT(RT_DEBUG_ERROR, ("LastBssid is zero; Not sending this LINK DOWN\n"));
	}
#endif /* WIDI_SUPPORT */

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.WpaSupplicantUP) {
		/*send disassociate event to wpa_supplicant */
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM,
					RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
	}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CGIWAP, -1, NULL,
				NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

#ifdef RT30xx
	if ((IS_RT3071(pAd) || IS_RT3572(pAd) || IS_RT3593(pAd))
	    && (pAd->Antenna.field.RxPath > 1 || pAd->Antenna.field.TxPath > 1)) {
		RTMP_ASIC_MMPS_DISABLE(pAd);
	}
#endif /* RT30xx */

	if (pAd->StaCfg.BssType != BSS_ADHOC)
		pAd->StaCfg.bNotFirstScan = FALSE;
	else
		pAd->StaCfg.bAdhocCreator = FALSE;

/*After change from one ap to another , we need to re-init rssi for AdjustTxPower  */
	pAd->StaCfg.RssiSample.AvgRssi0	= -127;
	pAd->StaCfg.RssiSample.AvgRssi1	= -127;
	pAd->StaCfg.RssiSample.AvgRssi2	= -127;

	NdisZeroMemory(pAd->StaCfg.ConnectinfoSsid, MAX_LEN_OF_SSID);
	NdisZeroMemory(pAd->StaCfg.ConnectinfoBssid, MAC_ADDR_LEN);
	pAd->StaCfg.ConnectinfoSsidLen  = 0;
	pAd->StaCfg.ConnectinfoBssType  = 1;
	pAd->StaCfg.ConnectinfoChannel = 0;

#ifdef RTMP_FREQ_CALIBRATION_SUPPORT
	if (pAd->chipCap.FreqCalibrationSupport) 
		StopFrequencyCalibration(pAd);
#endif /* RTMP_FREQ_CALIBRATION_SUPPORT */

#ifdef CONFIG_MULTI_CHANNEL
	if (pAd->Multi_Channel_Enable == TRUE)
	{
		if (P2P_CLI_ON(pAd)/* && pAd->P2pCfg.bStartP2pConnect*/)
		{
			BssTableDeleteEntry(&pAd->ScanTab, pAd->CommonCfg.Bssid,
					    pAd->CommonCfg.Channel);
			
			pAd->StaCfg.bAutoReconnect = TRUE;
		}

		if (P2P_CLI_ON(pAd) && (!pAd->P2pCfg.bStartP2pConnect))
			MultiChannelSwitchToP2P(pAd);

		UINT32 Value=0;
		RTUSBReadMACRegister(pAd, PBF_CFG, &Value);
		Value |= ((1 << 3) | (1 << 13));/* bit 3 and bit 13 set to 1 */
		Value |= ((1 << 2) | (1 << 12));/* bit 2  and bit 12 set to 1 */
		RTUSBWriteMACRegister(pAd, PBF_CFG, Value,FALSE);
		pAd->MultiChannelFlowCtl=0;

	/* when p2p & p2p all connnect , when rao disconnect then scan , channel will back to common channel so p2p will also disconnect... , */
		pAd->CommonCfg.Channel = pAd->ApCliMlmeAux.Channel;
	}
#endif /*CONFIG_MULTI_CHANNEL*/


#ifdef MICROWAVE_OVEN_SUPPORT
	pAd->CommonCfg.MO_Cfg.bEnable = FALSE;
#endif /* MICROWAVE_OVEN_SUPPORT */

#if 1
	RTUSBRejectPendingPackets(pAd);
	UCHAR                    acidx;
	for(acidx=0; acidx<NUM_OF_TX_RING; acidx++)
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

	//RTMP_IO_WRITE32(pAd, 0x106C, 0x3); //clear HW Q
	//RTMPusecDelay(1000);	/* 1 ms */
	DBGPRINT(RT_DEBUG_ERROR, ("Clear Q when LINK DOWN\n"));
	//RTMP_IO_WRITE32(pAd, 0x106C, 0x0);
#endif


}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID IterateOnBssTab(
	IN PRTMP_ADAPTER pAd)
{
	MLME_START_REQ_STRUCT StartReq;
	MLME_JOIN_REQ_STRUCT JoinReq;
	ULONG BssIdx;
	BSS_ENTRY *pInBss = NULL;

	/* Change the wepstatus to original wepstatus */
	pAd->StaCfg.PairCipher = pAd->StaCfg.WepStatus;
#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif /* WPA_SUPPLICANT_SUPPORT */
	pAd->StaCfg.GroupCipher = pAd->StaCfg.WepStatus;

	BssIdx = pAd->MlmeAux.BssIdx;

	if (pAd->StaCfg.BssType == BSS_ADHOC) {
		if (BssIdx < pAd->MlmeAux.SsidBssTab.BssNr) {
			pInBss = &pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx];
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - iterate BSS %ld of %d\n", BssIdx,
				  pAd->MlmeAux.SsidBssTab.BssNr));
			JoinParmFill(pAd, &JoinReq, BssIdx);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_JOIN_REQ,
				    sizeof (MLME_JOIN_REQ_STRUCT), &JoinReq, 0);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
#ifdef WSC_STA_SUPPORT
			if (pAd->StaCfg.WscControl.WscState >= WSC_STATE_START) {
				NdisMoveMemory(pAd->StaCfg.WscControl.
					       WscPeerMAC, pInBss->MacAddr,
					       MAC_ADDR_LEN);
				NdisMoveMemory(pAd->StaCfg.WscControl.EntryAddr,
					       pInBss->MacAddr, MAC_ADDR_LEN);
			}
#endif /* WSC_STA_SUPPORT */
			return;
		}
#ifdef IWSC_SUPPORT
		if (pAd->StaCfg.WscControl.bWscTrigger == TRUE)
		{
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			MlmeEnqueue(pAd, IWSC_STATE_MACHINE, IWSC_MT2_MLME_RECONNECT, 0, NULL, 0);
			RTMP_MLME_HANDLER(pAd);
		}
		else
#endif /* IWSC_SUPPORT */
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - All BSS fail; start a new ADHOC (Ssid=%s)...\n",pAd->MlmeAux.Ssid));
			StartParmFill(pAd, &StartReq, (PCHAR) pAd->MlmeAux.Ssid,pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof (MLME_START_REQ_STRUCT), &StartReq, 0);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}
	} else if ((BssIdx < pAd->MlmeAux.SsidBssTab.BssNr) &&
		   (pAd->StaCfg.BssType == BSS_INFRA)) {
		pInBss = &pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx];
		/* Check cipher suite, AP must have more secured cipher than station setting */
		/* Set the Pairwise and Group cipher to match the intended AP setting */
		/* We can only connect to AP with less secured cipher setting */
		if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA)
		    || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)) {
			pAd->StaCfg.GroupCipher = pInBss->WPA.GroupCipher;

			if (pAd->StaCfg.WepStatus == pInBss->WPA.PairCipher)
				pAd->StaCfg.PairCipher = pInBss->WPA.PairCipher;
			else if (pInBss->WPA.PairCipherAux != Ndis802_11WEPDisabled)
				pAd->StaCfg.PairCipher = pInBss->WPA.PairCipherAux;
			else	/* There is no PairCipher Aux, downgrade our capability to TKIP */
				pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
		} else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2)
			   || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)) {
			pAd->StaCfg.GroupCipher = pInBss->WPA2.GroupCipher;

			if (pAd->StaCfg.WepStatus == pInBss->WPA2.PairCipher)
				pAd->StaCfg.PairCipher = pInBss->WPA2.PairCipher;
			else if (pInBss->WPA2.PairCipherAux != Ndis802_11WEPDisabled)
				pAd->StaCfg.PairCipher = pInBss->WPA2.PairCipherAux;
			else	/* There is no PairCipher Aux, downgrade our capability to TKIP */
				pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;

			/* RSN capability */
			pAd->StaCfg.RsnCapability = pInBss->WPA2.RsnCapability;
		}
#ifdef WAPI_SUPPORT
		else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAICERT)
			 || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWAIPSK)) {
			pAd->StaCfg.GroupCipher = pInBss->WAPI.GroupCipher;
			pAd->StaCfg.PairCipher = pInBss->WAPI.PairCipher;
		}
#endif /* WAPI_SUPPORT */

		/* Set Mix cipher flag */
		pAd->StaCfg.bMixCipher =
		    (pAd->StaCfg.PairCipher ==
		     pAd->StaCfg.GroupCipher) ? FALSE : TRUE;
#if 0
		if (pAd->StaCfg.bMixCipher == TRUE) {
			/* If mix cipher, re-build RSNIE */
			RTMPMakeRSNIE(pAd, pAd->StaCfg.AuthMode,
				      pAd->StaCfg.WepStatus, 0);
		}
#endif
		DBGPRINT(RT_DEBUG_TRACE,
			 ("CNTL - iterate BSS %ld of %d\n", BssIdx,
			  pAd->MlmeAux.SsidBssTab.BssNr));
		JoinParmFill(pAd, &JoinReq, BssIdx);
		MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_JOIN_REQ,
			    sizeof (MLME_JOIN_REQ_STRUCT), &JoinReq, 0);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
	}
	else
	{
		/* no more BSS */
#ifdef DOT11_N_SUPPORT
#ifdef MESH_SUPPORT
		if ((pAd->CommonCfg.BBPCurrentBW == BW_40) && MESH_ON(pAd)) {
			UINT32 Data;
			UCHAR Value = 0, rf_channel = 0;
			INT ext_ch;

			/* Change to mesh channel */
			if (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40) {
				if (pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel) {
					rf_channel = pAd->CommonCfg.CentralChannel;
					ext_ch = EXTCHA_ABOVE;
				} 
				else if (pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel)
				{
					rf_channel = pAd->CommonCfg.CentralChannel;
					ext_ch = EXTCHA_BELOW;
				}
			}

			if (rf_channel != 0)
			{
				AsicSetChannel(pAd, rf_channel, BW_40, ext_ch, FALSE);

				DBGPRINT(RT_DEBUG_TRACE,
					 ("%s(): All roaming failed, restore to Channel(CtrlCh=%d, CentralCh=%d)\n",
					__FUNCTION__, pAd->CommonCfg.Channel,
					pAd->CommonCfg.CentralChannel));
			}
		}
		else
#endif /* MESH_SUPPORT */
#endif /* DOT11_N_SUPPORT */
		{
#if 0
			AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);
			AsicLockChannel(pAd, pAd->CommonCfg.Channel);
#endif
#ifdef CONFIG_MULTI_CHANNEL
			if (P2P_CLI_ON(pAd) && (pAd->Multi_Channel_Enable == TRUE))
			{
				AsicSwitchChannel(pAd, pAd->ApCliMlmeAux.Channel, FALSE);
				AsicLockChannel(pAd, pAd->ApCliMlmeAux.Channel);

				DBGPRINT(RT_DEBUG_TRACE,
					 ("CNTL - All roaming failed,  p2p_on restore to p2p channel channel %d, Total BSS[%02d]\n",
					  pAd->ApCliMlmeAux.Channel, pAd->ScanTab.BssNr));

			}
			else
#endif /* CONFIG_MULTI_CHANNEL */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("CNTL - All roaming failed, restore to channel %d, Total BSS[%02d]\n",
				  pAd->CommonCfg.Channel, pAd->ScanTab.BssNr));
		}

#ifdef P2P_SUPPORT
		if (P2P_GO_ON(pAd) || P2P_CLI_ON(pAd))
			; /* Do NOT delete this entry here because ra interface cannot do all channel scan in this case. */
		else
#endif /* P2P_SUPPORT */
		{
		pAd->MlmeAux.SsidBssTab.BssNr = 0;
		BssTableDeleteEntry(&pAd->ScanTab,
				    pAd->MlmeAux.SsidBssTab.BssEntry[0].Bssid,
							pAd->MlmeAux.SsidBssTab.BssEntry[0].Channel);
		}

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
#ifdef WSC_STA_SUPPORT
#ifdef WSC_LED_SUPPORT
		/* LED indication. */
		LEDConnectionCompletion(pAd, FALSE);
#endif /* WSC_LED_SUPPORT */
#endif /* WSC_STA_SUPPORT */
	}
}

/* for re-association only */
/* IRQL = DISPATCH_LEVEL */
VOID IterateOnBssTab2(
	IN PRTMP_ADAPTER pAd)
{
	MLME_REASSOC_REQ_STRUCT ReassocReq;
	ULONG BssIdx;
	BSS_ENTRY *pBss;

	BssIdx = pAd->MlmeAux.RoamIdx;
	pBss = &pAd->MlmeAux.RoamTab.BssEntry[BssIdx];

	if (BssIdx < pAd->MlmeAux.RoamTab.BssNr) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("CNTL - iterate BSS %ld of %d\n", BssIdx,
			  pAd->MlmeAux.RoamTab.BssNr));

		AsicSwitchChannel(pAd, pBss->Channel, FALSE);
		AsicLockChannel(pAd, pBss->Channel);

		/* reassociate message has the same structure as associate message */
		AssocParmFill(pAd, &ReassocReq, pBss->Bssid,
			      pBss->CapabilityInfo, ASSOC_TIMEOUT,
			      pAd->StaCfg.DefaultListenCount);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_REASSOC_REQ,
			    sizeof (MLME_REASSOC_REQ_STRUCT), &ReassocReq, 0);

		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_REASSOC;
	} else {
		/* no more BSS */
		UCHAR rf_channel = 0, rf_bw;
		INT ext_ch;
			
#ifdef DOT11_N_SUPPORT
#ifdef MESH_SUPPORT
		if ((pAd->CommonCfg.BBPCurrentBW == BW_40) && MESH_ON(pAd)) {
			/* Change to mesh channel */
			if ((pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel)
			    && (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40)) {
				rf_channel = pAd->CommonCfg.CentralChannel;
				rf_bw = BW_40;
				ext_ch = EXTCHA_ABOVE;
			}
			else if ((pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel)
				&& (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40))
			{
				rf_channel = pAd->CommonCfg.CentralChannel;
				rf_bw = BW_40;
				ext_ch = EXTCHA_BELOW;
			}
		}
		else
#endif /* MESH_SUPPORT */
#endif /* DOT11_N_SUPPORT */
		{
			rf_channel = pAd->CommonCfg.Channel;
			rf_bw = BW_20;
			ext_ch = EXTCHA_NONE;
		}

		if (rf_channel != 0) {
			AsicSetChannel(pAd, rf_channel, rf_bw, ext_ch, FALSE);

			DBGPRINT(RT_DEBUG_TRACE,
				 ("%s():CNTL - All roaming failed, restore to Channel(Ctrl=%d, Central = %d)\n",
				  __FUNCTION__, pAd->CommonCfg.Channel,
				  pAd->CommonCfg.CentralChannel));
		}

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID JoinParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_JOIN_REQ_STRUCT *JoinReq,
	IN ULONG BssIdx)
{
	JoinReq->BssIdx = BssIdx;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID ScanParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_SCAN_REQ_STRUCT *ScanReq,
	IN STRING Ssid[],
	IN UCHAR SsidLen,
	IN UCHAR BssType,
	IN UCHAR ScanType)
{
	NdisZeroMemory(ScanReq->Ssid, MAX_LEN_OF_SSID);
	ScanReq->SsidLen = SsidLen;
	NdisMoveMemory(ScanReq->Ssid, Ssid, SsidLen);
	ScanReq->BssType = BssType;
	ScanReq->ScanType = ScanType;
}

#ifdef QOS_DLS_SUPPORT
/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID DlsParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_DLS_REQ_STRUCT *pDlsReq,
	IN PRT_802_11_DLS pDls,
	IN USHORT reason)
{
	pDlsReq->pDLS = pDls;
	pDlsReq->Reason = reason;
}
#endif /* QOS_DLS_SUPPORT */

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID StartParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_START_REQ_STRUCT *StartReq,
	IN CHAR Ssid[],
	IN UCHAR SsidLen)
{
	ASSERT(SsidLen <= MAX_LEN_OF_SSID);
	if (SsidLen > MAX_LEN_OF_SSID)
		SsidLen = MAX_LEN_OF_SSID;
	NdisMoveMemory(StartReq->Ssid, Ssid, SsidLen);
	StartReq->SsidLen = SsidLen;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
VOID AuthParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_AUTH_REQ_STRUCT *AuthReq,
	IN PUCHAR pAddr,
	IN USHORT Alg)
{
	COPY_MAC_ADDR(AuthReq->Addr, pAddr);
	AuthReq->Alg = Alg;
	AuthReq->Timeout = AUTH_TIMEOUT;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL
	
	==========================================================================
 */
#ifdef RTMP_MAC_USB

VOID MlmeCntlConfirm(
	IN PRTMP_ADAPTER pAd,
	IN ULONG MsgType,
	IN USHORT Msg)
{
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MsgType, sizeof (USHORT),
		    &Msg, 0);
}
#endif /* RTMP_MAC_USB */

/*
	==========================================================================
	Description:
		Pre-build a BEACON frame in the shared memory

	IRQL = PASSIVE_LEVEL
	IRQL = DISPATCH_LEVEL
	
	==========================================================================
*/
ULONG MakeIbssBeacon(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR DsLen = 1, IbssLen = 2;
	UCHAR LocalErpIe[3] = { IE_ERP, 1, 0x04 };
	HEADER_802_11 BcnHdr;
	USHORT CapabilityInfo;
	LARGE_INTEGER FakeTimestamp;
	ULONG FrameLen = 0;
	TXWI_STRUC *pTxWI = &pAd->BeaconTxWI;
	UCHAR *pBeaconFrame = pAd->BeaconBuf;
	BOOLEAN Privacy;
	UCHAR SupRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR SupRateLen = 0;
	UCHAR ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR ExtRateLen = 0;

	if (WMODE_EQUAL(pAd->CommonCfg.PhyMode, WMODE_B)
	    && (pAd->CommonCfg.Channel <= 14)) {
		SupRate[0] = 0x82;	/* 1 mbps */
		SupRate[1] = 0x84;	/* 2 mbps */
		SupRate[2] = 0x8b;	/* 5.5 mbps */
		SupRate[3] = 0x96;	/* 11 mbps */
		SupRateLen = 4;
		ExtRateLen = 0;
	} else if (pAd->CommonCfg.Channel > 14) {
		SupRate[0] = 0x8C;	/* 6 mbps, in units of 0.5 Mbps, basic rate */
		SupRate[1] = 0x12;	/* 9 mbps, in units of 0.5 Mbps */
		SupRate[2] = 0x98;	/* 12 mbps, in units of 0.5 Mbps, basic rate */
		SupRate[3] = 0x24;	/* 18 mbps, in units of 0.5 Mbps */
		SupRate[4] = 0xb0;	/* 24 mbps, in units of 0.5 Mbps, basic rate */
		SupRate[5] = 0x48;	/* 36 mbps, in units of 0.5 Mbps */
		SupRate[6] = 0x60;	/* 48 mbps, in units of 0.5 Mbps */
		SupRate[7] = 0x6c;	/* 54 mbps, in units of 0.5 Mbps */
		SupRateLen = 8;
		ExtRateLen = 0;

		/* */
		/* Also Update MlmeRate & RtsRate for G only & A only */
		/* */
		pAd->CommonCfg.MlmeRate = RATE_6;
		pAd->CommonCfg.RtsRate = RATE_6;
		pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_OFDM;
		pAd->CommonCfg.MlmeTransmit.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
		pAd->MacTab.Content[BSS0Mcast_WCID].HTPhyMode.field.MODE = MODE_OFDM;
		pAd->MacTab.Content[BSS0Mcast_WCID].HTPhyMode.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
	} else {
		SupRate[0] = 0x82;	/* 1 mbps */
		SupRate[1] = 0x84;	/* 2 mbps */
		SupRate[2] = 0x8b;	/* 5.5 mbps */
		SupRate[3] = 0x96;	/* 11 mbps */
		SupRateLen = 4;

		ExtRate[0] = 0x0C;	/* 6 mbps, in units of 0.5 Mbps, */
		ExtRate[1] = 0x12;	/* 9 mbps, in units of 0.5 Mbps */
		ExtRate[2] = 0x18;	/* 12 mbps, in units of 0.5 Mbps, */
		ExtRate[3] = 0x24;	/* 18 mbps, in units of 0.5 Mbps */
		ExtRate[4] = 0x30;	/* 24 mbps, in units of 0.5 Mbps, */
		ExtRate[5] = 0x48;	/* 36 mbps, in units of 0.5 Mbps */
		ExtRate[6] = 0x60;	/* 48 mbps, in units of 0.5 Mbps */
		ExtRate[7] = 0x6c;	/* 54 mbps, in units of 0.5 Mbps */
		ExtRateLen = 8;
	}

	pAd->StaActive.SupRateLen = SupRateLen;
	NdisMoveMemory(pAd->StaActive.SupRate, SupRate, SupRateLen);
	pAd->StaActive.ExtRateLen = ExtRateLen;
	NdisMoveMemory(pAd->StaActive.ExtRate, ExtRate, ExtRateLen);

	/* compose IBSS beacon frame */
	MgtMacHeaderInit(pAd, &BcnHdr, SUBTYPE_BEACON, 0, BROADCAST_ADDR,
#ifdef P2P_SUPPORT
						pAd->CurrentAddress,
#endif /* P2P_SUPPORT */
			 pAd->CommonCfg.Bssid);
	Privacy = (pAd->StaCfg.WepStatus == Ndis802_11Encryption1Enabled)
	    || (pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
	    || (pAd->StaCfg.WepStatus == Ndis802_11Encryption3Enabled);
	CapabilityInfo =
	    CAP_GENERATE(0, 1, Privacy,
			 (pAd->CommonCfg.TxPreamble == Rt802_11PreambleShort),
			 0, 0);

	MakeOutgoingFrame(pBeaconFrame, &FrameLen,
			  sizeof (HEADER_802_11), &BcnHdr,
			  TIMESTAMP_LEN, &FakeTimestamp,
			  2, &pAd->CommonCfg.BeaconPeriod,
			  2, &CapabilityInfo,
			  1, &SsidIe,
			  1, &pAd->CommonCfg.SsidLen,
			  pAd->CommonCfg.SsidLen, pAd->CommonCfg.Ssid,
			  1, &SupRateIe,
			  1, &SupRateLen,
			  SupRateLen, SupRate,
			  1, &DsIe,
			  1, &DsLen,
			  1, &pAd->CommonCfg.Channel,
			  1, &IbssIe,
			  1, &IbssLen, 2, &pAd->StaActive.AtimWin, END_OF_ARGS);

	/* add ERP_IE and EXT_RAE IE of in 802.11g */
	if (ExtRateLen) {
		ULONG tmp;

		MakeOutgoingFrame(pBeaconFrame + FrameLen, &tmp,
				  3, LocalErpIe,
				  1, &ExtRateIe,
				  1, &ExtRateLen,
				  ExtRateLen, ExtRate, END_OF_ARGS);
		FrameLen += tmp;
	}

	/* If adhoc secruity is set for WPA-None, append the cipher suite IE */
	/* Modify by Eddy, support WPA2PSK in Adhoc mode */
	if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
#ifdef ADHOC_WPA2PSK_SUPPORT
	    || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#endif /* ADHOC_WPA2PSK_SUPPORT */
	    ) {
		UCHAR RSNIe = IE_WPA;
		ULONG tmp;

		RTMPMakeRSNIE(pAd, pAd->StaCfg.AuthMode, pAd->StaCfg.WepStatus,
			      BSS0);
#ifdef ADHOC_WPA2PSK_SUPPORT
		if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
			RSNIe = IE_RSN;
#endif /* ADHOC_WPA2PSK_SUPPORT */

		MakeOutgoingFrame(pBeaconFrame + FrameLen, &tmp,
				  1, &RSNIe,
				  1, &pAd->StaCfg.RSNIE_Len,
				  pAd->StaCfg.RSNIE_Len, pAd->StaCfg.RSN_IE,
				  END_OF_ARGS);
		FrameLen += tmp;
	}
#ifdef WSC_STA_SUPPORT
	/* add Simple Config Information Element */
	if (pAd->StaCfg.WpsIEBeacon.ValueLen != 0) {
		ULONG WscTmpLen = 0;

		MakeOutgoingFrame(pBeaconFrame + FrameLen, &WscTmpLen,
				  pAd->StaCfg.WpsIEBeacon.ValueLen,
				  pAd->StaCfg.WpsIEBeacon.Value, END_OF_ARGS);
		FrameLen += WscTmpLen;
	}
#endif /* WSC_STA_SUPPORT */

#ifdef DOT11_N_SUPPORT
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode)
	    && (pAd->StaCfg.bAdhocN == TRUE)) {
		ULONG TmpLen;
		UCHAR HtLen, HtLen1;

#ifdef RT_BIG_ENDIAN
		HT_CAPABILITY_IE HtCapabilityTmp;
		ADD_HT_INFO_IE addHTInfoTmp;
		USHORT b2lTmp, b2lTmp2;
#endif

		/* add HT Capability IE */
		HtLen = sizeof (pAd->CommonCfg.HtCapability);
		HtLen1 = sizeof (pAd->CommonCfg.AddHTInfo);
#ifndef RT_BIG_ENDIAN
		MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
				  1, &HtCapIe,
				  1, &HtLen,
				  HtLen, &pAd->CommonCfg.HtCapability,
				  1, &AddHtInfoIe,
				  1, &HtLen1,
				  HtLen1, &pAd->CommonCfg.AddHTInfo,
				  END_OF_ARGS);
#else
		NdisMoveMemory(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability,
			       HtLen);
		*(USHORT *) (&HtCapabilityTmp.HtCapInfo) =
		    SWAP16(*(USHORT *) (&HtCapabilityTmp.HtCapInfo));
		*(USHORT *) (&HtCapabilityTmp.ExtHtCapInfo) =
		    SWAP16(*(USHORT *) (&HtCapabilityTmp.ExtHtCapInfo));

		NdisMoveMemory(&addHTInfoTmp, &pAd->CommonCfg.AddHTInfo,
			       HtLen1);
		*(USHORT *) (&addHTInfoTmp.AddHtInfo2) =
		    SWAP16(*(USHORT *) (&addHTInfoTmp.AddHtInfo2));
		*(USHORT *) (&addHTInfoTmp.AddHtInfo3) =
		    SWAP16(*(USHORT *) (&addHTInfoTmp.AddHtInfo3));

		MakeOutgoingFrame(pBeaconFrame + FrameLen, &TmpLen,
				  1, &HtCapIe,
				  1, &HtLen,
				  HtLen, &HtCapabilityTmp,
				  1, &AddHtInfoIe,
				  1, &HtLen1,
				  HtLen1, &addHTInfoTmp, END_OF_ARGS);
#endif
		FrameLen += TmpLen;
	}
#endif /* DOT11_N_SUPPORT */

	/*beacon use reserved WCID 0xff */
	if (pAd->CommonCfg.Channel > 14) {
		RTMPWriteTxWI(pAd, pTxWI, FALSE, FALSE, TRUE, FALSE, FALSE,
			      TRUE, 0, 0xff, FrameLen, PID_MGMT, PID_BEACON,
			      RATE_1, IFS_HTTXOP, FALSE,
			      &pAd->CommonCfg.MlmeTransmit);
	} else {
		/* Set to use 1Mbps for Adhoc beacon. */
		HTTRANSMIT_SETTING Transmit;
		Transmit.word = 0;
		RTMPWriteTxWI(pAd, pTxWI, FALSE, FALSE, TRUE, FALSE, FALSE,
			      TRUE, 0, 0xff, FrameLen, PID_MGMT, PID_BEACON,
			      RATE_1, IFS_HTTXOP, FALSE, &Transmit);
	}

#ifdef RT_BIG_ENDIAN
	RTMPFrameEndianChange(pAd, pBeaconFrame, DIR_WRITE, FALSE);
	RTMPWIEndianChange(pAd, (PUCHAR) pTxWI, TYPE_TXWI);
#endif

	DBGPRINT(RT_DEBUG_TRACE,
		 ("MakeIbssBeacon (len=%ld), SupRateLen=%d, ExtRateLen=%d, Channel=%d, PhyMode=%d\n",
		  FrameLen, SupRateLen, ExtRateLen, pAd->CommonCfg.Channel,
		  pAd->CommonCfg.PhyMode));
	return FrameLen;
}

VOID InitChannelRelatedValue(RTMP_ADAPTER *pAd)
{
	UCHAR rf_channel, rf_bw;
	INT ext_ch;
	
#ifdef RTMP_MAC_PCI
	/* In power save , We will force use 1R. */
	/* So after link up, check Rx antenna # again. */
	rtmp_bbp_set_rxpath(pAd, pAd->Antenna.field.RxPath);
#endif /* RTMP_MAC_PCI */

	pAd->CommonCfg.CentralChannel = pAd->MlmeAux.CentralChannel;
	pAd->CommonCfg.Channel = pAd->MlmeAux.Channel;
#ifdef DOT11_N_SUPPORT
	/* Change to AP channel */
	if ((pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel)
	    && (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40)) {
	    	rf_channel = pAd->CommonCfg.CentralChannel;
		rf_bw = BW_40;
		ext_ch = EXTCHA_ABOVE;
	}
	else if ((pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel)
			&& (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40))
	{
	    	rf_channel = pAd->CommonCfg.CentralChannel;
		rf_bw = BW_40;
		ext_ch = EXTCHA_BELOW;
	} else
#endif /* DOT11_N_SUPPORT */
	{
	    	rf_channel = pAd->CommonCfg.CentralChannel;
		rf_bw = BW_20;
		ext_ch = EXTCHA_NONE;
	}

	AsicSetChannel(pAd, rf_channel, rf_bw, ext_ch, FALSE);

	DBGPRINT(RT_DEBUG_TRACE,
		 ("%s():BW_%s, CtrlChannel=%d, CentralChannel=%d\n",
		  __FUNCTION__, (rf_bw == BW_40 ? "40" : "20"),
		  pAd->CommonCfg.Channel, 
		  pAd->CommonCfg.CentralChannel));

	/* Save BBP_R66 value, it will be used in RTUSBResumeMsduTransmission */
	rtmp_bbp_get_agc(pAd, &pAd->BbpTuning.R66CurrentValue, RX_CHAIN_0);
}

/* IRQL = DISPATCH_LEVEL */
VOID MaintainBssTable(
	IN PRTMP_ADAPTER pAd,
	IN OUT BSS_TABLE *Tab,
	IN ULONG MaxRxTimeDiff,
	IN UCHAR MaxSameRxTimeCount)
{
	UCHAR	i, j;
	UCHAR	total_bssNr = Tab->BssNr;
	BOOLEAN	bDelEntry = FALSE;
	ULONG	now_time = 0;

	for (i = 0; i < total_bssNr; i++) 
	{
		PBSS_ENTRY	pBss = &Tab->BssEntry[i];

		bDelEntry = FALSE;
		if (pBss->LastBeaconRxTimeA != pBss->LastBeaconRxTime)
		{
			pBss->LastBeaconRxTimeA = pBss->LastBeaconRxTime;
			pBss->SameRxTimeCount = 0;
		}
		else
			pBss->SameRxTimeCount++;

		NdisGetSystemUpTime(&now_time);
		if (RTMP_TIME_AFTER(now_time, pBss->LastBeaconRxTime + (MaxRxTimeDiff * OS_HZ)))
			bDelEntry = TRUE;		
		else if (pBss->SameRxTimeCount > MaxSameRxTimeCount)
			bDelEntry = TRUE;
		
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)
			&& NdisEqualMemory(pBss->Ssid, pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen))
			bDelEntry = FALSE;
		
		if (bDelEntry)
		{
			UCHAR *pOldAddr = NULL;
			
			for (j = i; j < total_bssNr - 1; j++)
			{
				pOldAddr = Tab->BssEntry[j].pVarIeFromProbRsp;
				NdisMoveMemory(&(Tab->BssEntry[j]), &(Tab->BssEntry[j + 1]), sizeof(BSS_ENTRY));
				if (pOldAddr)
				{
					RTMPZeroMemory(pOldAddr, MAX_VIE_LEN);
					NdisMoveMemory(pOldAddr, 
								   Tab->BssEntry[j + 1].pVarIeFromProbRsp, 
								   Tab->BssEntry[j + 1].VarIeFromProbeRspLen);
					Tab->BssEntry[j].pVarIeFromProbRsp = pOldAddr;
				}
			}

			pOldAddr = Tab->BssEntry[total_bssNr - 1].pVarIeFromProbRsp;
			NdisZeroMemory(&(Tab->BssEntry[total_bssNr - 1]), sizeof(BSS_ENTRY));
			if (pOldAddr)
			{
				RTMPZeroMemory(pOldAddr, MAX_VIE_LEN);
			}
			
			total_bssNr -= 1;
			i -= 1;
		}
	}
	Tab->BssNr = total_bssNr;
}

VOID AdjustChannelRelatedValue(
	IN PRTMP_ADAPTER pAd,
	OUT UCHAR *pBwFallBack,
	IN USHORT ifIndex,
	IN BOOLEAN BandWidth,
	IN UCHAR PriCh,
	IN UCHAR ExtraCh
#ifdef CONFIG_MULTI_CHANNEL
	,IN BOOLEAN bfromP2P
#endif /*CONFIG_MULTI_CHANNEL*/
	)
{
	UCHAR rf_channel, rf_bw = BW_20;
	INT ext_ch;
#ifdef P2P_SUPPORT
	PAPCLI_STRUCT pApCliEntry = NULL;
#endif /* P2P_SUPPORT */


	// TODO: shiang-6590, this function need to revise to make sure two purpose can achieved!
	//	1. Channel-binding rule between STA and P2P-GO mode
	//	2. Stop MAC Tx/Rx when bandwidth change

	*pBwFallBack = 0;

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		return;

	DBGPRINT(RT_DEBUG_TRACE, ("%s():CentralChannel=%d, Channel=%d, ChannelWidth=%d\n",
			__FUNCTION__, ExtraCh, PriCh, BandWidth));


#ifdef CONFIG_MULTI_CHANNEL


	pApCliEntry = &pAd->ApCfg.ApCliTab[ifIndex];

		if ((bfromP2P) && (pAd->Multi_Channel_Enable == TRUE))
		{
			if (BandWidth == BW_40)
			{
				pAd->ApCliMlmeAux.CentralChannel = ExtraCh;
				pAd->ApCliMlmeAux.Channel = PriCh;
				pAd->P2pCfg.BW = BW_40;
			}
			else
			{
				pAd->ApCliMlmeAux.CentralChannel = PriCh;
				pAd->ApCliMlmeAux.Channel = PriCh;
				pAd->P2pCfg.BW = BW_20;
			}
		}
		else
		{
			if (BandWidth == BW_40)
			{
				pAd->CommonCfg.CentralChannel = ExtraCh;
				pAd->CommonCfg.Channel = PriCh;
				pAd->StaCfg.BW = BW_40;
			}
			else
			{
				pAd->CommonCfg.CentralChannel = PriCh;
				pAd->CommonCfg.Channel = PriCh;
				pAd->StaCfg.BW = BW_20;
			}		
		}

#else
	pAd->CommonCfg.CentralChannel = ExtraCh;
	pAd->CommonCfg.Channel = PriCh;
#endif /*CONFIG_MULTI_CHANNEL*/




#ifdef CONFIG_MULTI_CHANNEL

	if ((bfromP2P) && (pAd->Multi_Channel_Enable == TRUE))
#ifdef DOT11_N_SUPPORT
	{
		if ((pAd->ApCliMlmeAux.CentralChannel  > pAd->ApCliMlmeAux.Channel) && (BandWidth == BW_40))
		{
			rf_channel = pAd->CommonCfg.CentralChannel;
			rf_bw = BW_40;
			ext_ch = EXTCHA_ABOVE;
		} 
		else if ((pAd->ApCliMlmeAux.CentralChannel  < pAd->ApCliMlmeAux.Channel) && (BandWidth == BW_40))
		{
			rf_channel = pAd->ApCliMlmeAux.CentralChannel;
			rf_bw = BW_40;
			ext_ch = EXTCHA_BELOW;
		}
		else
#endif /* DOT11_N_SUPPORT */
		{
			rf_channel = pAd->ApCliMlmeAux.Channel;
			rf_bw = BW_20;
			ext_ch = EXTCHA_NONE;
		}
	}
	else
#endif /*CONFIG_MULTI_CHANNEL*/
#ifdef DOT11_N_SUPPORT
	/* Change to AP channel */
	if ((pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel) && (BandWidth == BW_40))
	{
		rf_channel = pAd->CommonCfg.CentralChannel;
		rf_bw = BW_40;
		ext_ch = EXTCHA_ABOVE;
	} 
	else if ((pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel) && (BandWidth == BW_40))
	{
		rf_channel = pAd->CommonCfg.CentralChannel;
		rf_bw = BW_40;
		ext_ch = EXTCHA_BELOW;
	}
	else
#endif /* DOT11_N_SUPPORT */
	{
		rf_channel = pAd->CommonCfg.Channel;
		rf_bw = BW_20;
		ext_ch = EXTCHA_NONE;
	}

#ifdef DOT11_VHT_AC
	if (rf_bw == BW_40 &&
		pAd->StaActive.SupportedPhyInfo.bVhtEnable == TRUE &&
		pAd->StaActive.SupportedPhyInfo.vht_bw == VHT_BW_80) {
		rf_bw = BW_80;
		rf_channel = pAd->CommonCfg.vht_cent_ch;
	}
	DBGPRINT(RT_DEBUG_OFF, ("%s(): Input BW=%d, rf_channel=%d, vht_bw=%d, Channel=%d, vht_cent_ch=%d!\n",
				__FUNCTION__, rf_bw, rf_channel, pAd->CommonCfg.vht_bw, pAd->CommonCfg.Channel,
				pAd->CommonCfg.vht_cent_ch));
#endif /* DOT11_VHT_AC */

	rtmp_bbp_set_bw(pAd, rf_bw);
	rtmp_bbp_set_ctrlch(pAd, ext_ch);
	rtmp_mac_set_ctrlch(pAd, ext_ch);

	AsicSetChannel(pAd, rf_channel, rf_bw, ext_ch, FALSE);

	DBGPRINT(RT_DEBUG_TRACE,
				 ("%s():BW_%s, RF-Ch=%d, CtrlCh=%d, HT-CentralCh=%d\n",
				__FUNCTION__, (rf_bw == BW_80 ? "80" : (rf_bw == BW_40 ? "40": "20")),
				pAd->LatchRfRegs.Channel,
				pAd->CommonCfg.Channel,
				pAd->CommonCfg.CentralChannel));
#ifdef DOT11_VHT_AC
	DBGPRINT(RT_DEBUG_TRACE, ("VHT-CentralCh=%d\n", pAd->CommonCfg.vht_cent_ch));
#endif /* DOT11_VHT_AC */

	DBGPRINT(RT_DEBUG_TRACE, ("AdjustChannelRelatedValue ==> not any connection !!!\n"));
}
