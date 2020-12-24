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

#include "XBOXUSB.h"
//To enable serial debugging see "settings.h"
//#define EXTRADEBUG // Uncomment to get even more debugging data
//#define PRINTREPORT // Uncomment to print the report send by the Xbox 360 Controller

XBOXUSB::XBOXUSB(USB *p) : pUsb(p),     // pointer to USB class instance - mandatory
                           bAddress(0), // device address - mandatory
                           bPollEnable(false)
{ // don't start polling before dongle is connected
    x360InitEpInfo(epInfo, XBOXUSB_MAX_ENDPOINTS);

    if (pUsb)                            // register in USB subsystem
        pUsb->RegisterDeviceClass(this); //set devConfig[] entry
}

uint8_t XBOXUSB::Init(uint8_t parent, uint8_t port, bool lowspeed)
{
    uint8_t buf[sizeof(USB_DEVICE_DESCRIPTOR)];
    USB_DEVICE_DESCRIPTOR *udd = reinterpret_cast<USB_DEVICE_DESCRIPTOR *>(buf);
    uint8_t rcode;
    UsbDevice *p = NULL;
    EpInfo *oldep_ptr = NULL;
    uint16_t PID;
    uint16_t VID;
    bool v114;		// 2 Wired Controller Versions: 1.10 & 1.14

    // get memory address of USB device address pool
    AddressPool &addrPool = pUsb->GetAddressPool();
#ifdef EXTRADEBUG
    Notify(PSTR("\r\nXBOXUSB Init"), 0x80);
#endif
    // check if address has already been assigned to an instance
    if (bAddress)
    {
#ifdef DEBUG_USB_HOST
        Notify(PSTR("\r\nAddress in use"), 0x80);
#endif
        return USB_ERROR_CLASS_INSTANCE_ALREADY_IN_USE;
    }

    // Get pointer to pseudo device with address 0 assigned
    p = addrPool.GetUsbDevicePtr(0);

    if (!p)
    {
#ifdef DEBUG_USB_HOST
        Notify(PSTR("\r\nAddress not found"), 0x80);
#endif
        return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;
    }

    if (!p->epinfo)
    {
#ifdef DEBUG_USB_HOST
        Notify(PSTR("\r\nepinfo is null"), 0x80);
#endif
        return USB_ERROR_EPINFO_IS_NULL;
    }

    // Save old pointer to EP_RECORD of address 0
    oldep_ptr = p->epinfo;

    // Temporary assign new pointer to epInfo to p->epinfo in order to avoid toggle inconsistence
    p->epinfo = epInfo;

    p->lowspeed = lowspeed;

    // Get device descriptor
    rcode = pUsb->getDevDescr(0, 0, sizeof(USB_DEVICE_DESCRIPTOR), (uint8_t *)buf);
    if (udd->bcdDevice == 0x114)
        v114 = true;
    else
        v114 = false;

    // Restore p->epinfo
    p->epinfo = oldep_ptr;

    if (rcode)
        goto FailGetDevDescr;

    VID = udd->idVendor;
    PID = udd->idProduct;

    if (!VIDPIDOK(VID, PID))
        goto FailUnknownDevice;

    // Allocate new address according to device class
    bAddress = addrPool.AllocAddress(parent, false, port);

    if (!bAddress)
        return USB_ERROR_OUT_OF_ADDRESS_SPACE_IN_POOL;

    // Extract Max Packet Size from device descriptor
    epInfo[0].maxPktSize = udd->bMaxPacketSize0;

    // Assign new address to the device
    rcode = pUsb->setAddr(0, 0, bAddress);
    if (rcode)
    {
        p->lowspeed = false;
        addrPool.FreeAddress(bAddress);
        bAddress = 0;
#ifdef DEBUG_USB_HOST
        Notify(PSTR("\r\nsetAddr: "), 0x80);
        D_PrintHex<uint8_t>(rcode, 0x80);
#endif
        return rcode;
    }
#ifdef EXTRADEBUG
    Notify(PSTR("\r\nAddr: "), 0x80);
    D_PrintHex<uint8_t>(bAddress, 0x80);
#endif
    //delay(300); // Spec says you should wait at least 200ms

    p->lowspeed = false;

    //get pointer to assigned address record
    p = addrPool.GetUsbDevicePtr(bAddress);
    if (!p)
        return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

    p->lowspeed = lowspeed;

    // Assign epInfo to epinfo pointer - only EP0 is known
    rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);
    if (rcode)
        goto FailSetDevTblEntry;

    /* The application will work in reduced host mode, so we can save program and data
    memory space. After verifying the VID we will use known values for the
    configuration values for device, interface, endpoints and HID for the XBOX360 Controllers */

    /* Initialize data structures for endpoints of device */
    x360FillEpInfo(&epInfo[XBOX_INPUT_PIPE], 0x01); // XBOX 360 report endpoint
    // Controller Output epAddr differs for 2 different hardware versions, v1.10: 0x02, v1.14: 0x01
    x360FillEpInfo(&epInfo[XBOX_OUTPUT_PIPE], (v114) ? 0x01 : 0x02); // XBOX 360 output endpoint
    // ChatPad Input epAddr differs for 2 different hardware versions, v1.10: 0x06, v1.14: 0x04
    x360FillEpInfo(&epInfo[XBOX_INPUT_PIPE_CHATPAD], (v114) ? 0x04 : 0x06); // XBOX 360 Chatpad report endpoint

    rcode = pUsb->setEpInfoEntry(bAddress, XBOXUSB_MAX_ENDPOINTS, epInfo);
    if (rcode)
        goto FailSetDevTblEntry;

    delay(200); // Give time for address change

    rcode = pUsb->setConf(bAddress, epInfo[XBOX_CONTROL_PIPE].epAddr, 1);
    if (rcode)
        goto FailSetConfDescr;

