set(app_src real_time_app.c)
set(pri_req ds1302)
idf_component_register(SRCS ${app_src}
                        INCLUDE_DIRS "."
                        REQUIRES ${pri_req})