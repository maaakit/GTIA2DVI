import os

import png


def encode_rgb565(r, g, b):
    # Convert the red, green, and blue values to 16-bit integers
    r = int(r * 31 / 255)
    g = int(g * 63 / 255)
    b = int(b * 31 / 255)

    # Pack the red, green, and blue values into a single 16-bit integer
    return (r << 11) | (g << 5) | b


def save_hex_bytes(filename, varname, data):
    # Split the data into a list of words
    # words = []
    # for i in range(0, len(data), 2):
    #     words.append((data[i] << 8) | data[i + 1])

    # Save the words as a C header file
    with open(filename, 'w') as f:
        f.write(f'uint8_t {varname}[] = ')
        f.write('{\n')
        i = 0
        for byte in data:
            f.write(f' 0x{byte:02x}')
            if (i+1) < len(data):
                f.write(',')
            if (i + 1) % 16 == 0:
                f.write('\n')

            i = i+1
        f.write('};\n')


def toarray(color_palette):
    pal = []
    for p in color_palette:
        r, g, b = p
        pal.append(r)
        pal.append(g)
        pal.append(b)
    return pal


def encode_file(filename, palette):
    with open(filename + '.png', 'rb') as f:
        # Read the PNG file
        reader = png.Reader(file=f)

        # Decode the image data
        image_data = reader.read()

        # Extract the image dimensions and pixel data
        (width, height, pixels, metadata) = image_data

        # Extract the color palette from the metadata
        color_palette = metadata['palette']

        pad_pixels = int((400 - width) / 2)

        # Encode the pixels in the RGB565 format
        rgb565_pixels = []
        for row in pixels:

            pad_pixels_array(pad_pixels, rgb565_pixels)

            for pixel in row:
                # Look up the RGB values in the color palette
                if palette:
                    r, g, b = color_palette[pixel]
                    rgb565 = encode_rgb565(r, g, b)
                    # Convert the 16-bit integer to two 8-bit bytes and append them to the list
                    rgb565_pixels.append((rgb565 >> 8) & 0xff)
                    rgb565_pixels.append(rgb565 & 0xff)
                else:
                    rgb565_pixels.append(pixel & 0xff)

            pad_pixels_array(pad_pixels, rgb565_pixels)

    if palette:
        save_hex_bytes(filename + '_palette.h', 'palette', toarray(color_palette))
        save_hex_bytes(filename + '_image.h', 'framebuf', rgb565_pixels)
    else:
        save_hex_bytes(filename + '_rgb565.h', 'framebuf', rgb565_pixels)


def pad_pixels_array(pad_size, pixel_array):
    for x in range(pad_size):
        pixel_array.append(0)
        pixel_array.append(0)


# Get a list of all files in the current directory
files = os.listdir()

# Filter the list to only include PNG files
png_files = [f for f in files if f.endswith('.png')]

# Extract the names (without extensions) of the PNG files
png_names = [os.path.splitext(f)[0] for f in png_files]

print(png_names)

for f in png_names:
    encode_file(f, True)
