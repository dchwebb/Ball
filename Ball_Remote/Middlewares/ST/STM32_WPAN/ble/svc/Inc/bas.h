#include "svc_ctl.h"


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BAS_H
#define __BAS_H

#ifdef __cplusplus
extern "C" 
{
#endif

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef enum
{
  BAS_LEVEL_NOT_ENABLED_EVT,
  BAS_LEVEL_NOT_DISABLED_EVT,
  BAS_LEVEL_READ_EVT
} BAS_Opcode_Notification_evt_t;


typedef enum
{
  /* Mountjoy_Char_Notify */
  BAS_NOTIFY_ENABLED_EVT,
  BAS_NOTIFY_DISABLED_EVT,

  BAS_BOOT_REQUEST_EVT
} BAS_Opcode_evt_t;

typedef struct
{
  BAS_Opcode_Notification_evt_t  BAS_Evt_Opcode;
  uint8_t ServiceInstance;
} BAS_Notification_evt_t;



/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
#define BAS_LEVEL_NOTIFICATION_OPTION                                          1


/* Exported functions ------------------------------------------------------- */
void BAS_Init(void);
tBleStatus BAS_Update_Char(uint16_t UUID, uint8_t *pPayload);
void BAS_Notification(BAS_Notification_evt_t * pNotification);
void BAS_App_Init(void);
void BAS_App_Send_Notification(uint8_t level);
//void BAS_App_Notification(BAS_Notification_evt_t *pNotification);



#ifdef __cplusplus
}
#endif

#endif
