idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                      REQUIRES esp_driver_gpio
                                 esp_adc
                                 onewire_bus
                                 ds18b20
                                 esp_driver_i2c)
