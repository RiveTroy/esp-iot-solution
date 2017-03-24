/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
  
#include <stdio.h>
#include "driver/i2c.h"
#include "hts221.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    bool dev_addr_10bit_en;
} hts221_dev_t;

esp_err_t hts221_write_one_reg(hts221_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    hts221_dev_t* sens = (hts221_dev_t*) sensor;
	esp_err_t  ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }
	return ESP_OK;
}

esp_err_t hts221_write_reg(hts221_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++) {
            hts221_write_one_reg(sensor, reg_start_addr+i, data_buf[i]);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t hts221_read_one_reg(hts221_handle_t sensor, uint8_t reg, uint8_t *data)
{
    hts221_dev_t* sens = (hts221_dev_t*) sensor;
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t hts221_read_reg(hts221_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    uint8_t data_t = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++){
            hts221_read_one_reg(sensor, reg_start_addr+i, &data_t);
            data_buf[i] = data_t;
        }
        return ESP_OK;
    } 
    return ESP_FAIL;  
}

esp_err_t hts221_get_deviceid(hts221_handle_t sensor, uint8_t* deviceid)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_WHO_AM_I_REG, &tmp);
    *deviceid = tmp;
    return ret;
}

esp_err_t hts221_set_config(hts221_handle_t sensor, hts221_config_t* hts221_config)
{
    uint8_t buffer[3];
    hts221_read_reg(sensor, HTS221_AV_CONF_REG, 1, buffer);
    buffer[0] &= ~(HTS221_AVGH_MASK | HTS221_AVGT_MASK);
    buffer[0] |= (uint8_t)hts221_config->avg_h;
    buffer[0] |= (uint8_t)hts221_config->avg_t;
    hts221_write_reg(sensor, HTS221_AV_CONF_REG, 1, buffer);

    hts221_read_reg(sensor, HTS221_CTRL_REG1, 3, buffer);
    buffer[0] &= ~(HTS221_BDU_MASK | HTS221_ODR_MASK);
    buffer[0] |= (uint8_t)hts221_config->odr;
    buffer[0] |= ((uint8_t)hts221_config->bdu_status) << HTS221_BDU_BIT;
    buffer[1] &= ~HTS221_HEATER_BIT;
    buffer[1] |= ((uint8_t)hts221_config->heater_status) << HTS221_HEATER_BIT;
    buffer[2] &= ~(HTS221_DRDY_H_L_MASK | HTS221_PP_OD_MASK | HTS221_DRDY_MASK);
    buffer[2] |= ((uint8_t)hts221_config->irq_level) << HTS221_DRDY_H_L_BIT;
    buffer[2] |= (uint8_t)hts221_config->irq_output_type;
    buffer[2] |= ((uint8_t)hts221_config->irq_enable) << HTS221_DRDY_BIT;
    hts221_write_reg(sensor, HTS221_CTRL_REG1, 3, buffer);
    return ESP_OK;
}

esp_err_t hts221_get_config(hts221_handle_t sensor, hts221_config_t* hts221_config)
{
    uint8_t buffer[3];
    hts221_read_reg(sensor, HTS221_AV_CONF_REG, 1, buffer);
    hts221_config->avg_h = (hts221_avgh_t)(buffer[0] & HTS221_AVGH_MASK);
    hts221_config->avg_t = (hts221_avgt_t)(buffer[0] & HTS221_AVGT_MASK);

    hts221_read_reg(sensor, HTS221_CTRL_REG1, 3, buffer);
    hts221_config->odr = (hts221_odr_t)(buffer[0] & HTS221_ODR_MASK);
    hts221_config->bdu_status = (hts221_state_t)((buffer[0] & HTS221_BDU_MASK) >> HTS221_BDU_BIT);
    hts221_config->heater_status = (hts221_state_t)((buffer[1] & HTS221_HEATER_MASK) >> HTS221_HEATER_BIT);
    hts221_config->irq_level = (hts221_drdylevel_t)(buffer[2] & HTS221_DRDY_H_L_MASK);
    hts221_config->irq_output_type = (hts221_outputtype_t)(buffer[2] & HTS221_PP_OD_MASK);
    hts221_config->irq_enable = (hts221_state_t)((buffer[2] & HTS221_DRDY_MASK) >> HTS221_DRDY_BIT);
    return ESP_OK;
}

esp_err_t hts221_set_activate(hts221_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG1, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp |= HTS221_PD_MASK;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_powerdown(hts221_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG1, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_PD_MASK;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_odr(hts221_handle_t sensor, hts221_odr_t odr)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG1, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_ODR_MASK;
    tmp |= (uint8_t)odr;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_set_avgh(hts221_handle_t sensor, hts221_avgh_t avgh)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_AV_CONF_REG, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_AVGH_MASK;
    tmp |= (uint8_t)avgh;
    ret = hts221_write_one_reg(sensor, HTS221_AV_CONF_REG, tmp);
    return ret;
}

esp_err_t hts221_set_avgt(hts221_handle_t sensor, hts221_avgt_t avgt)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_AV_CONF_REG, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_AVGT_MASK;
    tmp |= (uint8_t)avgt;
    ret = hts221_write_one_reg(sensor, HTS221_AV_CONF_REG, tmp);
    return ret;
}

esp_err_t hts221_set_bdumode(hts221_handle_t sensor, hts221_state_t status)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG1, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_BDU_MASK;
    tmp |= ((uint8_t)status) << HTS221_BDU_BIT;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG1, tmp);
    return ret;
}

