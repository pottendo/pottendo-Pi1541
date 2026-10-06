// Pi1541 - A Commodore 1541 disk drive emulator
// Copyright(C) 2018 Stephen White
//
// This file is part of Pi1541.
// 
// Pi1541 is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Pi1541 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Pi1541. If not, see <http://www.gnu.org/licenses/>.

#include "m8520.h"
#include "iec_bus.h"
#include "Pi1581.h"

// The 8520 contains a programmable baud rate generator which is used for fast serial transfers. 
// Timer A is used for the baud rate generator. In the output mode data is shifted out on SP at 1/2 the underflow rate of Timer A.
// The maximum baud rate possible is phi 2 divided by 4, but the maximum usable baud rate will be determined by line loading and the speed at which the receiver responds to the input data.
// Transmission will start following a write to the Serial Data Register (provided Timer A is running and in continuous mode).
// The clock derived from Timer A appears on the CNT pin.
// The Data in the Serial Data Register will be loaded into the shift register then shifted out to the SP pin. 
// After 8 pulses on the CNT pin, a bit in the ICR (interrupt control register) is set and if desired, an interrupt may be generated.
// All incoming fast bytes generate an interrupt within the Fast Serial Drive. 
// Bytes are shifted out; most significant bit first.

// The serial port is a buffered, 8-bit synchronous shift register system.
// A control bit selects input or output mode.
// In input mode, data on the SP pin is shifted into the shift register on the rising edge of the signal applied to the CNT pin.
// After 8 CNT pulses, the data in the shift register is dumped into the Serial Data Register and an interrupt is generated.
// In the output mode, TIMER A is used for the baud rate generator.
// Data is shifted out on the SP pin at 1/2 the underflow rate of TIMER A.
// The maximum baud rate possible is (212 divided by 4, but the maximum useable baud rate will be determined byline loading and the speed at which the receiver responds to input data.
// Transmission will start following a write to the Serial Data Register (provided TIMER A is running and in continuous mode).
// The clock signal derived from TIMER A appears as an output on the CNT pin.
// The data in the Serial Data Register will be loaded into the shift register then shift out to the SP pin when a CNT pulse occurs.
// Datashifted out becomes valid on the falling edge of CNT and remains valid until the next falling edge.
// After 8 CNT pulses, an interrupt is generated to indicate more data can be sent.
// If the Serial Data Register was loaded with new information prior to this interrupt, the new data will automatically be loaded into the shift register and transmission will continue.
// If the microprocessor stays one byte ahead of the shift register, transmission will be continuous.
// If no further data is to be transmitted, after the 8th CNT pulse, CNT will return high and SP will remain at the level of the last data bit transmitted.
// SDR data is shifted out MSB first and serial input data should also appear in this format.
// The bidirectional capability of the Serial Port and CNT clock allows many 6526 devices to be connected to a common serial communication bus on which one 6526 acts as a master,
// sourcing data and shift clock, while all other 6526 chips act as slaves.
// Both CNT and SP outputs are open drain to allow such a common bus.
// Protocol for master / slave selection can be transmitted over the serial bus, or via dedicated handshaking lines.

// Reset
//		sdr_valid = 0
//		sr_bits = 0

// SR write
//		SR = value
//		if in output mode
//			sdr_valid = 1
//	SR Read
//		value = SR
//
// Update
//		if TA times out
//			if in ouput
//				if sr_bits
//					sr_bits--
//					if sr_bits == 0
//						flag IRQ
//						SR = shifter
//					endif
//				endif
//				if sr_bits == 0 && sdr_valid
//					shifter = SR
//					sdr_valid = 0
//					sr_bits = 14
//				endif
//			endif


// Get it to burst again but clear the memory each time
//	- see if it is receiving anything
// Trace out
//	- outputs again
//		- check the time it takes
//	- include timer
// When it loads for the first time
//	- check what the file name is
//		- why does it not send anything and stop the task?
// 2Mhz timings
//	- need it?
//		- if not update the timers twice
// Try without timerAReloaded


