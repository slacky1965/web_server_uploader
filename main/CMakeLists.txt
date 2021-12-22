idf_component_register(SRCS  "main.c"
                             "utils.c"
                             "http.c"
                INCLUDE_DIRS "include")

spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
                