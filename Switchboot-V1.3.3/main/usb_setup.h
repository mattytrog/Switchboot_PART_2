#include <Arduino.h>
#include <Usb.h>
#include <Adafruit_DotStar.h>

#define INTERMEZZO_SIZE 92
const byte intermezzo[INTERMEZZO_SIZE] =
{
  0x44, 0x00, 0x9F, 0xE5, 0x01, 0x11, 0xA0, 0xE3, 0x40, 0x20, 0x9F, 0xE5, 0x00, 0x20, 0x42, 0xE0,
  0x08, 0x00, 0x00, 0xEB, 0x01, 0x01, 0xA0, 0xE3, 0x10, 0xFF, 0x2F, 0xE1, 0x00, 0x00, 0xA0, 0xE1,
  0x2C, 0x00, 0x9F, 0xE5, 0x2C, 0x10, 0x9F, 0xE5, 0x02, 0x28, 0xA0, 0xE3, 0x01, 0x00, 0x00, 0xEB,
  0x20, 0x00, 0x9F, 0xE5, 0x10, 0xFF, 0x2F, 0xE1, 0x04, 0x30, 0x90, 0xE4, 0x04, 0x30, 0x81, 0xE4,
  0x04, 0x20, 0x52, 0xE2, 0xFB, 0xFF, 0xFF, 0x1A, 0x1E, 0xFF, 0x2F, 0xE1, 0x20, 0xF0, 0x01, 0x40,
  0x5C, 0xF0, 0x01, 0x40, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x01, 0x40,
};

#define PACKET_CHUNK_SIZE 0x1000

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.print (x)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#define DEBUG_PRINTHEX(x,y)  serialPrintHex (x,y)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTHEX(x,y)
#endif

byte usbWriteBuffer[PACKET_CHUNK_SIZE] = {0};
uint32_t usbWriteBufferUsed = 0;
uint32_t packetsWritten = 0;
byte tegraDeviceAddress = -1;
bool foundTegra = false;

EpInfo epInfo[3];

USBHost usb;
#ifdef DOTSTAR_ENABLED
Adafruit_DotStar strip = Adafruit_DotStar(1, INTERNAL_DS_DATA, INTERNAL_DS_CLK, DOTSTAR_BGR);
#endif

const char *hexChars = "0123456789ABCDEF";
void serialPrintHex(const byte *data, byte length)
{
  for (int i = 0; i < length; i++)
  {
    DEBUG_PRINT(hexChars[(data[i] >> 4) & 0xF]);
    DEBUG_PRINT(hexChars[data[i] & 0xF]);
  }
  DEBUG_PRINTLN();
}

void usbOutTransferChunk(uint32_t addr, uint32_t ep, uint32_t nbytes, uint8_t* data)
{


  EpInfo* epInfo = usb.getEpInfoEntry(addr, ep);

  usb_pipe_table[epInfo->epAddr].HostDescBank[0].CTRL_PIPE.bit.PDADDR = addr;

  if (epInfo->bmSndToggle)
    USB->HOST.HostPipe[epInfo->epAddr].PSTATUSSET.reg = USB_HOST_PSTATUSSET_DTGL;
  else
    USB->HOST.HostPipe[epInfo->epAddr].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_DTGL;

  UHD_Pipe_Write(epInfo->epAddr, PACKET_CHUNK_SIZE, data);
  uint32_t rcode = usb.dispatchPkt(tokOUT, epInfo->epAddr, 15);
  if (rcode)
  {
    if (rcode == USB_ERROR_DATATOGGLE)
    {
      epInfo->bmSndToggle = USB_HOST_DTGL(epInfo->epAddr);
      if (epInfo->bmSndToggle)
        USB->HOST.HostPipe[epInfo->epAddr].PSTATUSSET.reg = USB_HOST_PSTATUSSET_DTGL;
      else
        USB->HOST.HostPipe[epInfo->epAddr].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_DTGL;
    }
    else
    {
      DEBUG_PRINTLN("Error in OUT transfer");
      return;
    }
  }

  epInfo->bmSndToggle = USB_HOST_DTGL(epInfo->epAddr);
}

void usbFlushBuffer()
{
  usbOutTransferChunk(tegraDeviceAddress, 0x01, PACKET_CHUNK_SIZE, usbWriteBuffer);

  memset(usbWriteBuffer, 0, PACKET_CHUNK_SIZE);
  usbWriteBufferUsed = 0;
  packetsWritten++;
}

// This accepts arbitrary sized USB writes and will automatically chunk them into writes of size 0x1000 and increment
// packetsWritten every time a chunk is written out.
void usbBufferedWrite(const byte *data, uint32_t length)
{
  while (usbWriteBufferUsed + length >= PACKET_CHUNK_SIZE)
  {
    uint32_t bytesToWrite = min(PACKET_CHUNK_SIZE - usbWriteBufferUsed, length);
    memcpy(usbWriteBuffer + usbWriteBufferUsed, data, bytesToWrite);
    usbWriteBufferUsed += bytesToWrite;
    usbFlushBuffer();
    data += bytesToWrite;
    length -= bytesToWrite;
  }

  if (length > 0)
  {
    memcpy(usbWriteBuffer + usbWriteBufferUsed, data, length);
    usbWriteBufferUsed += length;
  }
}

void usbBufferedWriteU32(uint32_t data)
{
  usbBufferedWrite((byte *)&data, 4);
}

