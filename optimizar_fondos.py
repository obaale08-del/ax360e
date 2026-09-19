import os
from PIL import Image, ImageEnhance

INPUT_DIR = "static/backgrounds"
OUTPUT_DIR = "static/backgrounds/optimizadas"

os.makedirs(OUTPUT_DIR, exist_ok=True)

TARGET_WIDTH = 1080
TARGET_HEIGHT = 1920

def procesar_fondos():
    print("✨ Iniciando optimización de fondos para Petruska Engine...")
    
    if not os.path.exists(INPUT_DIR):
        print(f"❌ No se encuentra la carpeta {INPUT_DIR}")
        return

    for filename in os.listdir(INPUT_DIR):
        if filename.lower().endswith(('.png', '.jpg', '.jpeg', '.webp')):
            img_path = os.path.join(INPUT_DIR, filename)
            try:
                with Image.open(img_path) as img:
                    img = img.convert("RGB")
                    
                    img_ratio = img.width / img.height
                    target_ratio = TARGET_WIDTH / TARGET_HEIGHT
                    
                    if img_ratio > target_ratio:
                        new_height = TARGET_HEIGHT
                        new_width = int(new_height * img_ratio)
                    else:
                        new_width = TARGET_WIDTH
                        new_height = int(new_width / img_ratio)
                        
                    img_resized = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
                    
                    left = (new_width - TARGET_WIDTH) / 2
                    top = (new_height - TARGET_HEIGHT) / 2
                    right = (new_width + TARGET_WIDTH) / 2
                    bottom = (new_height + TARGET_HEIGHT) / 2
                    
                    img_cropped = img_resized.crop((left, top, right, bottom))
                    
                    enhancer_sharpness = ImageEnhance.Sharpness(img_cropped)
                    img_enhanced = enhancer_sharpness.enhance(1.4)
                    
                    enhancer_color = ImageEnhance.Color(img_enhanced)
                    img_final = enhancer_color.enhance(1.2)
                    
                    output_path = os.path.join(OUTPUT_DIR, filename)
                    img_final.save(output_path, "JPEG", quality=92)
                    print(f"✅ ¡Optimizado con éxito: {filename} -> Alta Nitidez!")
            except Exception as e:
                print(f"❌ Error procesando {filename}: {e}")

if __name__ == "__main__":
    procesar_fondos()
