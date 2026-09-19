import os
from PIL import Image

DOWNLOAD_DIR = '/storage/emulated/0/Download'
TARGET_DIR = 'static/backgrounds'

# Limpiamos primero la carpeta de fondos para empezar completamente de cero con los 60 limpios
for f in os.listdir(TARGET_DIR):
    os.remove(os.path.join(TARGET_DIR, f))

print("🧹 Carpeta 'static/backgrounds' limpiada. Buscando únicamente los 60 paisajes de hoy...")

extensiones_validas = ('.jpg', '.jpeg', '.png', '.webp')
# Ordenamos los archivos por fecha de modificación (los más recientes primero)
archivos = sorted(
    [f for f in os.listdir(DOWNLOAD_DIR) if f.lower().endswith(extensiones_validas)],
    key=lambda x: os.path.getmtime(os.path.join(DOWNLOAD_DIR, x)),
    reverse=True
)

# Tomamos exactamente los primeros 60 archivos más recientes (que son los que descargaste hoy)
paisajes_hoy = archivos[:60]

print(f"📁 Seleccionados exactamente {len(paisajes_hoy)} archivos recientes de hoy.\n")

procesados = 0
for filename in paisajes_hoy:
    src_path = os.path.join(DOWNLOAD_DIR, filename)
    target_path = os.path.join(TARGET_DIR, filename)
    
    try:
        with Image.open(src_path) as img:
            width, height = img.size
            ratio = width / height
            
            # Adaptación inteligente a formato horizontal para el fondo
            if ratio > 1.1:
                img.thumbnail((1920, 1080))
                img_final = img
            elif 0.9 <= ratio <= 1.1:
                img_final = img.resize((1080, 1080))
            else:
                img_final = img.resize((int(1080 * ratio), 1080))
            
            img_final.convert('RGB').save(target_path, 'JPEG', quality=85)
            procesados += 1
            print(f"• Procesado y ajustado: {filename}")
            
    except Exception as e:
        print(f"❌ Error con {filename}: {e}")

print("\n" + "="*50)
print(f"✨ ¡LISTO! Exactamente {procesados} paisajes de fondo listos y ordenados.")
print("="*50)
if __name__ == '__main__':
    pass