void readTegraDeviceID(byte *deviceID)
{
  byte readLength = 16;
  UHD_Pipe_Alloc(tegraDeviceAddress, 0x01, USB_HOST_PTYPE_BULK, USB_EP_DIR_IN, 0x40, 0, USB_HOST_NB_BK_1);

  if (usb.inTransfer(tegraDeviceAddress, 0x01, &readLength, deviceID))
    DEBUG_PRINTLN("Failed to get device ID!");
}

void sendPayload(const byte *payload, uint32_t payloadLength)
{
  byte zeros[0x1000] = {0};

  usbBufferedWriteU32(0x30298);
  usbBufferedWrite(zeros, 680 - 4);
  for (uint32_t i = 0; i < 0x3C00; i++)
    usbBufferedWriteU32(0x4001F000);

  usbBufferedWrite(intermezzo, INTERMEZZO_SIZE);
  usbBufferedWrite(zeros, 0xFA4);
  if (hekate == true){
  usbBufferedWrite(HKPART1, HKPART1_SIZE);
  usbBufferedWrite(payload, payloadLength);
  usbBufferedWrite(MIDSECTION, MIDSECTION_SIZE);
  
  if (UNWRITTEN_PAYLOAD_NUMBER == 1) {
      usbBufferedWrite(HKSECTION_1, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 2) {
      usbBufferedWrite(HKSECTION_2, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 3) {
      usbBufferedWrite(HKSECTION_3, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 4) {
      usbBufferedWrite(HKSECTION_4, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 5) {
      usbBufferedWrite(HKSECTION_5, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 6) {
      usbBufferedWrite(HKSECTION_6, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 7) {
      usbBufferedWrite(HKSECTION_7, HKSECTION_SIZE);
    } else if (UNWRITTEN_PAYLOAD_NUMBER == 8) {
      usbBufferedWrite(HKSECTION_8, HKSECTION_SIZE);
    }
    
  usbBufferedWrite(MODCHIP_MODE, 23);
  
  if (UNWRITTEN_MODE_NUMBER == 1){
    usbBufferedWrite(NUMBER1, 1);
  } else if (UNWRITTEN_MODE_NUMBER == 2){
    usbBufferedWrite(NUMBER2, 1);
  } else if (UNWRITTEN_MODE_NUMBER == 3){
    usbBufferedWrite(NUMBER3, 1);
  } else if (UNWRITTEN_MODE_NUMBER == 4){
    usbBufferedWrite(NUMBER4, 1);
  } else if (UNWRITTEN_MODE_NUMBER == 5){
    usbBufferedWrite(NUMBER5, 1);
  }
  usbBufferedWrite(USB_STRAP_DETECTED, 21);
  if (USB_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 3);
  } else {
    usbBufferedWrite(NO, 3);
  }

  usbBufferedWrite(VOL_STRAP_DETECTED, 21);
  if (VOLUP_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 3);
  } else {
    usbBufferedWrite(NO, 3);
  }

 usbBufferedWrite(JOYCON_STRAP_DETECTED, 21);
  if (JOYCON_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 3);
  } else {
    usbBufferedWrite(NO, 3);
  }
   
 usbBufferedWrite(ENDSECTION, ENDSECTION_SIZE);
  } else 
  if (argon == true){
    usbBufferedWrite(payload, payloadLength);
  }
  usbFlushBuffer();
}

void findTegraDevice(UsbDeviceDefinition *pdev)
{
  uint32_t address = pdev->address.devAddress;
  USB_DEVICE_DESCRIPTOR deviceDescriptor;
  if (usb.getDevDescr(address, 0, 0x12, (uint8_t *)&deviceDescriptor))
  {
    DEBUG_PRINTLN("Error getting device descriptor.");
    return;
  }

  if (deviceDescriptor.idVendor == 0x0955 && deviceDescriptor.idProduct == 0x7321)
  {
    tegraDeviceAddress = address;
    foundTegra = true;
  }
}

void setupTegraDevice()
{
  epInfo[0].epAddr = 0;
  epInfo[0].maxPktSize = 0x40;
  epInfo[0].epAttribs = USB_TRANSFER_TYPE_CONTROL;
  epInfo[0].bmNakPower = USB_NAK_MAX_POWER;
  epInfo[0].bmSndToggle = 0;
  epInfo[0].bmRcvToggle = 0;

  epInfo[1].epAddr = 0x01;
  epInfo[1].maxPktSize = 0x40;
  epInfo[1].epAttribs = USB_TRANSFER_TYPE_BULK;
  epInfo[1].bmNakPower = USB_NAK_MAX_POWER;
  epInfo[1].bmSndToggle = 0;
  epInfo[1].bmRcvToggle = 0;

  usb.setEpInfoEntry(tegraDeviceAddress, 2, epInfo);
  usb.setConf(tegraDeviceAddress, 0, 0);
  usb.Task();

  UHD_Pipe_Alloc(tegraDeviceAddress, 0x01, USB_HOST_PTYPE_BULK, USB_EP_DIR_IN, 0x40, 0, USB_HOST_NB_BK_1);
}

void standby(){
  #ifdef VOLUP_STRAP_PIN
  VOL_TICK_TIMER = 0;
  #endif
  foundTegra = false;
  
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; /* Enable deepsleep */

  GCLK->CLKCTRL.reg = uint16_t(
                        GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN_GCLK2 |
                        GCLK_CLKCTRL_ID( GCLK_CLKCTRL_ID_EIC_Val )
                      );
  while (GCLK->STATUS.bit.SYNCBUSY) {}

  __DSB(); /* Ensure effect of last store takes effect */
  __WFI(); /* Enter sleep mode */
}
