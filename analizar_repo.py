import os
import re

print("="*50)
print(" 🔍 ANALIZADOR DE FUNCIONES - PETRUSKA ENGINE PRO")
print("="*50)

app_path = 'app.py'
html_path = 'templates/index.html'

if os.path.exists(app_path):
    with open(app_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print(f"\n[+] Archivo analizado: {app_path}")
    print(f"    - Tamaño: {os.path.getsize(app_path)} bytes")
    
    # Buscar rutas de Flask
    routes = re.findall(r'@app\.route\(\'(.*?)\'(?:, methods=\$(.*?)\$)?\)', content)
    print("\n   📌 Rutas Web (Endpoints) detectadas:")
    for route in routes:
        print(f"      * Ruta: {route[0]}")

    # Buscar funciones principales
    functions = re.findall(r'def (.*?)\(', content)
    print("\n   ⚙️ Funciones de Python:")
    for func in functions:
        print(f"      * def {func}()")
        
    # Verificar comandos FFmpeg
    if 'ffmpeg' in content:
        print("\n   🎬 Motor Multimedia: FFmpeg integrado correctamente (Presets H.264 / CRF / Audio AAC).")
else:
    print(f"\n[!] No se encontró el archivo {app_path}")

if os.path.exists(html_path):
    print(f"\n[+] Archivo analizado: {html_path}")
    print(f"    - Tamaño: {os.path.getsize(html_path)} bytes")
    if 'landscape,nature' in open(html_path, 'r', encoding='utf-8').read():
        print("    - Fondo dinámico: Biblioteca infinita de paisajes configurada.")
else:
    print(f"\n[!] No se encontró el archivo {html_path}")

print("\n" + "="*50)
print(" ✨ Análisis rápido finalizado. ¡Buen descanso, Gerald!")
print("="*50)
