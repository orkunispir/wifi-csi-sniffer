/* Get Start Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nvs_flash.h"

#include "esp_mac.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"

#define ENABLE_CHANNEL_HOPPING 0 // hops from channel 1 to 10
#define ENABLE_PROMISCOUS_PRINT 0 // to print every incoming packet
#define ENABLE_PRINT_CSI_TYPE 0 // prints rigorous data and csi type
/**
 * @brief WIFI_SECOND_CHAN_BELOW, WIFI_SECOND_CHAN_NONE, WIFI_SECOND_CHAN_ABOVE
 * 
 */
#define SECOND_CHANNEL WIFI_SECOND_CHAN_BELOW
#define CONFIG_LESS_INTERFERENCE_CHANNEL 5
#define CONFIG_SEND_FREQUENCY 100

//static const uint8_t CONFIG_CSI_SEND_MAC[] = {0x1a, 0x00, 0x00, 0x00, 0x00, 0x00};
static const char *TAG = "csi_recv";

#if ENABLE_PROMISCOUS_PRINT
typedef struct
{
    // Header of the raw packet
    /*unsigned frame_ctrl: 16;
    unsigned duration_id: 16;*/
    uint8_t subtype[1];
    uint8_t ctrl_field[1];
    uint8_t duration[2];
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */

    uint8_t SC[2];
    uint64_t timestamp_abs;
    uint8_t beacon_interval[2];
    uint8_t capability_info[2];

} wifi_ieee80211_mac_hdr_t;

typedef struct
{
    // Packet is header + payload
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;
#endif

static void wifi_init()
{
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // TODO: does STA Mode and AP Mode have any different effect on received csi?
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    //ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, WIFI_BW_HT40));
    ESP_ERROR_CHECK(esp_wifi_start());


    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_LESS_INTERFERENCE_CHANNEL,SECOND_CHANNEL));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, CONFIG_CSI_SEND_MAC));
}

static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{

    //ESP_LOGI(TAG, "Received CSI Data");

    if (!info || !info->buf || !info->mac)
    {
        ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return;
    }

    static uint32_t s_count = 0;
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

#if ENABLE_PRINT_CSI_TYPE
    //printf("type, channel, secondary channel, sig_mode, bandwith, STBC\n");
    /**
     * @brief use this for rigorous information and comment out print form below
     * 
     */
    printf("CSI_RIG," MACSTR ",%d,%d,%d,%d,%d,%d\n", MAC2STR(info->mac),rx_ctrl->channel, rx_ctrl->secondary_channel, rx_ctrl->sig_mode, rx_ctrl->cwb, rx_ctrl->stbc, info->len);
#endif

    if (!s_count)
    {
        ESP_LOGI(TAG, "================ CSI RECV ================");
        ets_printf("type,id,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,len,first_word,data\n");
    }

    ets_printf("CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               s_count++, MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
               rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
               rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
               rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
               rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);

    ets_printf(",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);

    for (int i = 1; i < info->len; i++)
    {
        ets_printf(",%d", info->buf[i]);
    }

    ets_printf("]\"\n");
}
#if ENABLE_PROMISCOUS_PRINT
void promi_cb(void *buff, wifi_promiscuous_pkt_type_t type)
{
    // TODO: Can be ignored for csi extraction purposes, leave behind for now?

    if (type != WIFI_PKT_MGMT)
    {
        printf("Not management");
        return;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    wifi_promiscuous_pkt_t *PayloadPacket = (wifi_promiscuous_pkt_t *)buff;
    wifi_pkt_rx_ctrl_t *rx_ctrl = &(PayloadPacket->rx_ctrl);

    const wifi_ieee80211_packet_t *InfoPacket = (wifi_ieee80211_packet_t *)PayloadPacket->payload;
    const wifi_ieee80211_mac_hdr_t *Header = &InfoPacket->hdr;
    uint8_t *data_ptr = InfoPacket;
    char MacAddTx[20] = {0};
    char MacAddRx[20] = {0};
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    sprintf(MacAddRx, "%02X:%02X:%02X:%02X:%02X:%02X", Header->addr1[0], Header->addr1[1], Header->addr1[2], Header->addr1[3], Header->addr1[4], Header->addr1[5]);
    sprintf(MacAddTx, "%02X:%02X:%02X:%02X:%02X:%02X", Header->addr2[0], Header->addr2[1], Header->addr2[2], Header->addr2[3], Header->addr2[4], Header->addr2[5]);
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // printf("Receiver MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", Header->addr1[0], Header->addr1[1], Header->addr1[2], Header->addr1[3], Header->addr1[4], Header->addr1[5]);
    printf("Sender MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", Header->addr2[0], Header->addr2[1], Header->addr2[2], Header->addr2[3], Header->addr2[4], Header->addr2[5]);
    // printf("Filter MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", Header->addr3[0], Header->addr3[1], Header->addr3[2], Header->addr3[3], Header->addr3[4], Header->addr3[5]);

    printf("RX: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n\n",
           rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
           rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
           rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
           rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
           rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Display Valid Frames only
    //
    if (PayloadPacket->rx_ctrl.sig_mode > 0)
    {
        printf("Rx Data packet from %s to %s\n", MacAddTx, MacAddRx);
        for (int i = 0; i < PayloadPacket->rx_ctrl.sig_len; i++)
            printf("%02x ", data_ptr[i]);
        printf("RX: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
               rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
               rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
               rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
               rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
               rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);
        printf("\n\n");
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////
}
#endif

#if ENABLE_CHANNEL_HOPPING
void changeChannels(void *pvParameters)
{
    wifi_second_chan_t secondCh;
    uint8_t channel;
    while (1)
    {   
        vTaskDelay(4000 / portTICK_PERIOD_MS);  
        ESP_ERROR_CHECK(esp_wifi_get_channel(&channel, &secondCh));
        channel++;
        if (channel >= 11)
        {
            channel = 1;
            if (SECOND_CHANNEL == WIFI_SECOND_CHAN_BELOW)
            {
                secondCh = WIFI_SECOND_CHAN_ABOVE;
            }
            
        }
        ESP_ERROR_CHECK(esp_wifi_set_channel(channel, secondCh));
        printf("================================================\n");
        printf("Set channel to %d\n", channel);
        printf("================================================\n");
    }
}
#endif

static void wifi_csi_init()
{
    //ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(promi_cb));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    /**< default config */
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
        .shift = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
#if ENABLE_CHANNEL_HOPPING
    xTaskCreate(changeChannels, "chngChn", 4096, NULL, 5, NULL);
#endif
}

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    wifi_csi_init();
}