#ifdef DEBUG_USB_HOST
    Notify(PSTR("\r\nXbox 360 Controller Connected\r\n"), 0x80);
#endif
    onInit();
    Xbox360Connected = true;
    bPollEnable = true;
    return 0; // Successful configuration

/* Diagnostic messages */
FailGetDevDescr:
#ifdef DEBUG_USB_HOST
    NotifyFailGetDevDescr();
    goto Fail;
#endif

FailSetDevTblEntry:
#ifdef DEBUG_USB_HOST
    NotifyFailSetDevTblEntry();
    goto Fail;
#endif

FailSetConfDescr:
#ifdef DEBUG_USB_HOST
    NotifyFailSetConfDescr();
#endif
    goto Fail;

FailUnknownDevice:
#ifdef DEBUG_USB_HOST
    NotifyFailUnknownDevice(VID, PID);
#endif
    rcode = USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;

Fail:
#ifdef DEBUG_USB_HOST
    Notify(PSTR("\r\nXbox 360 Init Failed, error code: "), 0x80);
    NotifyFail(rcode);
#endif
    Release();
    return rcode;
}

/* Performs a cleanup after failed Init() attempt */
uint8_t XBOXUSB::Release()
{
    Xbox360Connected = false;
    pUsb->GetAddressPool().FreeAddress(bAddress);
    bAddress = 0;
    bPollEnable = false;
    return 0;
}

uint8_t XBOXUSB::Poll()
{
    static uint32_t checkStatusTimer = 0;
    static uint32_t chatPadLedTimer = 0;
    static bool keepAliveToggle = false;
    uint16_t bufferSize;

    if (!bPollEnable)
        return 0;

    bufferSize = EP_MAXPKTSIZE;
    pUsb->inTransfer(bAddress, epInfo[XBOX_INPUT_PIPE].epAddr, &bufferSize, readBuf); // input on endpoint 1
    if (bufferSize > 0) {
        readReport();
#ifdef PRINTREPORT
        printReport(epInfo[ XBOX_INPUT_PIPE ].epAddr, bufferSize); // Uncomment "#define PRINTREPORT" to print the report send by the Xbox 360 Controller
#endif
    }

    bufferSize = EP_MAXPKTSIZE;
    pUsb->inTransfer(bAddress, epInfo[XBOX_INPUT_PIPE_CHATPAD].epAddr, &bufferSize, readBuf); // input on chatpad endpoint
    if (bufferSize > 0) {
        readChatPadReport();
#ifdef PRINTREPORT
        printReport(epInfo[XBOX_INPUT_PIPE_CHATPAD].epAddr, bufferSize); // Uncomment "#define PRINTREPORT" to print the report send by the Xbox 360 Controller
#endif
    }

    if (millis() - chatPadLedTimer > 250) {
        chatPadProcessLed();
        chatPadLedTimer = millis();
    } else if (millis() - checkStatusTimer > 1000) {
        // Send keep alive packet every second
        if (keepAliveToggle)
            chatPadKeepAlive1();
        else
            chatPadKeepAlive2();
        keepAliveToggle = !keepAliveToggle;
        checkStatusTimer=millis();
    }

    return 0;
}

