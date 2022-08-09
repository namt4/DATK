set(app_src sd_card_lib.c)
set(pri_req fatfs json bme680)
idf_component_register(SRCS ${app_src}
                        INCLUDE_DIRS "."
                        REQUIRES ${pri_req})