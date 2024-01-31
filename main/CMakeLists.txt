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
    "tunein.c"
    "http_client.c"
    "sd_card_browser.c"
    
    "img/dlna_160x44.c"
    "img/tunein_160x44.c"
    "img/microsd_160x44.c"
    "img/radio_128x82.c"
    "img/tunein_refresh128x128.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(REQUIRES json)
set(COMPONENT_EMBED_TXTFILES ../logo.png)
register_component()