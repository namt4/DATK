set(app_src ds1302.c)
set(pri_req)
idf_component_register(SRCS ${app_src}
                        INCLUDE_DIRS "."
                        REQUIRES ${pri_req})