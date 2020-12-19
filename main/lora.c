/* 
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "lmic.h"
#include "config.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"


static const char *TAG = "LORA APP";

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

const lmic_pinmap lmic_pins = {
  .nss = 5,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = LMIC_UNUSED_PIN,
  .dio = {26, 33, 32},
#ifdef CONFIG_LMIC_SPI_VSPI
.spi[0] = 19,  //MISO 19
.spi[1] = 23,  //MOSI
.spi[2] = 18,  //SCLK
#elif CONFIG_LMIC_SPI_HSPI
.spi[0] = 12,  //MISO
.spi[1] = 13,  //MOSI
.spi[2] = 14,  //SCLK
#endif /* CFG_SPI_PINS */
};



// LoRaWAN NwkSKey, network session key
	static const u1_t NWKSKEY[16] = { 0x93, 0x1A, 0x9A, 0xDF, 0xDD, 0xCE, 0x1E, 0x98, 0x59, 0xFD, 0x16, 0xE2, 0x42, 0x14, 0xAB, 0x2E };
// LoRaWAN AppSKey, application session key
	static const u1_t APPSKEY[16] = { 0x0A, 0x68, 0x8B, 0x0F, 0x2E, 0xC0, 0x2E, 0x1E, 0xFD, 0x40, 0xDE, 0x73, 0xB5, 0x7B, 0xE2, 0xC7 };
// LoRaWAN end-device address (DevAddr)
	static const u4_t DEVADDR = {0x26031B60};
// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static osjob_t sendjob;

// Schedule data trasmission in every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 60; // Fair Use policy of TTN requires update interval of at least several min. We set update interval here of 1 min for testing

static const u4_t loraBuffer = {0x1234526031B60};

void do_send(osjob_t* j)
{
  ESP_LOGI(TAG, "Check if there is not a current TX/RX job running"); 
  if (LMIC.opmode & OP_TXRXPEND) 
  {
	  ESP_LOGI(TAG, "OP_TXRXPEND, not sending\r\n"); 
  } 
  else
  if (!(LMIC.opmode & OP_TXRXPEND)) 
  {
    ESP_LOGI(TAG, "Prepare upstream data transmission at the next possible time");
    LMIC_setTxData2(1, (xref2u1_t) loraBuffer, 10, 0);
  }
	ESP_LOGI(TAG, "Next TX is scheduled after TX_COMPLETE event");
}

void onEvent (ev_t ev) 
{
  switch(ev) 
  {
    case EV_TXCOMPLETE:
	  ESP_LOGI(TAG, "EV_TXCOMPLETE (includes waiting for RX windows)\r\n");
	  if(LMIC.dataLen) 
		{
		  ESP_LOGI(TAG, "Received %d bytes in RX after TX Slot\n", LMIC.dataLen);   // data received in rx slot after tx
		}
        os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);  // Schedule next transmission
	  break;  
    case EV_RXCOMPLETE:
      if (LMIC.dataLen)
      {
        ESP_LOGI(TAG, "Received %d bytes\n", LMIC.dataLen);
      }
      break;
    default:
	    ESP_LOGI(TAG, "Unknown event\r\n");
     break;
  }
    
}

void app_main(void)
{
	

    ESP_LOGW(TAG,"Aplicacion LoRaWan ESP-IDF Version 1.0!\n");
	os_init();
	LMIC_reset();   	// Reset the MAC state. Session and pending data transfers will be discarded.
	
	uint8_t appskey[sizeof(APPSKEY)];
	uint8_t nwkskey[sizeof(NWKSKEY)];
	memcpy(appskey, APPSKEY, sizeof(APPSKEY));
	memcpy(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  
	LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);

	for (int b = 0; b < 8; ++b) {
		LMIC_disableSubBand(b);
    }
	LMIC_enableChannel(0);

	LMIC_setLinkCheckMode(0);   // Disable link check validation
    ESP_LOGI(TAG, "TTN uses SF9 for its RX2 window");
	LMIC.dn2Dr = DR_SF9;
    ESP_LOGI(TAG, "Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library");
	LMIC_setDrTxpow(DR_SF7,14);
    ESP_LOGI(TAG, "LMIC setup done!\r\n");
  
    ESP_LOGI(TAG, "Start job");
	do_send(&sendjob);

	ESP_LOGI(TAG, "Iniciando el Loop");

  for(;;)
  {
    vTaskDelay((TickType_t) 1);  // Watchdog Reset
	os_runloop_once();
  }
}
