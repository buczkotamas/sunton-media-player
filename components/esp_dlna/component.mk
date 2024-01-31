#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS :=  .

COMPONENT_EMBED_TXTFILES := xml/rootDesc.xml \
                            xml/devicelist.xml \
                            xml/rootDescLogo.xml \
                            xml/rootDescService.xml \      
                            xml/ConnectionManager.xml \
                            xml/RenderingControl.xml \
                            xml/EventLastChange.xml \
                            xml/EventProtocolCMR.xml \
                            ml/SoapResponseErr.xml \
                            xml/SoapResponseOk.xml 

COMPONENT_EMBED_FILES := xml/AVTransport.xml                          