// A control bit allows the timer output to appear on a PORT B output line(PB6 for TIMER A and PB7 for TIMER B).
// This function overrides the DDRB control bit and forces the appropriate PB line to an output.

extern u16 pc;
extern bool bLoggingCYCs;

u8 lastShiftedValue = 0;

m8520::m8520()
{
	Reset();
}

void m8520::Reset()
{
	// The port pins are set as inputs and port registers to zero(although a read of the ports will return all highs because of passive pullups).
	portA.SetDirection(0);
	portB.SetDirection(0);

	//PCAsserted = 0;

	FLAGPin = true;	// external devices should be setting this
	CNTPin = true;	// external devices should be setting this
	CNTPinOld = false;
	SPPin = false;	// external devices should be setting this
	SPPinLastOut = false;
	TODPin = false;	// external devices should be setting this

	//CRARegister = 0;
	//CRBRegister = 0;
	Write(CRA, 0);
	Write(CRB, 0);
	
	// The timer control registers are set to zero and the timer latches to all ones.
	timerACounter = 0;
	timerALatch = 0xffff;
	timerAOutputOnPB6 = false;
	//timerAToggle = false;
	timerAMode = TA_MODE_PHI2;
	//timerA50Hz = false;
	//ta_pb6 = true;
	timerAReloaded = false;

	timerBCounter = 0;
	timerBLatch = 0xffff;
	//timerBOutputOnPB7 = false;	// replaced with (CRBRegister & CRB_PBON)
	//timerBToggle = false;
	timerBMode = TB_MODE_PHI2;
	//tb_pb7 = true;
	timerBReloaded = false;

	serialDataRegister = 0;
	serialShiftRegister = 0;
	serialDataRegisterLoaded = false;
	//serialBitsShiftedSoFar = 8;
	serialBitsShiftedSoFar = 0;
	serialOutputShiftStarted = false;

	TODActive = false;
	TODAlarm = 0;
	TODClock = 0;
	TODLatch = 0;

	ICRMask = 0;
	ICRData = 0;
	//OutputIRQ();

	serialSetToInputCoolDown = 0;
}

extern u16 pc;

