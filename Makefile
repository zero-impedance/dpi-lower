all : dpi-lower.bin

dpi-lower.bin : dpi-lower.c
	$(CC) -o $@ $< -lX11 -lXi
