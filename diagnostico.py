import os
import platform

print("=" * 50)
print(" 🔍 INFORME DE ESTADO Y CONTEXTO - PETRUSKA ENGINE ")
print("=" * 50)

# 1. Entorno y Hardware del Servidor
print(f"\n[+] Entorno del Servidor:")
print(f" - Sistema Operativo: {platform.system()} {platform.release()}")
print(f" - Arquitectura: {platform.machine()}")
print(f" - Núcleos de CPU (Servidor): {os.cpu_count()}")

# 2. Verificar Herramientas del Sistema (FFmpeg)
print(f"\n[+] Herramientas Multimedia:")
ffmpeg_check = os.system("ffmpeg -version > /dev/null 2>&1")
if ffmpeg_check == 0:
    print(" - FFmpeg: ✅ Instalado y disponible para comprimir vídeos.")
else:
    print(" - FFmpeg: ❌ NO detectado (Ejecuta 'pkg install ffmpeg -y').")

# 3. Estado de Carpetas y Almacenamiento
carpetas = ['uploads', 'optimizadas', 'envios', 'vault_cifrada', 'static/backgrounds']
print(f"\n[+] Estado de Directorios y Archivos:")

for carpeta in carpetas:
    if os.path.exists(carpeta):
        archivos = os.listdir(carpeta)
        total_mb = sum(os.path.getsize(os.path.join(carpeta, f)) for f in archivos if os.path.isfile(os.path.join(carpeta, f))) / (1024 * 1024)
        print(f"📁 /{carpeta} -> {len(archivos)} archivo(s) | Ocupa: {total_mb:.2f} MB")
        for f in archivos[:5]: # Mostrar hasta los primeros 5 archivos de ejemplo
            print(f"    - {f}")
        if len(archivos) > 5:
            print(f"    ... y {len(archivos) - 5} archivos más.")
    else:
        print(f"📁 /{carpeta} -> ❌ No existe (se creará automáticamente).")

print("\n" + "=" * 50)
print(" Resumen conceptual de tu plataforma:")
print(" 1. /uploads: Archivos originales que subes desde la tablet.")
print(" 2. /optimizadas: Versión Ultrareducida adaptada localmente (-90%).")
print(" 3. /envios: Versión equilibrada pensada para compartir por mensajería (-60%).")
print(" 4. /vault_cifrada: Copia cifrada de seguridad mediante XOR (0x5A).")
print("=" * 50)