void XBOXUSB::readReport()
{
    if (readBuf == NULL)
        return;
    if (readBuf[0] != 0x00 || readBuf[1] != 0x14)
    { // Check if it's the correct report - the controller also sends different status reports
        return;
    }

    x360ProcessButton(readBuf, 2, &button);
}

void XBOXUSB::readChatPadReport() {
    if (readBuf == NULL)
        return;
    if (readBuf[0] == 0xF0 && readBuf[1] == 0x03) {
        enableChatPad();
    }
    if (readBuf[0] == 0x00)
        x360ProcessChatPad(readBuf, 0, &chatPad);
}

#ifdef PRINTREPORT
void XBOXUSB::printReport(uint8_t endpoint, uint8_t nBytes)
{ //Uncomment "#define PRINTREPORT" to print the report send by the Xbox 360 Controller
    if (readBuf == NULL || nBytes == 0)
        return;

//if (readBuf[0] == 0xF0 & readBuf[1] == 0x04) return;
    Notify(endpoint, 0x80);
    Notify(PSTR(": "), 0x80);
    for (uint8_t i = 0; i < nBytes; i++)
    {
        D_PrintHex<uint8_t>(readBuf[i], 0x80);
        Notify(PSTR(" "), 0x80);
    }
    Notify(PSTR("\r\n"), 0x80);
}
#endif

uint8_t XBOXUSB::getButtonPress(ButtonEnum b)
{
    return(x360GetButtonPress(b, &button));
}

bool XBOXUSB::getButtonClick(ButtonEnum b)
{
    return(x360GetButtonClick(b, &button));
}

uint8_t XBOXUSB::getChatPadPress(ChatPadButton b)
{
    return(x360GetChatPadPress(b, &chatPad));
}

uint8_t XBOXUSB::getChatPadClick(ChatPadButton b)
{
    return(x360GetChatPadClick(b, &chatPad));
}

int16_t XBOXUSB::getAnalogHat(AnalogHatEnum a)
{
    return(x360GetAnalogHat(a, &button));
}

/* Xbox Controller commands */
void XBOXUSB::XboxCommand(uint8_t *data, uint16_t nbytes)
{
    uint32_t timeout;

    uint8_t rcode = hrNAK;
    while (millis() - outPipeTimer < 2);

    timeout= millis();
    while (rcode != hrSUCCESS && (millis() - timeout) < 50)
        rcode = pUsb->outTransfer(bAddress, epInfo[XBOX_OUTPUT_PIPE].epAddr, nbytes, data);

    //Readback any response
    rcode = hrSUCCESS;
    timeout = millis();
    while (rcode != hrNAK && (millis() - timeout) < 50)
    {
        uint16_t bufferSize = EP_MAXPKTSIZE;
        rcode = pUsb->inTransfer(bAddress, epInfo[XBOX_INPUT_PIPE].epAddr, &bufferSize, readBuf);
        if (bufferSize > 0) {
#ifdef PRINTREPORT
    	    printReport(epInfo[ XBOX_INPUT_PIPE ].epAddr, bufferSize); // Uncomment "#define PRINTREPORT" to print the report send by the Xbox 360 Controller
#endif
            readReport();
	}
    }
    outPipeTimer = millis();
}

void XBOXUSB::sendCtrlEp(uint8_t val1, uint8_t val2, uint16_t val3, uint16_t val4)
{
    sendCtrlEp(val1, val2, val3, val4, 0, 0, NULL);
}

void XBOXUSB::sendCtrlEp(uint8_t val1, uint8_t val2, uint16_t val3, uint16_t val4, uint16_t total, uint16_t nbytes, uint8_t *data)
{
    pUsb->ctrlReq(bAddress, epInfo[XBOX_CONTROL_PIPE].epAddr, val1, val2, val3 & 0xFF, (val3 & 0xFF00) >> 8, val4, total, nbytes, data, NULL);
    delay(1);
}

void XBOXUSB::chatPadKeepAlive1()
{
    sendChatPadCommand(0x1f);
}

void XBOXUSB::chatPadKeepAlive2()
{
    sendChatPadCommand(0x1e);
}

void XBOXUSB::enableChatPad()
{
    sendChatPadCommand(0x1b);
}

