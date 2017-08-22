/******************************************************************************/
/*    Copyright (c) 2016 MD Automotive Controls. Original Work.               */
/*    License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher   */
/******************************************************************************/
/* CONTEXT:USER_APP                                                           */
/* PACKAGE TITLE:      Coolant Temperature                                    */
/* DESCRIPTION:        This code module initialises the required ADC          */
/*                     resources and functions for coolant temperature        */
/*                     measurement                                            */
/* FILE NAME:          CTS.c                                                  */
/* REVISION HISTORY:   28-03-2016 | 1.0 | Initial revision                    */
/*                                                                            */
/******************************************************************************/
#define _CTS_C

/******************************************************************************/
/* HEADER FILES                                                               */
/******************************************************************************/
#include "build.h"

#if BUILD_USER

#include "CTS.h"


/* LOCAL VARIABLE DEFINITIONS (STATIC) ****************************************/
bool CTS_boNewSample;
SPREADAPI_ttSpreadIDX CTS_tSpreadSensorIDX;
SPREADAPI_ttSpreadIDX CTS_tSpreadEnrichmentIDX;
SPREADAPI_ttSpreadIDX CTS_tSpreadStartEnrichmentIDX;
SPREADAPI_ttSpreadIDX CTS_tSpreadPostStartEnrichmentIDX;
TABLEAPI_ttTableIDX CTS_tTableSensorIDX;
TABLEAPI_ttTableIDX CTS_tTableEnrichmentIDX;
TABLEAPI_ttTableIDX CTS_tTableStartEnrichmentIDX;
TABLEAPI_ttTableIDX CTS_tTablePostStartEnrichmentIDX;

sint32 CTS_i32Enrichment;
sint32 CTS_i32StartEnrichment;
sint32 CTS_i32PostStartEnrichment;
uint32 CTS_u32SampleCount;

/* LOCAL FUNCTION PROTOTYPES (STATIC) *****************************************/
/*******************************************************************************
* Interface        : CTS_vADCCallBack
*
* Implementation   : Callback to receive the measured ADC value
*
* Parameter
*    Par1          : enEHIOResource enum of the ADC resource
*    Par2          : u32ADCResult the ADC conversion value
*
* Return Value     : NIL
*******************************************************************************/
static void CTS_vADCCallBack(IOAPI_tenEHIOResource enEHIOResource, uint32 u32ADCResult);

/* GLOBAL FUNCTION DEFINITIONS ************************************************/
void CTS_vStart(puint32 const pu32Arg)
{
	CTS_u32ADCRaw = 0;	
	CTS_u32FuelMultiplier = 1000;
	CTS_u32SampleCount = 0;
	CTS_boNewSample = FALSE;
	CTS_i32StartEnrichment = 1000;
	CTS_i32StartEnrichmentScaled = 1000 * 1000;
	CTS_i32PostStartEnrichmentScaled = 1000 * 1000;	
	
	/* Request and initialise the CTS ADC input channel */
	SETUP_boSetupADSE(CTS_nADInput, IOAPI_enADSE, ADCAPI_en32Samples, &CTS_vADCCallBack, ADCAPI_enTrigger2, pu32Arg);
	
	/* Request and initialise required Kernel managed spread for coolant sensor */
	CTS_tSpreadSensorIDX = SETUP_tSetupSpread((void*)&CTS_tSensorVolts, (void*)&USERCAL_stRAMCAL.aUserCoolantSensorSpread, TYPE_enUInt32, 17, SPREADAPI_enSpread4ms, NULL);

	/* Request and initialise required Kernel managed table for coolant sensor */
	CTS_tTableSensorIDX = SETUP_tSetupTable((void*)&USERCAL_stRAMCAL.aUserCoolantSensorTable, (void*)&CTS_tTempCRaw, TYPE_enInt32, 17, CTS_tSpreadSensorIDX, NULL);		
	
	/* Request and initialise required Kernel managed spread for coolant enrichment */
	CTS_tSpreadEnrichmentIDX = SETUP_tSetupSpread((void*)&CTS_tTempCFiltered, (void*)&USERCAL_stRAMCAL.aUserCoolantEnrichmentSpread, TYPE_enInt32, 17, SPREADAPI_enSpread4ms, NULL);

	/* Request and initialise required Kernel managed table for coolant enrichment */
	CTS_tTableEnrichmentIDX = SETUP_tSetupTable((void*)&USERCAL_stRAMCAL.aUserCoolantEnrichmentTable, (void*)&CTS_i32Enrichment, TYPE_enInt32, 17, CTS_tSpreadEnrichmentIDX, NULL);		
	
	/* Request and initialise required Kernel managed spread for coolant starting enrichment */
	CTS_tSpreadStartEnrichmentIDX = SETUP_tSetupSpread((void*)&CTS_tTempCFiltered, (void*)&USERCAL_stRAMCAL.aUserCoolantStartEnrichmentSpread, TYPE_enInt32, 17, SPREADAPI_enSpread4ms, NULL);

	/* Request and initialise required Kernel managed table for coolant starting enrichment */
	CTS_tTableStartEnrichmentIDX = SETUP_tSetupTable((void*)&USERCAL_stRAMCAL.aUserCoolantStartEnrichmentTable, (void*)&CTS_i32StartEnrichment, TYPE_enInt32, 17, CTS_tSpreadStartEnrichmentIDX, NULL);			
	
	/* Request and initialise required Kernel managed spread for coolant post starting enrichment */
	CTS_tSpreadPostStartEnrichmentIDX = SETUP_tSetupSpread((void*)&CTS_tTempCFiltered, (void*)&USERCAL_stRAMCAL.aUserCoolantPostStartEnrichmentSpread, TYPE_enInt32, 17, SPREADAPI_enSpread4ms, NULL);

	/* Request and initialise required Kernel managed table for coolant post starting enrichment */
	CTS_tTablePostStartEnrichmentIDX = SETUP_tSetupTable((void*)&USERCAL_stRAMCAL.aUserCoolantPostStartEnrichmentTable, (void*)&CTS_i32PostStartEnrichment, TYPE_enInt32, 17, CTS_tSpreadPostStartEnrichmentIDX, NULL);		
}

