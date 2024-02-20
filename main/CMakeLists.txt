set(COMPONENT_SRCS 
    "img_download.c"
    "metadata.c" 
    "display.c" 
    "buttons.c" 
    "player.c"
    "dlna.c"
    "main.c"
    "hescape.c" 
    "metadata.c"
    "jpg_stream_reader.c"
    "gui/tunein_browser.c"
    "gui/camera_view.c"
    "http_client.c"
    "user_config.c"
    "gui/sd_card_browser.c"
    "gui/msg_window.c"
    "gui/config_panel.c"
    "gui/gui.c"
    
    "img/dlna_140x40.c"
    "img/tunein_140x40.c"
    "img/microsd_140x40.c"
    "img/radio_128x104.c"
    "img/tunein_refresh_128x128.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(REQUIRES json)
set(COMPONENT_EMBED_TXTFILES ../logo.png)
register_component()