void m8520::ShiftOut()
{
	if (SerialOutput())
	{
		//The individual data bits now appear at half the timeout rate of timer A on the SP line and the clock signal from timer A 
		// appears on the CNT line(it changes value on each timeout so that the next bit appears on the SP line on each negative transition[high to low]).
		// The transfer begins with the MSB of the data byte.Once all eight bits have been output, CNT remains high and the SP line retains the value of the last bit sent
		// in addition, the SP bit in the interrupt control register is set to show that the shift register can be supplied with new data.
		//DEBUG_LOG("o %d\r\n", serialBitsShiftedSoFar);

		// Output transmission will start following a write to the Serial Data Register(provided TIMER A is running and in continuous mode).
		// Will use serialBitsShiftedSoFar == 8 to indicate that this is yet to occur
		if (serialBitsShiftedSoFar >= 8)
		{
			// If no further data is to be transmitted, after the 8th CNT pulse, CNT will return high and SP will remain at the level of the last data bit transmitted.
			CNTPin = true;
		}
		else
		{
			// Data is shifted out on the SP pin at 1 / 2 the underflow rate of TIMER A.
			// (provided TIMER A is running and in continuous mode)

			//bool oldCNT = CNTPin;
			// The clock signal derived from TIMER A appears as an output on the CNT pin.
			CNTPin = !CNTPin;

			//DEBUG_LOG("CNTPin%d\r\n", CNTPin);
			// Datashifted out becomes valid on the falling edge of CNT and remains valid until the next falling edge.
			if (!CNTPin)
			{
				serialOutputShiftStarted = true;

				// SDR data is shifted out MSB first and serial input data should also appear in this format.
				SPPin = (serialShiftRegister & 0x80) != 0;
				SPPinLastOut = SPPin;
				serialShiftRegister <<= 1;

				//DEBUG_LOG("o%d %02x\r\n", serialBitsShiftedSoFar, serialShiftRegister);

				//serialBitsShiftedSoFar++;

				//if (serialBitsShiftedSoFar >= 8)
				//{
				//	// After 8 CNT pulses an interrupt is generated to indicate more data can be sent.
				//	// If the Serial Data Register was loaded with new information prior to this interrupt.
				//	// The new data will automatically be loaded into the shift register and transmission will be continuous.
				//	if (serialDataRegisterLoaded)
				//	{
				//		serialShiftRegister = serialDataRegister;
				//		serialDataRegisterLoaded = false;
				//		serialBitsShiftedSoFar = 0;
				//	}

				//	//DEBUG_LOG("o %04x\r\n", pc);
				//	//DEBUG_LOG("Q\r\n");
				//	SetInterrupt(IR_SDR);
				//}
			}
			else if (serialOutputShiftStarted)
			{
				serialBitsShiftedSoFar++;

				if (serialBitsShiftedSoFar >= 8)
				{
					//DEBUG_LOG("o%02x\r\n", lastShiftedValue);
					// After 8 CNT pulses an interrupt is generated to indicate more data can be sent.
					// If the Serial Data Register was loaded with new information prior to this interrupt.
					// The new data will automatically be loaded into the shift register and transmission will be continuous.
					if (serialDataRegisterLoaded)
					{
						serialShiftRegister = serialDataRegister;
						lastShiftedValue = serialDataRegister;
						serialDataRegisterLoaded = false;
						serialBitsShiftedSoFar = 0;
					}
					else
					{
						serialOutputShiftStarted = false;
						// Is this true? I don't think so. (The 3 places 1581 ROM writes to 400c it always only sends 1 byte then polls)
						//  // Commodore_128_Internals;- 
						//	// If the SDR is loaded with new data before this event, 
						//	// these are automatically loaded into the shift register
						//	// and shifted out No interrupt is generated in this case.
					}

					{
						SetInterrupt(IR_SDR);
					}
				}

			}
		}
	}
}

