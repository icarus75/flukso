//
// ctrl.c : AVR uC code for ctrl buffer initialisation and put/get ops
//
// Copyright (c) 2010 flukso.net
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

#include <avr/eeprom.h>

#include "global.h"
#include "main.h"
#include "buffer.h"
#include "ctrl.h"
#include "encode.h"

cBuffer ctrlRxBuffer; // ctrl receive buffer
cBuffer ctrlTxBuffer; // ctrl transmit buffer

static char ctrlRxData[CTRL_RX_BUFFER_SIZE];
static char ctrlTxData[CTRL_TX_BUFFER_SIZE];

extern volatile struct event_struct EEMEM EEPROM_event;
extern volatile struct event_struct event;

extern uint8_t EEMEM EEPROM_phy_to_log[MAX_SENSORS];
extern uint8_t phy_to_log[MAX_SENSORS];

extern volatile struct sensor_struct EEMEM EEPROM_sensor[MAX_SENSORS];
extern volatile struct sensor_struct sensor[MAX_SENSORS];

void ctrlInit(void)
{
	// initialize the CTRL receive buffer
	bufferInit(&ctrlRxBuffer, (u08*) ctrlRxData, CTRL_RX_BUFFER_SIZE);
	// initialize the CTRL transmit buffer
	bufferInit(&ctrlTxBuffer, (u08*) ctrlTxData, CTRL_TX_BUFFER_SIZE);
}

uint8_t ctrlTxBufferIsEmpty(void)
{
	if(ctrlTxBuffer.datalength == 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlAddToTxBuffer(uint8_t data)
{
	return bufferAddToEnd(&ctrlTxBuffer, data);
}

uint8_t ctrlGetFromTxBuffer(uint8_t* data) {
	// make sure we have data in the Tx buffer
	if(ctrlTxBuffer.datalength) {
		// get byte from beginning of buffer
		*data = bufferGetFromFront(&ctrlTxBuffer);
		return TRUE;
	}
	else {
		// no data
		return FALSE;
	}
}

uint8_t ctrlRxBufferIsEmpty(void)
{
	if(ctrlRxBuffer.datalength == 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlAddToRxBuffer(uint8_t data)
{
	return bufferAddToEnd(&ctrlRxBuffer, data);
}

uint8_t ctrlGetFromRxBuffer(uint8_t* pdata)
{
	// make sure we have data in the Rx buffer
	if(ctrlRxBuffer.datalength) {
		// get byte from beginning of buffer
		*pdata = bufferGetFromFront(&ctrlRxBuffer);
		return TRUE;
	}
	else {
		// no data
		return FALSE;
	}
}

void ctrlFlushReceiveBuffer(void)
{
	ctrlRxBuffer.datalength = 0;
}

uint8_t ctrlReadCharFromRxBuffer(uint8_t* pdata)
{
	uint8_t high_hex, low_hex;

	if (ctrlGetFromRxBuffer(&high_hex) && ctrlGetFromRxBuffer(&low_hex)) {
		*pdata = htob(((uint16_t)high_hex << 8) + low_hex);
		return TRUE;
	}
	else {
		return FALSE;
	}	

}

uint8_t ctrlReadShortFromRxBuffer(uint16_t* pdata) 
{
	uint8_t high_char, low_char;

	if(ctrlReadCharFromRxBuffer(&high_char) && ctrlReadCharFromRxBuffer(&low_char)) {
		*pdata = ((uint16_t)high_char << 8) + low_char;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlReadLongFromRxBuffer(uint32_t* pdata) 
{
	uint16_t high_short, low_short;

	if(ctrlReadShortFromRxBuffer(&high_short) && ctrlReadShortFromRxBuffer(&low_short)) {
		*pdata = ((uint32_t)high_short << 16) + low_short;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlWriteCharToTxBuffer(uint8_t data)
{
	uint16_t hex;

	hex = btoh(data);
	if (ctrlAddToTxBuffer((uint8_t)(hex >> 8)) && ctrlAddToTxBuffer((uint8_t)hex)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlWriteShortToTxBuffer(uint16_t data)
{
	if (ctrlWriteCharToTxBuffer((uint8_t)(data >> 8)) && ctrlWriteCharToTxBuffer((uint8_t)data)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

uint8_t ctrlWriteLongToTxBuffer(uint32_t data)
{
	if (ctrlWriteShortToTxBuffer((uint16_t)(data >> 16)) && ctrlWriteShortToTxBuffer((uint16_t)data)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void ctrlRxToTxLoop(void)
{
	uint8_t data;

	while (ctrlGetFromRxBuffer(&data)) {
		ctrlAddToTxBuffer(data);
	}
}

void ctrlDecode(void)
{
	uint8_t cmd[2];

	if (ctrlGetFromRxBuffer(cmd) && ctrlGetFromRxBuffer(cmd+1)) {
		ctrlAddToTxBuffer(cmd[0]);
		ctrlAddToTxBuffer(cmd[1]);

		switch (cmd[0]) {
		case 'g':
			ctrlCmdGet(cmd[1]);
			break;
		case 's':
			ctrlCmdSet(cmd[1]);
			break;
		case 'c':
			if (cmd[1] == 't') ctrlCmdCommit();
			break;
		}

		ctrlAddToTxBuffer('.');
	}

	ctrlFlushReceiveBuffer();
}

void ctrlCmdGet(uint8_t cmd)
{
	uint8_t i;

	switch (cmd) {
	case 'p':
		for (i = 0 ; i < MAX_SENSORS; i++) {
			ctrlWriteCharToTxBuffer(phy_to_log[i]);
		}
		break;

	case 'v':
		ctrlReadCharFromRxBuffer(&i);
		ctrlWriteLongToTxBuffer(sensor[i].value);
		break;
	}
}

void ctrlCmdSet(uint8_t cmd)
{
	uint8_t i;

	switch (cmd) {
	case 'p':
		for (i = 0 ; i < MAX_SENSORS; i++) {
			ctrlReadCharFromRxBuffer(&phy_to_log[i]);
		}
		break;
	case 'v':
		ctrlReadCharFromRxBuffer(&i);
		ctrlReadLongFromRxBuffer((uint32_t *)&sensor[i].value);
		break;
	}
}

void ctrlCmdCommit(void)
{
	cli();
	eeprom_write_block((const void*)&event, (void*)&EEPROM_event, sizeof(event));
	eeprom_write_block((const void*)&phy_to_log, (void*)&EEPROM_phy_to_log, sizeof(phy_to_log));
	eeprom_write_block((const void*)&sensor, (void*)&EEPROM_sensor, sizeof(sensor));
	sei();
}
