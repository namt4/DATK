set(app_src mqtt_app.c)
set(pri_req mqtt json)
idf_component_register(SRCS ${app_src}
                        INCLUDE_DIRS "."
                        REQUIRES ${pri_req})