// Update for a single cycle
void m8520::Execute()
{
	bool timerATimedOut = false;
	bool timerBTimedOut = false;
	// In oneshot mode, the timer will count down from the latched value to zero, generate an interrupt, reload the latched value, then stop.
	// In continuous mode, the timer will count from the latched value to zero, generate an interrupt, reload the latched value and repeat the procedure continuously.

	// The timer latch is loaded into the timer on any timer underflow
	if (TimerAActive() && !timerAReloaded)
	{
		switch (timerAMode)
		{
			case m8520::TA_MODE_PHI2:
				timerATimedOut = timerACounter == 0;
				timerACounter--;
			break;
			case m8520::TA_MODE_CNT_PVE:
				// Test probably not being used
				//if (SerialOutput())
				if (SerialInput())	// it must only be when input as if it was output then timerA will be driving the CNT waveform.
				{
					if (CNTPin && !CNTPinOld)
					{
						timerATimedOut = timerACounter == 0;
						timerACounter--;	// counts positive CNT transitions.
					}
				}
			break;
		}

		// Flag the IRQ when hit 0 but reload from latch when -1
		if (timerACounter == 0)
		{
			SetInterrupt(IR_TA);

			// try it here
			//if (!TimerAOneShot())
				// ShiftOut();
		}

		if (timerATimedOut)
		{
			//SetInterrupt(IR_TA);

			ReloadTimerA();

			if (TimerAOneShot())
			{
				CRARegister &= ~CRA_START;
			}
			else 
			{
				ShiftOut();
			}

			//ta_pb6 = !ta_pb6;
			//if (timerAOutputOnPB6)
			//{
			//	// This function overrides the DDRB control bit and forces the appropriate PB line to an output.
			//	unsigned char ddr = portB.GetDirection();
			//	if (ddr & 0x80)
			//	{
			//		// the signal on PB6 is inverted each time the counter reaches zero
			//		if (!ta_pb6) portB.SetOutput(portB.GetOutput() & (~0x40));
			//		else portB.SetOutput(portB.GetOutput() | 0x40);
			//	}
			//}
		}
	}

	//timerBTimedOut = timerBCounter == 0;

	if (TimerBActive() && !timerBReloaded)
	{
		//DEBUG_LOG("TB %04x\r\n", timerBCounter);

		switch (timerBMode)
		{
			case m8520::TB_MODE_PHI2:
				timerBTimedOut = timerBCounter == 0;
				timerBCounter--;
			break;
			case m8520::TB_MODE_CNT_PVE:
				//if (SerialInput())	// it must only be when input as if it was output then timerA will be driving the CNT waveform.
				{
					if (CNTPin && !CNTPinOld)
					{
						timerBTimedOut = timerBCounter == 0;
						timerBCounter--;	// counts positive CNT transitions.
					}
				}
			break;
			case m8520::TB_MODE_TA_UNDEFLOW:
				if (timerATimedOut)
				{
					timerBTimedOut = timerBCounter == 0;
					timerBCounter--;
				}
			break;
			case m8520::TB_MODE_TA_UNDEFLOW_CNT_PVE:
				if (SerialOutput())
				{
					if (timerATimedOut && CNTPin)
					{
						timerBTimedOut = timerBCounter == 0;
						timerBCounter--;
					}
				}
			break;
		}

		// Flag the IRQ when hit 0 but reload from latch when -1
		if (timerBCounter == 0)
			SetInterrupt(IR_TB);

		if (timerBTimedOut)
		{
			//DEBUG_LOG("TB out\r\n");
			//SetInterrupt(IR_TB);

			ReloadTimerB();

			if (TimerBOneShot())
			{
				CRBRegister &= ~CRB_START;
			}

			//tb_pb7 = !tb_pb7;
			//if (CRBRegister & CRB_PBON)	//timerBOutputOnPB7)
			//{
			//	// This function overrides the DDRB control bit and forces the appropriate PB line to an output.
			//	unsigned char ddr = portB.GetDirection();
			//	if (ddr & 0x80)
			//	{
			//		// the signal on PB7 is inverted each time the counter reaches zero
			//		if (!tb_pb7) portB.SetOutput(portB.GetOutput() & (~0x80));
			//		else portB.SetOutput(portB.GetOutput() | 0x80);
			//	}
			//}
		}
	}

	//if (PCAsserted)
	//	PCAsserted--;

	CNTPinOld = CNTPin;
	timerAReloaded = false;
	timerBReloaded = false;
}

void m8520::SetPinFLAG(bool value)	// Active low
{
	if (FLAGPin && !value)
	{
		// Any negative transition on FLAG will set the FLAG interrupt bit.
		SetInterrupt(IR_FLG);
		//DEBUG_LOG("IR_FLG\r\n");
	}
	FLAGPin = value;
}

#define BUFSIZE 7
static unsigned char buffer[BUFSIZE];
static int bufindex = 0;

