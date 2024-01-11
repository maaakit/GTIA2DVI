import re


def hex_to_rgb888(hex_str):
    r = int(hex_str[0:2], 16)
    g = int(hex_str[2:4], 16)
    b = int(hex_str[4:6], 16)
    return (r, g, b)


def rgb888_to_rgb565(rgb888):
    r = rgb888[0] >> 3
    g = rgb888[1] >> 2
    b = rgb888[2] >> 3
    return ((r << 11) | (g << 5) | b)


def replace_hex_with_rgb565(text):
    hex_pattern = re.compile(r'0x([0-9a-fA-F]{6})')
    matches = hex_pattern.findall(text)
    for match in matches:
        rgb888 = hex_to_rgb888(match)
        rgb565 = rgb888_to_rgb565(rgb888)
        text = text.replace(match, f'{rgb565:04X}')
    return text


with open('input.txt', 'r') as input_file:
    text = input_file.read()
    text = replace_hex_with_rgb565(text)

with open('output.txt', 'w') as output_file:
    output_file.write(text)
