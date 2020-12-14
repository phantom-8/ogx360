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

getBatteryLevel and checkStatus functions made by timstamp.co.uk found using BusHound from Perisoft.net
*/

#include "Usb.h"
#include "xboxEnums.h"
#include "x360Base.h"

// readBuf & writeBuf will be shared for XBOXRECV & XBOXUSB to save memory
uint8_t readBuf[EP_MAXPKTSIZE]; // General purpose buffer for input data
uint8_t writeBuf[12];           // General purpose buffer for output data

void x360InitEpInfo(EpInfo *epInfo, uint8_t maxEndpoints)
{
    for (uint8_t i = 0; i < maxEndpoints; i++)
    {
        epInfo[i].epAddr = 0;
        epInfo[i].maxPktSize = (i) ? 0 : 8;
        epInfo[i].bmSndToggle = 0;
        epInfo[i].bmRcvToggle = 0;
        epInfo[i].bmNakPower = (i) ? USB_NAK_NOWAIT : USB_NAK_MAX_POWER;
    }
    return;
}

void x360FillEpInfo(EpInfo *epInfo, uint8_t epAddr)
{
    /* Initialize data structures for endpoints of device */
    epInfo->epAddr = epAddr; // XBOX 360 report endpoint - poll interval
    epInfo->epAttribs = USB_TRANSFER_TYPE_INTERRUPT;
    epInfo->bmNakPower = USB_NAK_NOWAIT; // Only poll once for interrupt endpoints
    epInfo->maxPktSize = EP_MAXPKTSIZE;
    epInfo->bmSndToggle = 0;
    epInfo->bmRcvToggle = 0;
    return;
}

void x360ProcessButton(uint8_t *readBuf, uint8_t offset, x360ButtonType *button)
{
    // Wired Controller: Offset 2, Wireless Controller: Offset 6
    button->ButtonState = (uint32_t)(readBuf[offset+3] | ((uint16_t)readBuf[offset+2] << 8) | ((uint32_t)readBuf[offset+1] << 16) | ((uint32_t)readBuf[offset] << 24));

    button->hatValue[LeftHatX] = (int16_t)(((uint16_t)readBuf[offset+5] << 8) | readBuf[offset+4]);
    button->hatValue[LeftHatY] = (int16_t)(((uint16_t)readBuf[offset+7] << 8) | readBuf[offset+6]);
    button->hatValue[RightHatX] = (int16_t)(((uint16_t)readBuf[offset+9] << 8) | readBuf[offset+8]);
    button->hatValue[RightHatY] = (int16_t)(((uint16_t)readBuf[offset+11] << 8) | readBuf[offset+10]);

    if (button->ButtonState != button->OldButtonState)
    {
        button->buttonStateChanged = true;
        //Update click state variable, but don't include the two trigger buttons L2 and R2
        button->ButtonClickState = (button->ButtonState >> 16) & ((~(button->OldButtonState)) >> 16);
        if (((uint8_t)button->OldButtonState) == 0 && ((uint8_t)button->ButtonState) != 0)
        {
            button->R2Clicked = true;
        }

        if ((uint8_t)(button->OldButtonState >> 8) == 0 && (uint8_t)(button->ButtonState >> 8) != 0)
        {
            button->L2Clicked = true;
        }
        button->OldButtonState = button->ButtonState;
//Serial1.print("Button: ");
//Serial1.println(button->ButtonState, HEX);
    }
}

void x360ProcessChatPad(uint8_t *readBuf, uint8_t offset, x360ChatPadType *chatPad)
{
    // Wired Controller: Offset 2, Wireless Controller: Offset 24
    chatPad->ChatPadState = 0;
    chatPad->ChatPadState |= ((uint32_t)(readBuf[offset+1]) << 16) & 0xFF0000; //This contains modifiers like shift, green, orange and messenger buttons They are OR'd together in one byte
    chatPad->ChatPadState |= ((uint32_t)(readBuf[offset+2]) << 8) & 0x00FF00;  //This contains the first button being pressed.
    chatPad->ChatPadState |= ((uint32_t)(readBuf[offset+3]) << 0) & 0x0000FF;  //This contains the second button being pressed.

    if (chatPad->ChatPadState != chatPad->OldChatPadState)
    {
        chatPad->ChatPadStateChanged = true;
        chatPad->ChatPadClickState = chatPad->ChatPadState & ~(chatPad->OldChatPadState);
        chatPad->OldChatPadState = chatPad->ChatPadState;
//Serial1.print("Chatpad: ");
//Serial1.println(chatPad->ChatPadState, HEX);
    }
}

