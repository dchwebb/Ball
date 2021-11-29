#include "common_blesvc.h"
#include "custom_stm.h"

typedef struct {
	uint16_t  CustomMjoyHdle;                    /**< Mountjoy_Service handle */
	uint16_t  CustomMjchar_WHdle;                /**< Mountjoy_Char_Write handle */
	uint16_t  CustomMjchar_NHdle;                /**< Mountjoy_Char_Notify handle */
} CustomContext_t;

PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static CustomContext_t CustomContext;

#define UUID_128_SUPPORTED  1
#define BM_UUID_LENGTH  UUID_TYPE_128
#define BM_REQ_CHAR_SIZE    (3)
#define CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET         2
#define CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET              1

static const uint8_t SizeMjchar_W = 1;
static const uint8_t SizeMjchar_N = 4;

static SVCCTL_EvtAckStatus_t Custom_STM_Event_Handler(void *pckt);

#define COPY_UUID_128(uuid_struct, uuid_15, uuid_14, uuid_13, uuid_12, uuid_11, uuid_10, uuid_9, uuid_8, uuid_7, uuid_6, uuid_5, uuid_4, uuid_3, uuid_2, uuid_1, uuid_0) \
		do {\
			uuid_struct[0] = uuid_0; uuid_struct[1] = uuid_1; uuid_struct[2] = uuid_2; uuid_struct[3] = uuid_3; \
			uuid_struct[4] = uuid_4; uuid_struct[5] = uuid_5; uuid_struct[6] = uuid_6; uuid_struct[7] = uuid_7; \
			uuid_struct[8] = uuid_8; uuid_struct[9] = uuid_9; uuid_struct[10] = uuid_10; uuid_struct[11] = uuid_11; \
			uuid_struct[12] = uuid_12; uuid_struct[13] = uuid_13; uuid_struct[14] = uuid_14; uuid_struct[15] = uuid_15; \
		} while (0)

#define COPY_MOUNTJOY_SERVICE_UUID(uuid_struct)       COPY_UUID_128(uuid_struct,0x00,0x00,0x00,0x00,0x00,0x01,0x11,0xe1,0x9a,0xb4,0x00,0x02,0xa5,0xd5,0xc5,0x1b)
#define COPY_MOUNTJOY_CHAR_WRITE_UUID(uuid_struct)    COPY_UUID_128(uuid_struct,0x00,0x00,0xaa,0xcc,0x8e,0x22,0x45,0x41,0x9d,0x4c,0x21,0xed,0xae,0x82,0xed,0x19)
#define COPY_MOUNTJOY_CHAR_NOTIFY_UUID(uuid_struct)   COPY_UUID_128(uuid_struct,0x00,0x04,0x00,0x00,0x00,0x01,0x11,0xe1,0xac,0x36,0x00,0x02,0xa5,0xd5,0xc5,0x1b)


/**
 * @brief  Event handler
 * @param  Event: Address of the buffer holding the Event
 * @retval Ack: Return whether the Event has been managed or not
 */
