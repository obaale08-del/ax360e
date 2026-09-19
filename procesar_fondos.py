import os
from PIL import Image

# Ruta típica de descargas en Android (requiere haber ejecutado termux-setup-storage)
DOWNLOAD_DIR = '/storage/emulated/0/Download'
TARGET_DIR = 'static/backgrounds'

os.makedirs(TARGET_DIR, exist_ok=True)

def analizar_y_ajustar_fondos():
    print("="*60)
    print("🔍 INICIANDO ANALIZADOR Y AJUSTADOR DE PAISAJES...")
    print("="*60)
    
    if not os.path.exists(DOWNLOAD_DIR):
        print(f"❌ No se encuentra la ruta: {DOWNLOAD_DIR}")
        print("💡 Consejo: Asegúrate de ejecutar 'termux-setup-storage' en Termux para acceder a la galería.")
        return

    extensiones_validas = ('.jpg', '.jpeg', '.png', '.webp')
    archivos = [f for f in os.listdir(DOWNLOAD_DIR) if f.lower().endswith(extensiones_validas)]

    print(f"📁 Se encontraron {len(archivos)} imágenes en la carpeta Descargas.\n")
    
    procesadas = 0
    for filename in archivos:
        src_path = os.path.join(DOWNLOAD_DIR, filename)
        target_path = os.path.join(TARGET_DIR, filename)
        
        try:
            with Image.open(src_path) as img:
                width, height = img.size
                ratio = width / height
                
                print(f"• Imagen: {filename} | Resolución: {width}x{height} | Ratio: {ratio:.2f}")
                
                # Análisis de formato
                if ratio > 1.1:
                    print("  🟢 Estado: Formato Horizontal (Encaja perfecto como fondo).")
                    # Redimensionar optimizando peso si es muy grande, manteniendo aspecto
                    img.thumbnail((1920, 1080))
                    img_final = img
                elif 0.9 <= ratio <= 1.1:
                    print("  🟡 Estado: Formato Cuadrado. Adaptando a fondo...")
                    img_final = img.resize((1080, 1080))
                else:
                    print("  🟠 Estado: Formato Vertical (Retrato). Recortando inteligentemente a horizontal...")
                    # Si es vertical, la redimensionamos cubriendo los 1080p y recortamos los excesos
                    img_final = img.resize((int(1080 * ratio), 1080))
                
                # Guardar limpia en la carpeta static del proyecto
                img_final.convert('RGB').save(target_path, 'JPEG', quality=85)
                procesadas += 1
                
        except Exception as e:
            print(f"  ❌ Error procesando {filename}: {e}")

    print("\n" + "="*60)
    print(f"✨ ¡ÉXITO! Se han procesado y adaptado {procesadas} imágenes.")
    print(f"📂 Guardadas correctamente en '{TARGET_DIR}'. ¡Ya aparecerán en tu web!")
    print("="*60)

if __name__ == '__main__':
    analizar_y_ajustar_fondos()