uint8_t x360GetButtonPress(ButtonEnum b, x360ButtonType *button)
{
    if (b == L2) // These are analog buttons
        return (uint8_t)(button->ButtonState >> 8);
    else if (b == R2)
        return (uint8_t)button->ButtonState;
    return (bool)(button->ButtonState & ((uint32_t)pgm_read_word(&XBOX_BUTTONS[(uint8_t)b]) << 16));
}

bool x360GetButtonClick(ButtonEnum b, x360ButtonType *button)
{
    if (b == L2)
    {
        if (button->L2Clicked)
        {
            button->L2Clicked = false;
            return true;
        }
        return false;
    }
    else if (b == R2)
    {
        if (button->R2Clicked)
        {
            button->R2Clicked = false;
            return true;
        }
        return false;
    }
    uint16_t but = pgm_read_word(&XBOX_BUTTONS[(uint8_t)b]);
    bool click = (button->ButtonClickState & but);
    button->ButtonClickState &= ~but; // clear "click" event
    return click;
}

uint8_t x360GetChatPadPress(ChatPadButton b, x360ChatPadType *chatPad)
{
    uint8_t but = b;
    uint8_t click1 = (uint8_t)(chatPad->ChatPadState >> 16 & 0x0000FF);
    uint8_t click2 = (uint8_t)(chatPad->ChatPadState >> 8 & 0x0000FF);
    uint8_t click3 = (uint8_t)(chatPad->ChatPadState >> 0 & 0x0000FF);

    if (but < 17 && click1 & but)
    {
        return 1;
    }
    else if (but >= 17 && (click2 == but || click3 == but))
    {
        return 1;
    }
    return 0;
}

uint8_t x360GetChatPadClick(ChatPadButton b, x360ChatPadType *chatPad)
{
    uint8_t but = b;
    uint8_t click1 = (uint8_t)((chatPad->ChatPadClickState >> 16) & 0x0000FF);
    uint8_t click2 = (uint8_t)((chatPad->ChatPadClickState >> 8) & 0x0000FF);
    uint8_t click3 = (uint8_t)((chatPad->ChatPadClickState >> 0) & 0x0000FF);

    if (but < 17 && click1 & but)
    {
        chatPad->ChatPadClickState &= ~(((uint32_t)but << 16) & 0xFF0000);
        return 1;
    }
    else if (but >= 17 && click2 == but)
    {
        chatPad->ChatPadClickState &= 0xFF00FF;
        return 1;
    }
    else if (but >= 17 && click3 == but)
    {
        chatPad->ChatPadClickState &= 0xFFFF00;
        return 1;
    }
    return 0;
}

int16_t x360GetAnalogHat(AnalogHatEnum a, x360ButtonType *button)
{
    return button->hatValue[a];
}

bool x360ButtonChanged(x360ButtonType *button)
{
    bool state = button->buttonStateChanged;
    button->buttonStateChanged = false;
    return state;
}

void x360ChatPadQueueLed(uint8_t led, x360ChatPadType *chatPad)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (chatPad->chatPadLedQueue[i] == 0xFF)
        {
            chatPad->chatPadLedQueue[i] = led;
            return;
        }
    }
}

void x360ChatPadPopLedQueue(x360ChatPadType *chatPad)
{
    chatPad->chatPadLedQueue[0] = chatPad->chatPadLedQueue[1];
    chatPad->chatPadLedQueue[1] = chatPad->chatPadLedQueue[2];
    chatPad->chatPadLedQueue[2] = chatPad->chatPadLedQueue[3];
    chatPad->chatPadLedQueue[3] = 0xFF;
}
