#pragma once

#include "initialisation.h"
#include "CDCHandler.h"
#include <functional>
#include <cstring>
#include <string>

// Enables capturing of debug data for output over STLink UART on dev boards
#define USB_DEBUG false
#if (USB_DEBUG)
#include "uartHandler.h"
#define USB_DEBUG_COUNT 400
#endif

// Declare registers for PMA area
struct USB_PMA_t {
	static constexpr uint32_t pmaBlocksMask = 0x1F << 10;
	static constexpr uint32_t pmaBlkSizeMask = 0x1 << 15;

	volatile uint16_t ADDR_TX;
	volatile uint16_t COUNT_TX;
	volatile uint16_t ADDR_RX;
	volatile uint16_t COUNT_RX;
	uint32_t GetTXCount()	{ return COUNT_TX & 0x3FF; }
	uint32_t GetRXCount()	{ return COUNT_RX & 0x3FF; }
	void SetRXBlocks(uint32_t cnt)	{ COUNT_RX = (COUNT_RX & ~pmaBlocksMask) | (cnt << 10); }
	void SetRXBlkSize(uint32_t cnt)	{ COUNT_RX = (COUNT_RX & ~pmaBlkSizeMask) | (cnt << 15); }
};

// Create struct for easy access to endpoint registers
struct USB_EPR_t {
	volatile uint16_t EPR;
	volatile uint16_t reserved;
};




#define USBP     USB

#define LOBYTE(x)  (static_cast<uint8_t>(x & 0x00FFU))
#define HIBYTE(x)  (static_cast<uint8_t>((x & 0xFF00U) >> 8))

class USBMain {
	friend class USBHandler;
public:
	enum Interface {NoInterface = -1, CDCCmdInterface = 0, CDCDataInterface = 1, interfaceCount = 2};
	enum class DeviceState {Suspended, Addressed, Configured} devState;
	enum EndPoint {CDC_In = 0x81, CDC_Out = 0x1, CDC_Cmd = 0x82};
	enum EndPointType {Control = 0, Isochronous = 1, Bulk = 2, Interrupt = 3};
	enum Descriptor {DeviceDescriptor = 0x1, ConfigurationDescriptor = 0x2, StringDescriptor = 0x3, InterfaceDescriptor = 0x4,
		EndpointDescriptor = 0x5, DeviceQualifierDescriptor = 0x6, IadDescriptor = 0xb, BosDescriptor = 0xF, ClassSpecificInterfaceDescriptor = 0x24};
	enum RequestRecipient {RequestRecipientDevice = 0x0, RequestRecipientInterface = 0x1, RequestRecipientEndpoint = 0x2};
	enum RequestType {RequestTypeStandard = 0x0, RequestTypeClass = 0x20, RequestTypeVendor = 0x40};
	enum class Request {GetStatus = 0x0, SetAddress = 0x5, GetDescriptor = 0x6, SetConfiguration = 0x9};
	enum StringIndex {LangId = 0, Manufacturer = 1, Product = 2, Serial = 3, Configuration = 4, MassStorageClass = 5,
		CommunicationClass = 6, AudioClass = 7};

	static constexpr uint8_t ep_maxPacket = 0x40;

	void USBInterruptHandler();
	void InitUSB();
	size_t SendData(const uint8_t* data, uint16_t len, uint8_t endpoint);
	void SendString(const char* s);
	void SendString(const std::string& s);
	size_t SendString(const unsigned char* s, size_t len);
	uint32_t StringToUnicode(const std::string_view desc, uint8_t* unicode);

	EP0Handler  ep0  = EP0Handler(this, 0, 0, NoInterface);
	CDCHandler  cdc  = CDCHandler(this,  USBMain::CDC_In,  USBMain::CDC_Out,  CDCCmdInterface);

	bool classPendingData = false;			// Set when class setup command received and data pending
	bool transmitting;
	uint32_t stringErrors = 0;				// For debug capture number of times sending aborted because busy

private:
	static constexpr std::string_view manufacturerString = "Mountjoy Modular";
	static constexpr std::string_view productString      = "Mountjoy Ball Base";
	static constexpr std::string_view cdcString          = "Mountjoy Ball Base CDC";
	static constexpr uint8_t usbSerialNoSize = 24;	
	static constexpr uint32_t recipientMask = 0x03;
	static constexpr uint32_t requestTypeMask = 0x60;
	static constexpr uint8_t epAddrMask = 0x0F;


