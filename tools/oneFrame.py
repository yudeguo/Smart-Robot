from PIL import Image
import struct

def rgb888_to_rgb565(r, g, b):
    # 将 RGB888 转换为 RGB565
    r = (r >> 3) & 0x1F  # 高 5 位
    g = (g >> 2) & 0x3F  # 高 6 位
    b = (b >> 3) & 0x1F  # 高 5 位
    return (r << 11) | (g << 5) | b

def convert_jpg_to_rgb565(jpg_path, bin_path):
    # 打开 JPG 图片
    img = Image.open(jpg_path)
    
    # 获取图像的宽度和高度
    width, height = img.size
    
    # 获取图像的像素数据
    pixels = list(img.getdata())  # 获取所有像素数据，返回一个像素值列表
    
    # 初始化一个空的列表来保存 RGB565 数据
    rgb565_data = bytearray()
    
    # 按像素遍历并转换为 RGB565 格式
    for r, g, b in pixels:
        rgb565_pixel = rgb888_to_rgb565(r, g, b)
        
        # 将 RGB565 数据转换为大端字节序（MSB First）
        rgb565_data.extend(struct.pack('>H', rgb565_pixel))
    
    # 打开并写入文件，仅写入 RGB565 像素数据
    with open(bin_path, 'wb') as f:
        f.write(rgb565_data)  # 写入 RGB565 像素数据

    print(f"转换完成：{bin_path}")

# 使用示例
convert_jpg_to_rgb565('input.jpeg', 'image.bin')

