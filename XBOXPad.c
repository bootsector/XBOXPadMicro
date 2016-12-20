/*
     .k8GOGGNqkSFS5XkqXPSkSkkqXXFS5kSkSS15U22F2515U2uujuu1U1u2U1U2uUuFS.
   :0qE     JS5uuJuuFFX51jU2SSk12jU2SSXF5uuu15SFS5k12ujj21S5kFS5S12jJYu11
  5XS:        1UYYLu.   vUUX    U22r     SUF         SUF           ;YYLuU5
 1F5i  NNSkS7  2uLJui   51u     S5.      .PX         .XX           LJvLLu1.
 kUk  0iLk5FFu vuYY2:   5F    Xkk7        78    E0    i0    GEXPXk2uLLvLLuk
X25, 8O   2kX0  5YJUi   M    555    PkXk   i    q1FU   7    ONNkP12YLvLvLYS
S25  8888  888  5uY5         FuS    PS50   .    FuUU   7          uJvLvLLJ2i
kUF             SJjU.      P02UF    P25k   .    Su2Y   v          2LLvLvLL17
S21  XJj88  0u  1uY2.        X2k           .    k11E   v    7;ii:JuJvLvLvJ2:
2257 jqv   Pqq  1LJur         PP.          7    EX:    q    OqqXP51JYvLvYYS.
 X2F  kXkXSXk  kJYLU:   O     ,Z    0PXZ   i    ii    q0    i:::,,.jLLvLLuF'
 ik1k  ;qkPj  .uJvYu:   UN      :   XU2F   :         S5S           iJLLvjUF8
  :PSq       72uLLLui   uSi    .;   2uY1   r.       72j1           LYYLYJSU88
    XqE2   rP12juJuu1FX55U5FqXXSXkXF1juUkkPSXSPXPXPF1Jju5FkFSFXFSF5uujUu5j28V
      .uGOZESS5S5SFkkPkPkXkPXqXPXqXXFkSkkPXPXPkqSkSS1521252121U2u2u12Suv7

*
* Arduino Micro (Leonardo) Classic XBOX Pad emulator firmware
*
* Copyright (c) 2016
* Bruno Freitas - bruno@brunofreitas.com
* Jon Wilson    - degenatrons@gmail.com
* Kevin Mackett - kevin@sharpfork.com
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "XBOXPad.h"

USB_JoystickReport_Data_t gamepad_state;

static int padDetected = 0;

void (*padDetectedCallback)(void) = NULL;

void (*padUSBConnectEventCallback)(void) = NULL;

void (*padUSBDisconnectEventCallback)(void) = NULL;

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void) {
	if(padUSBConnectEventCallback)
		padUSBConnectEventCallback();
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management and joystick reporting tasks.
 */
void EVENT_USB_Device_Disconnect(void) {
	if(padUSBDisconnectEventCallback)
		padUSBDisconnectEventCallback();
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the joystick reporting task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void) {
	/* Setup HID Report Endpoint */
	Endpoint_ConfigureEndpoint(JOYSTICK_EPADDR_IN, EP_TYPE_INTERRUPT, 20, 1);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void) {
	/* Handle HID Class specific requests */
	switch (USB_ControlRequest.bRequest) {
	case HID_REQ_GetReport:
		if (USB_ControlRequest.bmRequestType
				== (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE)) {
			Endpoint_ClearSETUP();

			/* Write the report data to the control endpoint */
			Endpoint_Write_Control_Stream_LE(&gamepad_state, 20);
			Endpoint_ClearOUT();
		}

		break;

	default:
		if (USB_ControlRequest.bmRequestType
				== (REQDIR_DEVICETOHOST | REQTYPE_VENDOR | REQREC_INTERFACE)) {
			if (USB_ControlRequest.bRequest == 0x06) {
				Endpoint_ClearSETUP();

				/* Write the report data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(gamepad_state.reserved_2, 16);
				Endpoint_ClearOUT();

				padDetected = 1;

				if(padDetectedCallback)
					padDetectedCallback();
			}

		}

		break;
	}
}

/** Function to manage HID report generation and transmission to the host. */
void HID_Task(void) {
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	/* Select the Joystick Report Endpoint */
	Endpoint_SelectEndpoint(JOYSTICK_EPADDR_IN);

	/* Check to see if the host is ready for another packet */
	if (Endpoint_IsINReady()) {
		/* Write Joystick Report Data */
		Endpoint_Write_Stream_LE(&gamepad_state, 20, NULL);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
	}
}

void xbox_reset_pad_status(void) {
	memset(&gamepad_state, 0x00, sizeof(USB_JoystickReport_Data_t));

	gamepad_state.rsize = 20;

	gamepad_state.reserved_2[0] = 16; // 16 - must be greater than 7, length of this report?
	gamepad_state.reserved_2[1] = 66; //  0 - needed, USB interface protocol?
	gamepad_state.reserved_2[2] = 0; //  0 - needed, USB interface protocol?
	gamepad_state.reserved_2[3] = 1; //  1 - must be greater than 0, number of interfaces?
	gamepad_state.reserved_2[4] = 1; //  1 - needed, configuration index?
	gamepad_state.reserved_2[5] = 2; //  2 - must be greater than 0, number of endpoints?
	gamepad_state.reserved_2[6] = 20; // 20 - must be less or equal than max packet size for in endpoint, in max packet size?
	gamepad_state.reserved_2[7] = 6; //  6 - must be less or equal than max packet size for out endpoint, out max packet size?

	for (int i = 0; i < 8; i++) {
		gamepad_state.reserved_3[i] = 0xFF;
	}
}

void xbox_send_pad_state(void) {
	HID_Task();
	USB_USBTask();
}

void xbox_reset_watchdog(void) {
	wdt_reset();
}

void xbox_init(bool watchdog) {
	if (watchdog) {
		wdt_enable(WDTO_2S);
	} else {
		wdt_disable();
	}

	xbox_reset_pad_status();

	USB_Init();

	sei();
}

int xbox_pad_detected(void) {
	return padDetected;
}

void xbox_set_detected_callback(void (*callbackPtr)(void)) {
	padDetectedCallback = callbackPtr;
}

void xbox_set_connect_callback(void (*callbackPtr)(void)) {
	padUSBConnectEventCallback = callbackPtr;
}

void xbox_set_disconnect_callback(void (*callbackPtr)(void)) {
	padUSBDisconnectEventCallback = callbackPtr;
}