	void ProcessSetupPacket();
	void ReadPMA(const uint16_t pma, USBHandler* handler);
	void WritePMA(const uint16_t pma, const uint16_t bytes, USBHandler* handler);
	void ActivateEndpoint(uint8_t endpoint, const Direction direction, EndPointType eptype);
	void GetDescriptor();
	void EPStartXfer(const Direction direction, const uint8_t endpoint, uint32_t xfer_len);
	void EP0In(const uint8_t* buff, uint32_t size);
	bool ReadInterrupts(const uint32_t interrupt);
	void IntToUnicode(uint32_t value, uint8_t* pbuf, uint8_t len);
	uint32_t GetString(const char* desc);
	uint32_t MakeConfigDescriptor();
	void SerialToUnicode();

	std::array<USBHandler*, 2>classesByInterface;	// Lookup tables to get appropriate class handlers (set in handler constructor)
	std::array<USBHandler*, 2>classByEP;
	usbRequest req;

	static constexpr uint32_t pmaStartAddr = 0x20;	// PMA memory will be assigned sequentially to each endpoint from this address in 64 byte chunks (allowing space for PMA header)
	uint32_t pmaAddress;
	uint8_t devAddress = 0;			// Temporarily hold the device address as it cannot stored in the register until the 0 address response has been handled

	uint8_t stringDescr[128];
	uint8_t configDescriptor[255];

	// USB standard device descriptor
	static constexpr uint16_t VendorID = 1155;	// STMicroelectronics
	static constexpr uint16_t ProductId = 65432;

	// USB standard device descriptor - in usbd_desc.c
	const uint8_t USBD_FS_DeviceDesc[0x12] = {
			0x12,								// bLength
			DeviceDescriptor,					// bDescriptorType
			0x01,								// bcdUSB  - 0x01 if LPM enabled
			0x02,
			0xEF,								// bDeviceClass: (Miscellaneous)
			0x02,								// bDeviceSubClass (Interface Association Descriptor- with below)
			0x01,								// bDeviceProtocol (Interface Association Descriptor)
			ep_maxPacket,  						// bMaxPacketSize
			LOBYTE(VendorID),					// idVendor
			HIBYTE(VendorID),					// idVendor
			LOBYTE(ProductId),					// idProduct
			HIBYTE(ProductId),					// idProduct
			0x00,								// bcdDevice rel. 2.00
			0x02,
			Manufacturer,						// Index of manufacturer  string
			Product,							// Index of product string
			Serial,								// Index of serial number string
			0x01								// bNumConfigurations
	};

	// Binary Object Store (BOS) Descriptor
	const uint8_t USBD_FS_BOSDesc[12] = {
			0x05,								// Length
			BosDescriptor,						// DescriptorType
			0x0C,								// TotalLength
			0x00, 0x01,							// NumDeviceCaps

			// USB 2.0 Extension Descriptor: device capability
			0x07,								// bLength
			0x10, 								// USB_DEVICE_CAPABITY_TYPE
			0x02,								// Attributes
			0x02, 0x00, 0x00, 0x00				// Link Power Management protocol is supported
	};


	// USB lang indentifier descriptor
	static constexpr uint16_t languageIDString = 1033;
	const uint8_t USBD_LangIDDesc[4] = {
			0x04,
			StringDescriptor,
			LOBYTE(languageIDString),
			HIBYTE(languageIDString)
	};


public:
#if (USB_DEBUG)
	uint16_t usbDebugNo = 0;
	uint16_t usbDebugEvent = 0;

	struct usbDebugItem {
		uint16_t eventNo;
		uint32_t Interrupt;
		usbRequest Request;
		uint8_t endpoint;
		uint16_t PacketSize;
		uint32_t xferBuff0;
		uint32_t xferBuff1;
	};
	usbDebugItem usbDebug[USB_DEBUG_COUNT];
	void OutputDebug();
#endif
};


extern USBMain usb;
