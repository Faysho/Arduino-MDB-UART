/*
 * ExternalCmd_M.c
 *
 * Created: 26.08.2019 09:47:57
 *  Author: root
 */
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "USART_M.h"
#include "CoinChanger_M.h"
#include "BillValidator_M.h"
#include "CoinHopper_M.h"

/* interpreting recieved PC UART to be send to Peripheral
	=> 0* System commands
		0*0*+ Reset all
		0*1*+ Reset coin changer options
		0*2*+ Reset bill validator options
		0*3*+ Reset coin hopper options
	=> 1* Coinchanger
		1*1*+ Reset
		1*2*+ ID
		1*3*+ enable
		1*4*+ disable
		1*5*X*Y*+ dispense coins
			X=[1-16] coin type
			Y=[1-15] quantity
		1*6*X*+ dispanse value
			X=[1-255] number of minimum values to issue
		1*7*... Not used!
		1*8*X*Y*Z*+ config a coin (needs 'enable' afterwards to be applied)
			X=[1-16] coin type
			Y=[0,1] enable/disable accept
			Z=[0,1] enable/disable manual dispense
		1*9*X*Y*Z*+ config features
			X=[0,1] enable/disable value dispense
			Y=[0,1] enable/disable extended diagnostic
			Z=[0,1] enable/disable manual fill and payout
	=> 2* Bill Validator / Bill Acceptor / Bill Recycler commands
	=> 3* Coin Hopper / Tube Dispenser commands
			X=[1,2] select one of two possible coin hoppers
		3*X*1*+ Reset
		3*X*2*+ ID
		3*X*5*Y*Z*+ dispense coins
			Y=[1-16] coin type
			Z=[1-65535] coin quantity
		3*X*6*Y*+ dispense value
			Y=[1-65535] number of minimum values to issue
		3*X*8*Y*Z*+ enable/disable manual dispensing of certain coin type
			Y=[1-16] coin type
			Z=[0,1] enable/disable manual dispense

*/
void EXTCMD_PROCESS()
{ // receive commands from VMC
	int cnt = 0;
	uint8_t tmplen = EXT_UART_BUFFER_COUNT;
	uint8_t TMP[tmplen];
	memcpy(&TMP, &EXT_UART_BUFFER, tmplen);
	EXT_UART_BUFFER_COUNT = 0;
	EXTCMDCOMPLETE = 0;
	for (int i = 0; i < tmplen; i++)
	{
		if (TMP[i] == 0x2a)
			cnt++;
	}
	uint8_t(tmp[cnt])[6];
	int tmpcnt = 0;
	char *p = strtok(TMP, "*");
	while (p)
	{
		if ((tmpcnt < cnt) && (sizeof(p) <= 6))
			strcpy(&tmp[tmpcnt++], p);
		p = strtok(NULL, "*");
	}
	if (tmpcnt > 0)
	{
		uint16_t toplevelcmd = atoi(&tmp[0]);
		uint16_t secondlevelcmd = atoi(&tmp[1]);
		switch (toplevelcmd)
		{
		case 0:
		{
			switch (secondlevelcmd)
			{
			case 0:
				ResetAll();
				break;
			case 1:
				ResetCoinChangerOptions();
				break;
			case 2:
				ResetBVOptions();
				break;
			case 3:
				ResetCoinHoppersOptions();
				break;
			}
		}
		break;
		case 1:
			switch (secondlevelcmd)
			{
			case 1:
				MDBDeviceReset(0x08);
				break;
			case 2:
				GetCoinChangerSetupData();
				GetCoinChangerTubeStatus();
				GetCoinChangerIdentification();
				break;
			case 3:
				CoinChangerEnableAcceptCoins();
				break;
			case 4:
				CoinChangerDisableAcceptCoins();
				break;
			case 5:
			{
				if (cnt == 4)
				{
					uint8_t DispenseParams = (atoi(&tmp[3]) << 4) & 0xff;			// high 4 bits - quantity, max value = 15
					DispenseParams = DispenseParams | ((atoi(&tmp[2]) & 0x0f) - 1); // lower 4 bits - coin type, max value = 15
					CoinChangerDispense(DispenseParams);
				}
			}
			break;
			case 6:
				if (cnt == 3)
				{
					CoinChangerAlternativePayout(atoi(&tmp[2]) & 0xff);
				}
				break;
			case 7:
				// CoinChangerControlledManualFillReport();
				break;
			case 8:
				if (cnt == 5)
				{
					CoinChangerEnableCoinType(atoi(&tmp[2]), atoi(&tmp[3]), atoi(&tmp[4]));
				}
				break;
			case 9:
				if (cnt == 5)
				{
					CoinChangerConfigFeatures(atoi(&tmp[2]), atoi(&tmp[3]), atoi(&tmp[4]));
				}
				break;
			}
			break;
		case 2:
			switch (secondlevelcmd)
			{
			case 1:
				MDBDeviceReset(0x30);
				break;
			case 2:
				GetBillValidatorSetupData();
				break;
			case 3:
				BillValidatorEnableAcceptBills();
				break;
			case 4:
				BillValidatorDisableAcceptBills();
				break;
			case 5:
				if (cnt == 3)
				{
					BillValidatorEscrow(atoi(&tmp[2]) & 0x01);
				}
				break;
			case 6:
				if (cnt == 4)
				{
					BVDispenseBills(atoi(&tmp[2]) & 0xff, atoi(&tmp[3]) & 0xffff);
				}
				break;
			case 7:
				if (cnt == 3)
				{
					BVDispenseValue(atoi(&tmp[2]) & 0xffff);
				}
				break;
			case 8:
				if (cnt == 8)
				{
					BillValidatorEnableBillType(atoi(&tmp[2]), atoi(&tmp[3]), atoi(&tmp[4]), atoi(&tmp[5]), atoi(&tmp[6]), atoi(&tmp[7]));
				}
				break;
			case 9:
				if (cnt == 3)
				{
					BillValidatorConfigFeatures(atoi(&tmp[2]));
				}
				break;
			case 10:
				BillValidatorCancelPayout();
				break;
			}
			break;
		case 3:
			if (cnt >= 3)
			{
				uint8_t index = (atoi(&tmp[1]) == 1) ? 0 : 1;
				uint16_t thirdlevelcmd = atoi(&tmp[2]);
				switch (thirdlevelcmd)
				{
				case 1:
					MDBDeviceReset((index) ? 0x73 : 0x58);
					break;
				case 2:
					GetCoinHopperSetupData(index);
					GetCoinHopperIdentification(index);
					break;
				case 6:
					if (cnt == 5)
					{
						CoinHopperDispenseCoins(index, atoi(&tmp[3]) & 0xff, atoi(&tmp[4]) & 0xffff);
					}
					break;
				case 7:
					if (cnt == 4)
					{
						CoinHopperDispenseValue(index, atoi(&tmp[3]) & 0xffff);
					}
					break;
				case 8:
					if (cnt == 5)
					{
						CoinHopperEnableManualDispenseCoinType(index, atoi(&tmp[3]) & 0xff, atoi(&tmp[4]) & 0xff);
					}
					break;
				}
			}
			break;
		}
	}
}