void XBOXUSB::sendChatPadCommand(uint8_t cmd)
{
    sendCtrlEp(0x41, 0x00, cmd, 0x02);
}

void XBOXUSB::setLedRaw(uint8_t value)
{
    writeBuf[0] = 0x01;
    writeBuf[1] = 0x03;
    writeBuf[2] = value;

    XboxCommand(writeBuf, 3);
}

void XBOXUSB::setLedOn(LEDEnum led)
{
    if (led == OFF)
        setLedRaw(0);
    else if (led != ALL) // All LEDs can't be on a the same time
        setLedRaw(pgm_read_byte(&XBOX_LEDS[(uint8_t)led]) + 4);
}

void XBOXUSB::setRumbleOn(uint8_t lValue, uint8_t rValue)
{
#ifdef XBOXUSB_RUMBLE_OPTION
    if (rumbleMotorOn) {
#endif
        memset(writeBuf, 0, 8);
        writeBuf[1] = 0x08;
        writeBuf[3] = lValue; // big weight
        writeBuf[4] = rValue; // small weight

        XboxCommand(writeBuf, 8);
#ifdef XBOXUSB_RUMBLE_OPTION
    }
#endif
}

#ifdef XBOXUSB_RUMBLE_OPTION
void XBOXUSB::setRumbleMotorOn(bool motorOn)
{
    rumbleMotorOn = motorOn;
}
#endif

void XBOXUSB::chatPadQueueLed(uint8_t led)
{
    x360ChatPadQueueLed(led, &chatPad);
}

void XBOXUSB::chatPadProcessLed()
{
    if (chatPad.chatPadLedQueue[0] != 0xFF)
    {
        sendChatPadCommand(chatPad.chatPadLedQueue[0]);
        x360ChatPadPopLedQueue(&chatPad);
    }
}

void XBOXUSB::onInit()
{
    uint8_t stringDescriptor[10];
    uint8_t code[4];

    //8bit-do appears as a Wired Xbox 360 controller, but will quickly change to a switch controller if you do not request a string descriptor
    //Request string descriptors
    sendCtrlEp(0x80, 0x06, 0x0302, 0x0409, 0x02, 2, stringDescriptor);
    //Request string descriptors
    sendCtrlEp(0x80, 0x06, 0x0302, 0x0409, 0x22, 10, stringDescriptor);


    // Wired Chatpad initialzation Sequence
    sendCtrlEp(0x40, 0xa9, 0xa30c, 0x4423);
    sendCtrlEp(0x40, 0xa9, 0x2344, 0x7f03);
    sendCtrlEp(0x40, 0xa9, 0x5839, 0x6832);
    sendCtrlEp(0xc0, 0xa1, 0x0000, 0xe416, 2, 2, code);
    code[0] = 0x09;
    code[1] = 0x00;
    sendCtrlEp(0x40, 0xa1, 0x0000, 0xe416, 2, 2, code);
    sendCtrlEp(0xc0, 0xa1, 0x0000, 0xe416, 2, 2, code);

    // Starting sending ChatPad keep alive packets
    chatPadKeepAlive1();
    enableChatPad();

    // Rewrite to save flash memory
    uint8_t outBuf[3];
    outBuf[0] = 0x01;
    outBuf[1] = 0x03;
    outBuf[2] = 0x02;
    XboxCommand(outBuf, 3);
    outBuf[0] = 0x01;
    outBuf[1] = 0x03;
    outBuf[2] = 0x06;
    XboxCommand(outBuf, 3);
    outBuf[0] = 0x02;		//Not sure what this. Seen in windows driver
    outBuf[1] = 0x08;
    outBuf[2] = 0x03;
    XboxCommand(outBuf, 3);
    outBuf[0] = 0x00;		//Turn off rumble?
    outBuf[1] = 0x03;
    outBuf[2] = 0x00;
    XboxCommand(outBuf, 3);

    //Init all chatpad led FIFO queues 0xFF means empty spot.
    chatPad.chatPadLedQueue[0] = 0xFF;
    chatPad.chatPadLedQueue[1] = 0xFF;
    chatPad.chatPadLedQueue[2] = 0xFF;
    chatPad.chatPadLedQueue[3] = 0xFF;

    if (pFuncOnInit)
        pFuncOnInit(); // Call the user function
    else
        setLedOn(static_cast<LEDEnum>(LED1));
}
