idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    # Embed the server root certificate into the final binary
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)