static SVCCTL_EvtAckStatus_t Custom_STM_Event_Handler(void *Event)
{
	SVCCTL_EvtAckStatus_t return_value;
	hci_event_pckt *event_pckt;
	evt_blecore_aci *blecore_evt;
	aci_gatt_attribute_modified_event_rp0 *attribute_modified;
	aci_gatt_write_permit_req_event_rp0   *write_perm_req;
	aci_gatt_read_permit_req_event_rp0    *read_req;
	Custom_STM_App_Notification_evt_t     Notification;

	return_value = SVCCTL_EvtNotAck;
	event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch(event_pckt->evt) {
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		blecore_evt = (evt_blecore_aci*)event_pckt->data;

		switch(blecore_evt->ecode) {
		case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;
			if (attribute_modified->Attr_Handle == (CustomContext.CustomMjchar_NHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				switch(attribute_modified->Attr_Data[0]) {
				case (!(COMSVC_Notification)):
                	Notification.Custom_Evt_Opcode = CUSTOM_STM_MJCHAR_N_NOTIFY_DISABLED_EVT;
					Custom_STM_App_Notification(&Notification);
					break;

				/* Enabled Notification management */
				case COMSVC_Notification:
					Notification.Custom_Evt_Opcode = CUSTOM_STM_MJCHAR_N_NOTIFY_ENABLED_EVT;
					Custom_STM_App_Notification(&Notification);
					break;

				default:
					break;
				}
			}

			else if (attribute_modified->Attr_Handle == (CustomContext.CustomMjchar_WHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

			}
			break;

		case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE :
			read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;
			if (read_req->Attribute_Handle == (CustomContext.CustomMjchar_WHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			break;

		case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
			if (write_perm_req->Attribute_Handle == (CustomContext.CustomMjchar_WHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				/* Allow or reject a write request from a client using aci_gatt_write_resp(...) function */
			}
			break;

		default:
			break;
		}
		break;

		default:
		break;
	}

	return(return_value);
}



void SVCCTL_InitCustomSvc(void)
{

	Char_UUID_t  uuid;

	/**
	 *  Register the event handler to the BLE controller
	 */
	SVCCTL_RegisterSvcHandler(Custom_STM_Event_Handler);

	/*
	 *          Mountjoy_Service
	 *
	 * Max_Attribute_Records = 1 + 2*2 + 1*no_of_char_with_notify_or_indicate_property + 1*no_of_char_with_broadcast_property
	 * service_max_attribute_record = 1 for Mountjoy_Service +
	 *                                2 for Mountjoy_Char_Write +
	 *                                2 for Mountjoy_Char_Notify +
	 *                                1 for Mountjoy_Char_Notify configuration descriptor +
	 *                              = 6
	 */

	COPY_MOUNTJOY_SERVICE_UUID(uuid.Char_UUID_128);
	aci_gatt_add_service(UUID_TYPE_128,
			(Service_UUID_t *) &uuid,
			PRIMARY_SERVICE,
			6,
			&(CustomContext.CustomMjoyHdle));

	/**
	 *  Mountjoy_Char_Write
	 */
	 COPY_MOUNTJOY_CHAR_WRITE_UUID(uuid.Char_UUID_128);
	 aci_gatt_add_char(CustomContext.CustomMjoyHdle,
			 UUID_TYPE_128, &uuid,
			 SizeMjchar_W,
			 CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RESP,
			 ATTR_PERMISSION_NONE,
			 GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			 0x10,
			 CHAR_VALUE_LEN_CONSTANT,
			 &(CustomContext.CustomMjchar_WHdle));
	 /**
	  *  Mountjoy_Char_Notify
	  */
	  COPY_MOUNTJOY_CHAR_NOTIFY_UUID(uuid.Char_UUID_128);
	 aci_gatt_add_char(CustomContext.CustomMjoyHdle,
			 UUID_TYPE_128, &uuid,
			 SizeMjchar_N,
			 CHAR_PROP_NOTIFY,
			 ATTR_PERMISSION_NONE,
			 GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			 0x10,
			 CHAR_VALUE_LEN_CONSTANT,
			 &(CustomContext.CustomMjchar_NHdle));


	 return;
}

/**
 * @brief  Characteristic update
 * @param  CharOpcode: Characteristic identifier
 * @param  Service_Instance: Instance of the service to which the characteristic belongs
 *
 */
tBleStatus Custom_STM_App_Update_Char(Custom_STM_Char_Opcode_t CharOpcode, uint8_t *pPayload)
{
	tBleStatus result = BLE_STATUS_INVALID_PARAMS;

	switch(CharOpcode) {

	case CUSTOM_STM_MJCHAR_W:
		result = aci_gatt_update_char_value(CustomContext.CustomMjoyHdle,
				CustomContext.CustomMjchar_WHdle,
				0, /* charValOffset */
				SizeMjchar_W, /* charValueLen */
				(uint8_t *)  pPayload);
		break;

	case CUSTOM_STM_MJCHAR_N:
		result = aci_gatt_update_char_value(CustomContext.CustomMjoyHdle,
				CustomContext.CustomMjchar_NHdle,
				0, /* charValOffset */
				SizeMjchar_N, /* charValueLen */
				(uint8_t *)  pPayload);
		break;

	default:
		break;
	}

	return result;
}
