#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include

ifdef CONFIG_ESP32_S3_SUNTON_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./sunton-esp32-8048S070
COMPONENT_SRCDIRS += ./sunton-esp32-8048S070
endif

ifdef CONFIG_ESP32_S3_DEVKIT_C_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./esp32-s3-devkitc-1-n16r8
COMPONENT_SRCDIRS += ./esp32-s3-devkitc-1-n16r8
endif
