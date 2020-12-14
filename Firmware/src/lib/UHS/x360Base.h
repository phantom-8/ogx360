/* Copyright (C) 2012 Kristian Lauszus, TKJ Electronics. All rights reserved.

 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").

 Contact information
 -------------------

 Kristian Lauszus, TKJ Electronics
 Web      :  http://www.tkjelectronics.com
 e-mail   :  kristianl@tkjelectronics.com

 */

#ifndef _x360base_h_
#define _x360base_h_

/* Data Xbox 360 taken from descriptors */
#define EP_MAXPKTSIZE 32 // max size for data (for both Wireless & Wired Controllers

// Global buffer storage shared between XBOXUSB & XBOXRECV
extern uint8_t readBuf[]; 	// General purpose buffer for input data
extern uint8_t writeBuf[];	// General purpose buffer for output data

enum ChatPadButton
{
        //Offset byte 26 or 27. You can get 2 buttons are once on the chatpad,
        CHATPAD_1 = 23,
        CHATPAD_2 = 22,
        CHATPAD_3 = 21,
        CHATPAD_4 = 20,
        CHATPAD_5 = 19,
        CHATPAD_6 = 18,
        CHATPAD_7 = 17,
        CHATPAD_8 = 103,
        CHATPAD_9 = 102,
        CHATPAD_0 = 101,

        CHATPAD_Q = 39,
        CHATPAD_W = 38,
        CHATPAD_E = 37,
        CHATPAD_R = 36,
        CHATPAD_T = 35,
        CHATPAD_Y = 34,
        CHATPAD_U = 33,
        CHATPAD_I = 118,
        CHATPAD_O = 117,
        CHATPAD_P = 100,

        CHATPAD_A = 55,
        CHATPAD_S = 54,
        CHATPAD_D = 53,
        CHATPAD_F = 52,
        CHATPAD_G = 51,
        CHATPAD_H = 50,
        CHATPAD_J = 49,
        CHATPAD_K = 119,
        CHATPAD_L = 114,
        CHATPAD_COMMA = 98,

        CHATPAD_Z = 70,
        CHATPAD_X = 69,
        CHATPAD_C = 68,
        CHATPAD_V = 67,
        CHATPAD_B = 66,
        CHATPAD_N = 65,
        CHATPAD_M = 82,
        CHATPAD_PERIOD = 83,
        CHATPAD_ENTER = 99,

        CHATPAD_LEFT = 85,
        CHATPAD_SPACE = 84,
        CHATPAD_RIGHT = 81,
        CHATPAD_BACK = 113,

        //Offset byte 25,
        CHATPAD_SHIFT = 1,
        CHATPAD_GREEN = 2,
        CHATPAD_ORANGE = 4,
        CHATPAD_MESSENGER = 8,
};

#define CHATPAD_LED_CAPSLOCK_OFF 0x00
#define CHATPAD_LED_GREEN_OFF 0x01
#define CHATPAD_LED_ORANGE_OFF 0x02
#define CHATPAD_LED_MESSENGER_OFF 0x03
#define CHATPAD_LED_CAPSLOCK_ON 0x08
#define CHATPAD_LED_GREEN_ON 0x09
#define CHATPAD_LED_ORANGE_ON 0x0A
#define CHATPAD_LED_MESSENGER_ON 0x0B

// Button structure common for both Wireless & Wired Controllers
typedef struct {
        /* Variables to store the buttons */
        uint32_t ButtonState;
        uint32_t OldButtonState;

        uint16_t ButtonClickState;
        int16_t hatValue[4];

        bool buttonStateChanged; // True if a button has changed
        bool L2Clicked; // These buttons are analog, so we use we use these bools to check if they where clicked or not
        bool R2Clicked;
} x360ButtonType;

// ChatPad structure common for both Wireless & Wired Controllers
typedef struct {
        /* Variables to store the chatpad buttons */
        uint32_t ChatPadState;
        uint32_t OldChatPadState;
        uint32_t ChatPadClickState;
	uint8_t chatPadLedQueue[4];
        bool ChatPadStateChanged; // True if a chatpad button has changed
} x360ChatPadType;

// Xbox 360 Common Function Prototypes
void x360InitEpInfo(EpInfo *epInfo, uint8_t maxEndpoints);
void x360FillEpInfo(EpInfo *epInfo, uint8_t epAddr);
void x360ProcessButton(uint8_t *readBuf, uint8_t offset, x360ButtonType *button);
void x360ProcessChatPad(uint8_t *readBuf, uint8_t offset, x360ChatPadType *chatPad);
uint8_t x360GetButtonPress(ButtonEnum b, x360ButtonType *button);
bool x360GetButtonClick(ButtonEnum b, x360ButtonType *button);
uint8_t x360GetChatPadPress(ChatPadButton b, x360ChatPadType *chatPad);
uint8_t x360GetChatPadClick(ChatPadButton b, x360ChatPadType *chatPad);
int16_t x360GetAnalogHat(AnalogHatEnum a, x360ButtonType *button);
bool x360ButtonChanged(x360ButtonType *button);
void x360ChatPadQueueLed(uint8_t led, x360ChatPadType *chatPad);
void x360ChatPadPopLedQueue(x360ChatPadType *chatPad);


#endif
