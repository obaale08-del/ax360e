import os
import subprocess

print("=" * 50)
print(" 🔬 DEPURADOR DE FFMPEG - PETRUSKA ENGINE ")
print("=" * 50)

# Buscar un archivo en uploads
uploads = os.listdir('uploads') if os.path.exists('uploads') else []
video_files = [f for f in uploads if f.lower().endswith(('mp4', '3gp', 'mkv', 'mov', 'webm'))]

if not video_files:
    print("❌ No hay vídeos en la carpeta /uploads para probar.")
    exit()

test_file = video_files[0]
input_path = os.path.join('uploads', test_file)
output_path = os.path.join('optimizadas', f"test_{test_file}")

print(f"📁 Archivo de prueba seleccionado: {test_file}")
print(f"📦 Tamaño original: {os.path.getsize(input_path) / (1024*1024):.2f} MB")

# Ejecutar FFmpeg mostrando la salida completa (sin silenciar errores)
cmd = ['ffmpeg', '-y', '-i', input_path, '-vf', 'scale=-2:480', '-crf', '32', '-preset', 'fast', output_path]

print("\n🚀 Ejecutando comando FFmpeg...")
resultado = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

print(f"🔹 Código de salida de FFmpeg: {resultado.returncode}")

if resultado.returncode == 0:
    print("✅ ¡Compresión exitosa!")
    print(f"📦 Nuevo tamaño: {os.path.getsize(output_path) / (1024*1024):.2f} MB")
else:
    print("❌ FFmpeg ha fallado. Aquí está el error exacto que dio:")
    print(resultado.stderr.decode('utf-8', errors='ignore'))

print("=" * 50)
