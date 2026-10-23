idf_component_register(
    SRCS "main.c" "ssid_name.c" "stream_hub.c" "wifi_ap.c" "dns_captive.c" "http_srv.c" "uart_link.c" "status_led.c"
         # Shared verbatim with the FAP rather than reimplemented here: one
         # codec means the two halves of the link cannot drift apart, and the
         # host tests that cover it cover both sides.
         "../../radio_proto.c"
    INCLUDE_DIRS "." "../.."
    EMBED_FILES "web/index.html"
)
