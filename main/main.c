#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>  
#include <onewire_bus.h>
#include <ds18b20.h>
#include <driver/i2c_master.h>  

#define Wtemp_pin GPIO_NUM_4
#define EC_pin GPIO_NUM_32
#define Relay_pin GPIO_NUM_26
#define BMP280_sda_pin GPIO_NUM_21
#define BMP280_scl_pin GPIO_NUM_22


static const char *TAG = "HYDRO";

adc_oneshot_unit_handle_t adc_handle;
onewire_bus_handle_t onewire_bus_handle;
ds18b20_device_handle_t ds18b20_handle;
i2c_master_bus_handle_t i2c_bus_handle;
i2c_master_dev_handle_t bmp280_handle;
uint16_t dig_T1;
int16_t dig_T2;
int16_t dig_T3;

void gpio_pin_init(void)
{
    gpio_config_t relay = {
        .pin_bit_mask = (1ULL << Relay_pin),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&relay);
}


void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &chan_cfg);

}

float readTDS(float currentTemp)
{
    if (currentTemp == -127.0 || currentTemp == 85.0) 
    {
        ESP_LOGE(TAG, "TDS skip: bad water temp");
        return -1.0;
    }
    int raw = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &raw);
    float voltage = (raw / 4095.0) * 3.3;
    float compen_coeff = 1.0 + 0.02 * (currentTemp - 25);
    float compen_volt = voltage / compen_coeff;
    float ec = (133.42 * pow(compen_volt, 3)
              - 255.86 * pow(compen_volt, 2)
              + 857.39 * compen_volt);
    float tds = ec * 0.5;
    return (tds); 
}

void ds18b20_init(void)
{
    onewire_bus_config_t bus_cfg = {
        .bus_gpio_num = Wtemp_pin,
    };

    onewire_bus_rmt_config_t rmt_cfg = {
        .max_rx_bytes = 10,
    };
    onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &onewire_bus_handle);

    onewire_device_iter_handle_t iter = NULL;
    onewire_new_device_iter(onewire_bus_handle, &iter);

    onewire_device_t device;
    esp_err_t result = ESP_OK;

    while (result == ESP_OK)
    {
        result = onewire_device_iter_get_next(iter, &device);
        if(result == ESP_OK)
        {
            ds18b20_config_t ds_config = {};
            if (ds18b20_new_device_from_enumeration(&device, &ds_config, &ds18b20_handle) == ESP_OK)
            {
                ESP_LOGI(TAG, "DS18B20 found");
                break;
            }
        }
       
    }
    onewire_del_device_iter(iter);
}

float readWaterTemp(void)
{
    float temp = 0;
    
    ds18b20_trigger_temperature_conversion(ds18b20_handle);
    vTaskDelay(pdMS_TO_TICKS(750));

    ds18b20_get_temperature(ds18b20_handle, &temp);
    if (temp == -127.0 || temp == 85.0)
    {
        ESP_LOGE(TAG, "water sensor error");
        return (-127.0);
    }
    return (temp);
}

void i2c_init(void)
{
    i2c_master_bus_config_t i2c_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BMP280_scl_pin,
        .sda_io_num = BMP280_sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    if(i2c_new_master_bus(&i2c_config, &i2c_bus_handle) == ESP_OK)
    {
        ESP_LOGI(TAG, "I2C intial success!");
    }
    else
    {
        ESP_LOGE(TAG, "I2C intial error");
    }
}

void bmp280_init(void)
{
    i2c_device_config_t dev_confg = {
        .device_address = 0x76,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    i2c_master_bus_add_device(i2c_bus_handle, &dev_confg, &bmp280_handle);

    uint8_t reg = 0x88;
    uint8_t calib[6];
    i2c_master_transmit_receive(bmp280_handle, &reg, 1, calib, 6, -1);

    dig_T1 = (calib[1] << 8 | calib[0]);
    dig_T2 = (calib[3] << 8 | calib[2]);
    dig_T3 = (calib[5] << 8 | calib[4]);

    ESP_LOGI(TAG, "BMP280 ready. Calibration T1%u, T2%d, T3%d", dig_T1, dig_T2, dig_T3);
}

float readBMP_airTemp(void)
{
    uint8_t cmd[2] = {0xF4, 0x21};
    i2c_master_transmit(bmp280_handle, cmd, 2, -1);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t reg = 0xFA;
    uint8_t data[3];
    i2c_master_transmit_receive(bmp280_handle, &reg, 1, data, 3, -1);

    int32_t adc_T = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);

    int32_t var1, var2, t_fine, T;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    //T = (t_fine * 5 + 128) >> 8;
    float result = ((t_fine * 5 + 128) >> 8) / 100.0f;
    if (result < -40.0f || result > 85.0f)
    {
        ESP_LOGE(TAG, "BMP280 air temp error");
        return -999.0f;
    }
    return (result);
}

void app_main(void)
{

    while (1)
    {
        
        
    }
    
}