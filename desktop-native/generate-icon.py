from PIL import Image, ImageDraw, ImageFont
import sys

size = 256
image = Image.new("RGBA", (size, size), "#17110d")
draw = ImageDraw.Draw(image)
for inset in range(34):
    shade = int(23 + inset * 0.75)
    draw.rounded_rectangle((inset, inset, size - inset - 1, size - inset - 1), radius=max(4, 34 - inset), outline=(shade + 28, shade + 13, shade, 255))
draw.rounded_rectangle((15, 15, 240, 240), radius=25, outline="#b18b55", width=5)
draw.line((42, 61, 214, 61), fill="#6d5033", width=3)
draw.line((42, 195, 214, 195), fill="#6d5033", width=3)
draw.ellipse((52, 52, 204, 204), fill="#7b291f", outline="#d0a464", width=6)
draw.ellipse((66, 66, 190, 190), outline="#e0bd7e", width=2)

font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
font = ImageFont.truetype(font_path, 66)
text = "AN"
bounds = draw.textbbox((0, 0), text, font=font)
width = bounds[2] - bounds[0]
draw.text(((size - width) / 2 - 2, 93), text, fill="#f0d39b", font=font)
draw.line((91, 166, 165, 166), fill="#e0bd7e", width=3)

image.save(sys.argv[1], format="ICO", sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
