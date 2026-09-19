import os
import requests
import socket

def analizar_seguridad_avanzada():
    print("=" * 60)
    print("🛡️  PETRUSKA ENGINE - ANALIZADOR DE AVANCES EN SEGURIDAD  🛡️")
    print("=" * 60)
    
    puntuacion = 100
    alertas = []
    logros = []
    
    # 1. Integridad de archivos críticos y bóveda cifrada
    print("\n[1/5] Analizando estructura y Bóveda Cifrada...")
    if os.path.exists("vault_cifrada"):
        print(" [OK] Bóveda de archivos cifrados ('vault_cifrada') activa.")
        logros.append("Cifrado binario en bóveda habilitado.")
    else:
        print(" [!] Advertencia: No se encontró la carpeta 'vault_cifrada'.")
        puntuacion -= 15

    if os.path.exists("app.py"):
        with open("app.py", "r", encoding="utf-8") as f:
            contenido_app = f.read()
            if "hashlib.sha256" in contenido_app:
                print(" [OK] Autenticación mediante Hash SHA-256 detectada.")
                logros.append("Contraseña protegida con SHA-256.")
            else:
                print(" [!] Contraseña en texto plano o sin hash seguro.")
                puntuacion -= 20
                
            if "os.path.basename" in contenido_app:
                print(" [OK] Protección anti Path Traversal implementada.")
                logros.append("Sanitización de nombres de archivo activa.")
            else:
                print(" [!] Falta protección contra Path Traversal.")
                puntuacion -= 10
    else:
        print(" [X] Error crítico: No se encuentra 'app.py'.")
        puntuacion -= 30

    # 2. Verificación de servicio en puerto 5000
    print("\n[2/5] Comprobando disponibilidad del servidor...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        resultado = s.connect_ex(('127.0.0.1', 5000))
        s.close()
        if resultado == 0:
            print(" [OK] Servidor Flask respondiendo en puerto 5000.")
        else:
            print(" [!] El servidor no está corriendo en el puerto 5000.")
            alertas.append("Servidor apagado (enciéndelo para verificar HTTP).")
            puntuacion -= 15
    except Exception as e:
        print(f" [!] Excepción al comprobar puerto: {e}")

    # 3. Evaluación de Cabeceras HTTP Defensivas
    print("\n[3/5] Analizando cabeceras de seguridad HTTP...")
    try:
        response = requests.get("http://127.0.0.1:5000/", allow_redirects=True, timeout=3)
        headers = response.headers
        
        cabeceras_encontradas = 0
        if 'X-Content-Type-Options' in headers:
            cabeceras_encontradas += 1
        if 'X-Frame-Options' in headers:
            cabeceras_encontradas += 1
        if 'X-XSS-Protection' in headers:
            cabeceras_encontradas += 1
            
        if cabeceras_encontradas >= 2:
            print(f" [OK] Cabeceras de defensa HTTP activas ({cabeceras_encontradas}/3 detectadas).")
            logros.append("Cabeceras HTTP contra ataques XSS y Clickjacking.")
        else:
            print(" [!] Faltan cabeceras de seguridad HTTP avanzadas.")
            alertas.append("Cabeceras defensivas incompletas.")
            puntuacion -= 10
            
        if "/login" in response.url or response.status_code in [302, 401]:
            print(" [OK] Control de acceso por sesión verificado (Redirección a login).")
            logros.append("Control de sesiones obligatorio.")
        else:
            print(" [!] La ruta principal permite acceso libre sin sesión.")
            alertas.append("Ruta principal accesible sin login.")
            puntuacion -= 20
            
    except requests.exceptions.ConnectionError:
        print(" [!] No se pudo conectar vía HTTP (enciende app.py primero).")
        puntuacion -= 15

    # 4. Verificación de modo Debug
    print("\n[4/5] Comprobando modo de ejecución (Debug)...")
    if os.path.exists("app.py"):
        if "debug=False" in contenido_app:
            print(" [OK] Modo Debug desactivado de forma segura (producción local).")
            logros.append("Modo Debug apagado (Evita fugas de traza).")
        else:
            print(" [!] Cuidado: El modo Debug podría estar activo.")
            alertas.append("Modo debug encendido.")
            puntuacion -= 10

    # 5. Entorno Termux
    print("\n[5/5] Analizando entorno...")
    if "TERMUX_VERSION" in os.environ or os.path.exists("/data/data/com.termux"):
        print(" [INFO] Entorno Termux / Android confirmado.")
    else:
        print(" [INFO] Entorno estándar de escritorio/servidor.")

    # Resultado y Puntuación Analítica
    print("\n" + "=" * 60)
    print(f"📊 PUNTUACIÓN ANALÍTICA DE SEGURIDAD: {puntuacion}/100")
    print("=" * 60)
    
    if logros:
        print("✨ Logros de seguridad activos:")
        for l in logros:
            print(f"  ✔ {l}")
            
    if alertas:
        print("\n⚠️ Advertencias a revisar:")
        for a in alertas:
            print(f"  ✘ {a}")
            
    print("=" * 60)

if __name__ == "__main__":
    analizar_seguridad_avanzada()