void m8520::SetPinCNT(bool value)
{
	if (serialSetToInputCoolDown == 0)
//	if (SerialInput())
	{
		if (!CNTPin && value)	// rising edge?
		{
			//if (serialBitsShiftedSoFar < 8)
			{
				// In input mode, data on the SP pin is shifted into the shift register on the rising edge of the signal applied to the CNT pin.
				serialShiftRegister <<= 1;
				serialShiftRegister |= SPPin;

				serialBitsShiftedSoFar++;
				//DEBUG_LOG("%d\r\n", serialBitsShiftedSoFar);

				if (serialBitsShiftedSoFar >= 8)
				{
					// After 8 CNT pulses, the data in the shift register is dumped into the Serial Data Register and an interrupt is generated.
					// here
					//DEBUG_LOG("i=%02x\r\n", serialShiftRegister);

					//if (bufindex < BUFSIZE)
					//{
					//	buffer[bufindex] = serialShiftRegister;
					//	//DEBUG_LOG("%02x %d\r\n", serialShiftRegister, bufindex);
					//}
					//else
					//{
					//	if (bufindex == BUFSIZE)
					//	{
					//		for (int i = 0; i < BUFSIZE; ++i)
					//		{
					//			DEBUG_LOG("%02x\r\n", buffer[i]);
					//		}
					//	}
					//	else
					//	{
					//		DEBUG_LOG("%02x\r\n", serialShiftRegister);
					//	}
					//}
					//bufindex++;

					serialDataRegister = serialShiftRegister;
					serialBitsShiftedSoFar = 0;
					//serialShiftRegister = 0;	// do we need to do this?
					SetInterrupt(IR_SDR);
					//bLoggingCYCs = true;
				}
			}
		}
		CNTPin = value;
	}
	else
	{
		serialSetToInputCoolDown--;
		//DEBUG_LOG("serialSetToInputCoolDown = %d\r\n", serialSetToInputCoolDown);

	}
}

void m8520::SetPinTOD(bool value)
{
	// Posistive edge transitions on this pin cause the binary counter to increment.
	if (value && !TODPin && TODActive)
	{
		TODClock++;
		TODClock &= 0xffffff;
		if (TODClock == TODAlarm)
		{
			SetInterrupt(IR_TOD);
		}
	}
	TODPin = value;
}

unsigned char m8520::Read(unsigned int address)
{
	unsigned char value = 0;

	switch (address & 0xf)
	{
		case ORA:
			value = ReadPortA();
		break;
		case ORB:
			value = ReadPortB();
			// The 8520 datasheet contradicts itself;-
			// PC will go low for one cycle following a read or write of PORT B.
			// PC will go low on the 3rd cycle after a PORT B access.
			//PCAsserted = 3;
		break;
		case DDRA:
			value = portA.GetDirection();
			break;
		case DDRB:
			value = portB.GetDirection();
		break;

		// Data read from the timer are the present contents of the Timer Counter.
		case TALO:
			value = timerACounter & 0xff;
		break;
		case TAHI:
			value = timerACounter >> 8;
		break;
		case TBLO:
			value = timerACounter & 0xff;
		break;
		case TBHI:
			value = timerACounter >> 8;
		break;

		// Since a carry from one stage to the next can occur at any time with respect to a	read operation, a latching function is included to keep all Time of Day information constant during a read sequence.
		// All TOD registers latch on a read of MSB event and remain latched until after a read of LSB Event.
		// The TOD clock continues to count when the output registers are latched.
		// If only one register is to be read, there is no carry problem and the register can be read �on the fly", provided that any read of MSB Event is followed by a read of LSB Event to disable the latching.
		case EVENT_LSB:
			value = (unsigned char)(TODLatch);
		break;
		case EVENT_8_15:
			value = (unsigned char)(TODLatch >> 8);
		break;
		case EVENT_MSB:
			TODLatch = TODClock;
			value = (unsigned char)(TODLatch >> 16);
		break;


		case NC:
		break;
		case SDR:
			value = serialDataRegister;
			//DEBUG_LOG("rsr%02x\r\n", value);
			//serialBitsShiftedSoFar = 0;
		break;
		case ICR:
			// The interrupt DATA register is cleared and the IRQ line returns high following a read of the DATA register.
			value = ICRData & (IR_SET | IR_FLG | IR_SDR | IR_TOD | IR_TB | IR_TA);
			ClearInterrupt(ICRData & (IR_FLG | IR_SDR | IR_TOD | IR_TB | IR_TA));
			ICRData = 0;
		break;
		case CRA:
			value = CRARegister;
		break;
		case CRB:
			value = CRBRegister;
		break;
	}
	return value;
}

