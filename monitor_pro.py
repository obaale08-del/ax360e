import os
import time
import requests
import platform
import subprocess

TARGET_URL = "http://192.168.1.133:5000"

print("=" * 60)
print(" 🛡️ MONITOR DE SEGURIDAD, ESTABILIDAD Y FUNCIONAMIENTO ")
print(" 🚀 Petruska Engine Pro - Diagnóstico en Vivo ")
print("=" * 60)

# 1. Auditoría de Hardware y Estabilidad del Servidor
print(f"\n[+] 1. ESTABILIDAD Y HARDWARE (Android / Termux)")
print(f" - Núcleos de CPU activos: {os.cpu_count()}")
print(f" - Sistema Operativo: {platform.system()} {platform.release()}")

carpetas = ['uploads', 'optimizadas', 'envios', 'vault_cifrada']
for c in carpetas:
    if os.path.exists(c):
        size_mb = sum(os.path.getsize(os.path.join(c, f)) for f in os.listdir(c) if os.path.isfile(os.path.join(c, f))) / (1024*1024)
        print(f" 📁 Carpeta /{c}: {len(os.listdir(c))} archivos | Ocupa: {size_mb:.2f} MB")
    else:
        print(f" 📁 Carpeta /{c}: ❌ No encontrada")

# 2. Auditoría de Seguridad de Cabeceras HTTP (Flask Server)
print(f"\n[+] 2. SEGURIDAD DE CABECERAS HTTP")
try:
    response = requests.get(TARGET_URL, timeout=3)
    headers = response.headers
    
    seguridad_checks = {
        'X-Content-Type-Options': headers.get('X-Content-Type-Options'),
        'X-Frame-Options': headers.get('X-Frame-Options'),
        'X-XSS-Protection': headers.get('X-XSS-Protection')
    }
    
    for header, val in seguridad_checks.items():
        if val:
            print(f" ✅ {header}: {val} (Protegido)")
        else:
            print(f" ⚠️ {header}: No detectada")
except Exception as e:
    print(f" ❌ No se pudo conectar al servidor Flask en {TARGET_URL}. ({e})")

# 3. Prueba de Rendimiento y Funcionamiento de FFmpeg
print(f"\n[+] 3. FUNCIONAMIENTO DEL MOTOR DE VÍDEO (FFmpeg)")
uploads = os.listdir('uploads') if os.path.exists('uploads') else []
vids = [f for f in uploads if f.lower().endswith(('.mp4', '.3gp', '.mkv', '.mov', '.webm'))]

if vids:
    test_v = vids[0]
    in_p = os.path.join('uploads', test_v)
    out_p = os.path.join('optimizadas', f"audit_{test_v}")
    
    t_start = time.time()
    cmd = ['ffmpeg', '-y', '-i', in_p, '-vf', 'scale=-2:480', '-crf', '32', '-preset', 'ultrafast', out_p]
    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t_end = time.time()
    
    if res.returncode == 0:
        orig_sz = os.path.getsize(in_p) / (1024*1024)
        new_sz = os.path.getsize(out_p) / (1024*1024)
        print(f" ✅ Compresión de vídeo OK en {t_end - t_start:.2f} segundos.")
        print(f"    Reducción: {orig_sz:.2f} MB ➡️ {new_sz:.2f} MB (Ahorro del {((1 - new_sz/orig_sz)*100):.1f}%)")
    else:
        print(" ❌ Fallo en la prueba de compresión con FFmpeg.")
else:
    print(" ℹ️ No hay vídeos en /uploads para realizar la prueba.")

print("=" * 60)
print(" 🟢 Diagnóstico completado.")
print("=" * 60)