esp_err_t hts221_memory_boot(hts221_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG2, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp |= HTS221_BOOT_MASK;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_set_heaterstate(hts221_handle_t sensor, hts221_state_t status)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG2, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_HEATER_MASK;
    tmp |= ((uint8_t)status) << HTS221_HEATER_BIT;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_start_oneshot(hts221_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG2, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp |= HTS221_ONE_SHOT_MASK;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG2, tmp);
    return ret;
}

esp_err_t hts221_set_irq_activelevel(hts221_handle_t sensor, hts221_drdylevel_t value)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG3, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_DRDY_H_L_MASK;
    tmp |= (uint8_t)value;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_set_irq_outputtype(hts221_handle_t sensor, hts221_outputtype_t value)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG3, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_PP_OD_MASK;
    tmp |= (uint8_t)value;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_set_irq_enable(hts221_handle_t sensor, hts221_state_t status)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = hts221_read_one_reg(sensor, HTS221_CTRL_REG3, &tmp);
    if(ret == ESP_FAIL){
        return ret;
    }
    tmp &= ~HTS221_DRDY_MASK;
    tmp |= ((uint8_t)status) << HTS221_DRDY_BIT;
    ret = hts221_write_one_reg(sensor, HTS221_CTRL_REG3, tmp);
    return ret;
}

esp_err_t hts221_get_raw_humidity(hts221_handle_t sensor, int16_t *humidity)
{
    esp_err_t ret;
    uint8_t buffer[2];
    ret = hts221_read_reg(sensor, HTS221_HR_OUT_L_REG, 2, buffer);
    *humidity = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ret;
}

esp_err_t hts221_get_humidity(hts221_handle_t sensor, int16_t *humidity)
{
    int16_t h0_t0_out, h1_t0_out, h_t_out;
    int16_t h0_rh, h1_rh;
    uint8_t buffer[2];
    int32_t tmp_32;
    
    hts221_read_reg(sensor, HTS221_H0_RH_X2, 2, buffer);
    h0_rh = buffer[0] >> 1;
    h1_rh = buffer[1] >> 1;
    
    hts221_read_reg(sensor, HTS221_H0_T0_OUT_L, 2, buffer);
    h0_t0_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];

    hts221_read_reg(sensor, HTS221_H1_T0_OUT_L, 2, buffer);
    h1_t0_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    
    hts221_read_reg(sensor, HTS221_HR_OUT_L_REG, 2, buffer);
    h_t_out = (int16_t)(((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];
    
    tmp_32 = ((int32_t)(h_t_out - h0_t0_out)) * ((int32_t)(h1_rh - h0_rh) * 10);
    *humidity = tmp_32/(int32_t)(h1_t0_out - h0_t0_out)  + h0_rh * 10;
    if(*humidity > 1000) {
        *humidity = 1000;
    }
    return ESP_OK;
}

esp_err_t hts221_get_raw_temperature(hts221_handle_t sensor, int16_t *temperature)
{
    esp_err_t ret;
    uint8_t buffer[2];
    ret = hts221_read_reg(sensor, HTS221_TEMP_OUT_L_REG, 2, buffer);
    *temperature = (int16_t)((((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]);
    return ret;
}

esp_err_t hts221_get_temperature(hts221_handle_t sensor, int16_t *temperature)
{
    int16_t t0_out, t1_out, t_out, t0_degc_x8_u16, t1_degc_x8_u16;
    int16_t t0_degc, t1_degc;
    uint8_t buffer[4], tmp_8;
    uint32_t tmp_32;
    hts221_read_reg(sensor, HTS221_T0_DEGC_X8, 2, buffer);
    hts221_read_reg(sensor, HTS221_T0_T1_DEGC_H2, 1, &tmp_8);
    t0_degc_x8_u16 = (((uint16_t)(tmp_8 & 0x03)) << 8) | ((uint16_t)buffer[0]);  
    t1_degc_x8_u16 = (((uint16_t)(tmp_8 & 0x0C)) << 6) | ((uint16_t)buffer[1]);
    t0_degc = t0_degc_x8_u16 >> 3;
    t1_degc = t1_degc_x8_u16 >> 3;

    hts221_read_reg(sensor, HTS221_T0_OUT_L, 4, buffer);
    t0_out = (((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0];  
    t1_out = (((uint16_t)buffer[3]) << 8) | (uint16_t)buffer[2];

    hts221_read_reg(sensor, HTS221_TEMP_OUT_L_REG, 2, buffer);
    t_out = (((uint16_t)buffer[1]) << 8) | (uint16_t)buffer[0]; 

    tmp_32 = ((uint32_t)(t_out - t0_out)) * ((uint32_t)(t1_degc - t0_degc) * 10);
    *temperature = tmp_32 / (t1_out - t0_out) + t0_degc * 10;
    return ESP_OK;
}

hts221_handle_t sensor_hts221_create(i2c_bus_handle_t bus, uint16_t dev_addr, bool addr_10bit_en)
{
    hts221_dev_t* sensor = (hts221_dev_t*) calloc(1, sizeof(hts221_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    sensor->dev_addr_10bit_en = addr_10bit_en;
    return (hts221_handle_t) sensor;
}

esp_err_t sensor_hts221_delete(hts221_handle_t sensor, bool del_bus)
{
    hts221_dev_t* sens = (hts221_dev_t*) sensor;
    if(del_bus) {
        i2s_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