unsigned char m8520::Peek(unsigned int address)
{
	unsigned char value = 0;

	switch (address & 0xf)
	{
		case ORA:
			value = PeekPortA();
		break;
		case ORB:
			value = PeekPortB();
		break;
		case DDRA:
			value = portA.GetDirection();
		break;
		case DDRB:
			value = portB.GetDirection();
		break;
		case TALO:
			value = timerACounter & 0xff;
		break;
		case TAHI:
			value = timerACounter >> 8;
		break;
		case TBLO:
			value = timerACounter & 0xff;
		break;
		case TBHI:
			value = timerACounter >> 8;
		break;
		case EVENT_LSB:
			value = (unsigned char)(TODLatch);
		break;
		case EVENT_8_15:
			value = (unsigned char)(TODLatch >> 8);
		break;
		case EVENT_MSB:
			TODLatch = TODClock;
			value = (unsigned char)(TODLatch >> 16);
		break;
		case NC:
			break;
		case SDR:
			value = serialDataRegister;
			break;
		case ICR:
			value = ICRData;
			break;
		case CRA:
			// bit 4 will always read back a zero and writing a zero has no effect

			// CRA_LOAD bit is write only; it always returns a 0 when read.

			value = CRARegister & (~CRA_LOAD);
			break;
		case CRB:
			// CRA_LOAD bit is write only; it always returns a 0 when read.

			value = CRBRegister & (~CRB_LOAD);
			break;
	}
	return value;
}

