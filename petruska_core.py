import os
import math

def calcular_entropia_petruska(ruta_archivo, sample_size=4096):
    """
    Núcleo de análisis de entropía de Shannon optimizado para entorno móvil.
    Realiza un muestreo de los primeros bytes (4KB por defecto) para 
    clasificar la densidad de información sin saturar el I/O del procesador.
    """
    try:
        tamanio = os.path.getsize(ruta_archivo)
        if tamanio == 0:
            return 0.0, 0
            
        with open(ruta_archivo, 'rb') as f:
            chunk = f.read(sample_size)
            
        if not chunk:
            return 0.0, tamanio
            
        frecuencias = {i: 0 for i in range(256)}
        for byte in chunk:
            frecuencias[byte] += 1
            
        entropia = 0.0
        chunk_len = len(chunk)
        for count in frecuencias.values():
            if count > 0:
                p_x = count / chunk_len
                entropia -= p_x * math.log2(p_x)
                
        return entropia, tamanio
        
    except Exception as e:
        return 0.0, 0

if __name__ == "__main__":
    print("Núcleo Petruska cargado correctamente en el nodo móvil.")
