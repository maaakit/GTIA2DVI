# Replace TMDS with 10 bit UART (same baud rate):
# add_definitions(-DDVI_SERIAL_DEBUG=1)
# add_definitions(-DRUN_FROM_CRYSTAL)

add_executable(gtia2dvi
	main.c
)

pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_luma_ng.pio)
pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/gtia_chroma_ng.pio)
pico_generate_pio_header(gtia2dvi ${CMAKE_CURRENT_LIST_DIR}/pio/uart_8n1_tx.pio)

target_compile_definitions(gtia2dvi PRIVATE
	DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
	)

target_link_libraries(gtia2dvi 
	pico_stdlib
	pico_multicore
	hardware_flash
	libdvi
)
# create map/bin/hex file etc.
pico_add_extra_outputs(gtia2dvi)

#pico_set_binary_type(gtia2dvi no_flash)
