#include "zcl_utility.h"


#include "zcl/esp_zigbee_zcl_command.h"


esp_err_t zcl_utility_send_update_cmd(
    const uint8_t endpoint_id,
    const uint16_t cluster_id,
    const uint16_t attribute_id
) {
    esp_zb_zcl_report_attr_cmd_t ph_cmd_req = {
        .clusterID = cluster_id,
        .attributeID = attribute_id,
        .address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .zcl_basic_cmd.src_endpoint = endpoint_id
    };
    return esp_zb_zcl_report_attr_cmd_req(&ph_cmd_req);
}