void m8520::Write(unsigned int address, unsigned char value)
{
	unsigned char ddr;

	switch (address & 0xf)
	{
		case ORA:
			WritePortA(value);
		break;
		case ORB:
			WritePortB(value);
			// The 8520 datasheet contradicts itself;-
			// PC will go low forone cycle following a read orwrite of PORT B.
			// PC will go low on the 3rd cycle after a PORT B access.
			//PCAsserted = 3;
			break;
		case DDRA:
			portA.SetDirection(value);
		break;
		case DDRB:
			portB.SetDirection(value);
		break;

		// Data written to the timer are latched in the Timer Latch.
		case TALO:
			timerALatch = (timerALatch & 0xff00) | value;
			break;
		case TAHI:
			timerALatch = (timerALatch & 0xff) | (value << 8);
			// In oneshot mode; a write to Timer High will transfer the timer latch to the counter and initiate counting regardless of the start bit.

			// The timer latch is loaded into the timer following a write to the high byte of the prescaler while the timer is stopped.

			// The timer latch is loaded into the timer on any timer underflow, on a force load or following a write to the high byte of the prescaler while the timer is stopped.
			// If the timer is running, a write to the high byte will load the timer latch, but not reload the counter.

			if (!TimerAActive())
				ReloadTimerA();
			break;
		case TBLO:
			timerBLatch = (timerBLatch & 0xff00) | value;
			break;
		case TBHI:
			timerBLatch = (timerBLatch & 0xff) | (value << 8);
			// In oneshot mode; a write to Timer High will transfer the timer latch to the counter and initiate counting regardless of the start bit.

			// The timer latch is loaded into the timer following a write to the high byte of the prescaler while the timer is stopped.
			if (!TimerBActive())
				ReloadTimerB();
			break;


		// TOD is automatically stopped whenever a write to the regiser occurs.
		case EVENT_LSB:
			if (TimerBAlarm())
			{
				TODAlarm = (TODAlarm & 0xffff00) | value;
			}
			else
			{
				TODActive = true;	// The clock will not start again until after a write to the LSB Event Register.
				TODClock = (TODClock & 0xffff00) | value;
			}
			break;
		case EVENT_8_15:
			if (TimerBAlarm())
			{
				TODAlarm = (TODAlarm & 0xff00ff) | ((unsigned)value << 8);
			}
			else
			{
				TODActive = false;
				TODClock = (TODClock & 0xff00ff) | ((unsigned)value << 8);
			}
			break;
		case EVENT_MSB:
			if (TimerBAlarm())
			{
				TODAlarm = (TODAlarm & 0xffff) | ((unsigned)value << 16);
			}
			else
			{
				TODActive = false;
				TODClock = (TODClock & 0xffff) | ((unsigned)value << 16);
			}
			break;

		case NC:
			break;
		case SDR:
			// After 8 CNT pulses an interrupt is generated to indicate more data can be sent.
			// If the Serial Data Register was loaded with new information prior to this interrupt.
			// The new data will automatically be loaded into the shift register and transmission will be continuous.
			//DEBUG_LOG("wsr%02x %04x\r\n", value, pc);
			serialDataRegister = value;

			// Output transmission will start following a write to the Serial Data Register(provided TIMER A is running and in continuous mode).
			//if ((CRARegister & (CRA_SPMODE | CRA_START)) == (CRA_SPMODE | CRA_START))
			if (SerialOutput() && TimerAActive())
			{
				if (serialBitsShiftedSoFar >= 8)
				{
					serialShiftRegister = serialDataRegister;
					serialBitsShiftedSoFar = 0;	// Now when the timer times out the shift will commence
					serialDataRegisterLoaded = false;	// Only one byte so far ie the one just copied into serialShiftRegister
					lastShiftedValue = value;
					//DEBUG_LOG("%02x %04x\r\n", value, pc);
					serialOutputShiftStarted = false;
				}
				else
				{
					serialDataRegisterLoaded = true; // Flag that the next byte is queued up inside serialDataRegister
				}
			}

			// try Remove this entire if
			//if (SerialInput())
			//{
			//	serialBitsShiftedSoFar = 8;	// try remove this (does this even occur? Input and a write to SDR?)
			//	DEBUG_LOG("Wrote to SDR when in input mode!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
			//	serialDataRegisterLoaded = false; // need this?
			//}

			break;
		case ICR:
			// The MASK register provides convenient control of Individual mask bits. When writing to the MASK register,
			// if bit 7 (SET / CLEAR) of the data written is a ZERO, any mask bit written with a one will be cleared, while
			// those mask bits written with a zero will be unaffected. If bit 7 of the data written is a ONE, any mask bit written
			// with a one will be set, while those mask bits written with a zero will be unaffected.
			// In order for an interrupt flag to set IR and generate an Interrupt Request, the corresponding MASK bit must be set.
			if ((value & IR_SET) == 0)
				ICRMask &= ~(value & (IR_FLG | IR_SDR | IR_TOD | IR_TB | IR_TA));
			else
				ICRMask |= (value & (IR_FLG | IR_SDR | IR_TOD | IR_TB | IR_TA));

			OutputIRQ();
			break;
		case CRA:
		{
			unsigned char CRARegisterOld = CRARegister;

			CRARegister = value;

			if (CRARegister & CRA_PBON)
			{
				// Timer A output appears on PB6

				// Writing a 1 here enables PB6 output.
				// When PB6 is selected for timer A output, the line automatically becomes an output, regardless of the setting of the data direction register bit for that line.

				timerAOutputOnPB6 = true;
			}
			else
			{
				// PB6 normal operation

				// Writing a 0 here disables timer A output and restores PB6 to the state and function defined in the port B data and data direction registers.
				timerAOutputOnPB6 = false;
			}

			//if (CRARegister & CRA_OUTPUTMODE)
			//{
			//	// Toggle
			//	timerAToggle = true;

			//	// The toggle output is set high whenever the timer is started and is set low by RES
			//}
			//else
			//{
			//	// Pulse
			//	timerAToggle = false;
			//}

			// bit 4 will always read back a zero and writing a zero has no effect
			if (CRARegister & CRA_LOAD)
			{
				// Force load
				// A strobe bit allows the timer latch to be loaded into the timer counter at any time, whether the timer is running or not.

				//Writing a 1 to this bit, called the force load strobe, causes the contents of the timer A latch to be transferred to the counter, regardless of whether the timer is currently running or stopped.

				ReloadTimerA();
				CRARegister &= ~CRA_LOAD;
			}

			if (CRARegister & CRA_INMODE)
			{
				// Timer A counts positive CNT transitions
				timerAMode = TA_MODE_CNT_PVE;
				// Test probably not being used
				DEBUG_LOG("TA_MODE_CNT_PVE\r\n");
			}
			else
			{
				// A counts phi2
				timerAMode = TA_MODE_PHI2;
			}

			// 81D5

			//if ((CRARegisterOld ^ CRARegister) & CRA_SPMODE)
			if ((CRARegisterOld & CRA_SPMODE) ^ (CRARegister & CRA_SPMODE))
			{
				//DEBUG_LOG("%02x %02x %04x\r\n", (CRARegisterOld & CRA_SPMODE), (CRARegister & CRA_SPMODE), pc);
				//if (!(CRARegisterOld & CRA_SPMODE) && (CRARegister & CRA_SPMODE))
				if (SerialOutput())
				{
					// Output transmission will start following a write to the Serial Data Register(provided TIMER A is running and in continuous mode).
					// Will use serialBitsShiftedSoFar == 8 to indicate that this is yet to occur
					serialBitsShiftedSoFar = 8;
					//serialShiftRegister = serialDataRegister;
					serialDataRegisterLoaded = false;

					// What becomes of the SPPin when we switch from input to output?
					SPPin = SPPinLastOut;

					CNTPin = true;

					IEC_Bus::SetFastSerialSRQ(true);
					IEC_Bus::ReadEmulationMode1581();

				}
				//if ((CRARegisterOld & CRA_SPMODE) && SerialInput())
				else
				{
					//DEBUG_LOG("serialBitsShiftedSoFar = %d %02x %02x\r\n", serialBitsShiftedSoFar, CRARegisterOld, CRARegister);
					//DEBUG_LOG("12345678901234567890\r\n");//
					serialBitsShiftedSoFar = 0;		// <- WinUAE does not do this.
					serialShiftRegister = 0;		// <- WinUAE does not do this.
					serialDataRegisterLoaded = false;
					serialSetToInputCoolDown = 0;// 32;
					//serialSetToInputLastUpdate = true;
					//CNTPin = IEC_Bus::GetPI_SRQ();
					IEC_Bus::SetFastSerialSRQ(false);
					IEC_Bus::ReadEmulationMode1581();
					bool srq = IEC_Bus::GetPI_SRQ();
					//DEBUG_LOG("sr=%d\r\n", srq);
					//SetPinCNT(srq);	// Like the real hardware we have no inverter on the SRQ input 
					CNTPin = srq;
				}
			}

			//if (CRARegister & CRA_TODIN)
			//{
			//	timerA50Hz = true;
			//}
			//else
			//{
			//	timerA50Hz = false;
			//}

			break;
		}
		case CRB:
			CRBRegister = value;


			// Toggle / Pulse
			// A control bit selects the output applied to PORT B.
			// On every timer underflow the output can either toggle or generate a single positive pulse of one cycle duration.
			// The toggle output is set high whenever the timer is started and is set low by RES.
			//if (CRBRegister & CRB_OUTPUTMODE)
			//{
			//	// Toggle
			//	timerBToggle = true;
			//}
			//else
			//{
			//	// Pulse
			//	timerBToggle = false;
			//}

			// bit 4 will always read back a zero and writing a zero has no effect
			if (CRBRegister & CRB_LOAD)
			{
				// Force load
				// A strobe bit allows the timer latch to be loaded into the timer counter at any time, whether the timer is running or not.
				ReloadTimerB();
				CRBRegister &= ~CRB_LOAD;
			}

			switch ((CRBRegister & (CRB_INMODE1 | CRB_INMODE0)) >> 5)
			{
				case 0:
					timerBMode = TB_MODE_PHI2;
				break;
				case 1:
					timerBMode = TB_MODE_CNT_PVE;
				break;
				case 2:
					timerBMode = TB_MODE_TA_UNDEFLOW;
				break;
				case 3:
					timerBMode = TB_MODE_TA_UNDEFLOW_CNT_PVE;
				break;
			}

		break;
	}
}