void CTS_vRun(puint32 const pu32Arg)
{
	uint32 u32Temp;
	sint32 s32Temp;
	
	if (TRUE == CTS_boNewSample)
	{
		CTS_u32SampleCount++;
		
		USER_xEnterCritical();/*CR1_16*/
		(void)USERMATH_u16SinglePoleLowPassFilter16((uint16)CTS_u32ADCRaw, CTS_nADFiltVal,
			&CTS_u32ADCFiltered);
		CTS_boNewSample = FALSE;		
		USER_xExitCritical();/*CR1_16*/		
		
		u32Temp = CTS_u32ADCFiltered * SENSORS_nADRefVolts;
		u32Temp /= SENSORS_nADScaleMax;
		u32Temp /= SENSORS_nVDivRatio;		
		CTS_tSensorVolts = u32Temp;		
		
		/* Calculate the current spread for coolant sensor */
		USER_vSVC(SYSAPI_enCalculateSpread, (void*)&CTS_tSpreadSensorIDX,
				NULL, NULL);		
		
		/* Lookup the current spread for coolant sensor */
		USER_vSVC(SYSAPI_enCalculateTable, (void*)&CTS_tTableSensorIDX,
			NULL, NULL);			
		
		/* Calculate the current spread for coolant enrichment */
		USER_vSVC(SYSAPI_enCalculateSpread, (void*)&CTS_tSpreadEnrichmentIDX,
				NULL, NULL);		
		
		/* Lookup the current spread for coolant enrichment */
		USER_vSVC(SYSAPI_enCalculateTable, (void*)&CTS_tTableEnrichmentIDX,
			NULL, NULL);			
			
		CTS_tTempCFiltered = CTS_tTempCRaw;					
		
		/* Decrease the enrichment linearly at higher rpm */
		if (1500 < CAM_u32RPMRaw)
		{
			u32Temp = 7500 - CAM_u32RPMRaw;
		}
		else
		{
			u32Temp = 6000;
		}
		
		s32Temp = CTS_i32Enrichment - 1000;
		s32Temp *= u32Temp;
		s32Temp /= 6000;
		s32Temp += 1000;
		s32Temp *= (CTS_i32StartEnrichmentScaled / 1000);
		s32Temp /= 1000;
		s32Temp *= (CTS_i32PostStartEnrichmentScaled / 1000);
		s32Temp /= 1000;		
		CTS_u32FuelMultiplier = (uint32)s32Temp;
		
		if ((50 < CTS_u32SampleCount) && (0 == CAM_u32RPMRaw))
		{
			/* Calculate the current spread for starting coolant enrichment */
			USER_vSVC(SYSAPI_enCalculateSpread, (void*)&CTS_tSpreadStartEnrichmentIDX,
					NULL, NULL);		
			
			/* Lookup the current spread for starting coolant enrichment */
			USER_vSVC(SYSAPI_enCalculateTable, (void*)&CTS_tTableStartEnrichmentIDX,
				NULL, NULL);	
					
			/* Calculate the current spread for post starting coolant enrichment */
			USER_vSVC(SYSAPI_enCalculateSpread, (void*)&CTS_tSpreadPostStartEnrichmentIDX,
					NULL, NULL);		
			
			/* Lookup the current spread for post starting coolant enrichment */
			USER_vSVC(SYSAPI_enCalculateTable, (void*)&CTS_tTablePostStartEnrichmentIDX,
				NULL, NULL);	
			
			CTS_i32StartEnrichmentScaled = 1000	* CTS_i32StartEnrichment;		
			CTS_i32PostStartEnrichmentScaled = 1000	* CTS_i32PostStartEnrichment;					
		}
	}
}

void CTS_vTerminate(puint32 const pu32Arg)
{

}

void CTS_vCallBack(puint32 const pu32Arg)
{

}

static void CTS_vADCCallBack(IOAPI_tenEHIOResource enEHIOResource, uint32 u32ADCResult)
{
	CTS_u32ADCRaw = u32ADCResult;
	CTS_boNewSample = TRUE;
}

#endif //BUILD_USER