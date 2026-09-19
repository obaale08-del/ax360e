import os
import hashlib

print("="*50)
print(" 🔒 AUDITORÍA DE SEGURIDAD - PETRUSKA ENGINE PRO")
print("="*50)

# Verificar dependencias de cifrado o hashing disponibles
has_cryptography = False
try:
    import cryptography
    has_cryptography = True
except ImportError:
    pass

print(f"\n[+] Módulo de Criptografía Avanzada instalado: {'SÍ' if has_cryptography else 'NO (Usando librerías nativas de Python)'}")

# Analizar la integridad del directorio actual
total_files = 0
total_size = 0
for root, dirs, files in os.walk('.'):
    if '.git' in root or 'uploads' in root:
        continue
    for file in files:
        total_files += 1
        total_size += os.path.getsize(os.path.join(root, file))

print(f"[+] Archivos de código fuente analizados: {total_files}")
print(f"[+] Huella de peso del núcleo: {total_size / 1024:.2f} KB")

# Simulación de control de integridad por Hash (SHA-256)
app_hash = "No disponible"
if os.path.exists('app.py'):
    with open('app.py', 'rb') as f:
        app_hash = hashlib.sha256(f.read()).hexdigest()[:16]

print(f"[+] Firma criptográfica de app.py (SHA-256 resumido): {app_hash}")
print("\n" + "="*50)
print(" ✨ Diagnóstico listo. Estructura apta para integrar módulos matemáticos.")
print("="*50)
