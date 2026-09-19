# 🧠 Motor Symbio: Especificación Algorítmica y Bases de Arquitectura

## 1. Fundamentación del Problema y Enfoque de Diseño
El sistema nace de la necesidad de procesar volúmenes masivos de datos multimedia (reduciendo gigabytes críticos de almacenamiento) en un entorno con restricciones severas de hardware (dispositivos móviles ejecutando Termux/Android). 
* **Base lógica**: En lugar de depender de una transcodificación ciega por software que saturaría los núcleos y colapsaría el sistema operativo, se diseñó un **modelo de abstracción de hardware con conmutación dinámica**.

## 2. Algoritmo de Decisión y Tolerancia a Fallos (Doble Capa)
El núcleo no ejecuta un flujo lineal simple; implementa una **máquina de estados condicional** basada en el éxito de la ejecución a nivel de núcleo del sistema:

1. **Fase de Captura y Filtrado (Idempotencia)**:
   * El algoritmo evalúa primero el estado del sistema de archivos mediante una función de comprobación de existencia. Si el archivo de salida ya fue procesado, el sistema aplica una exclusión inmediata (`continue`), garantizando que el algoritmo sea idempotente y evite ciclos redundantes.
2. **Fase de Intento Primario (Aceleración por Silicio - Hardware Layer)**:
   * El sistema intenta desviar la carga computacional hacia el decodificador/codificador de hardware nativo (`h264_mediacodec`). 
   * *Base algorítmica*: Se fuerza un límite estricto de tasa de bits (`-b:v 2000k`) y tasa de cuadros (`-r 60`) para estandarizar la entropía visual y reducir el espacio de manera predecible.
3. **Fase de Evaluación de Estado (Exit Code Evaluation)**:
   * El script intercepta el código de salida del proceso (`$?`). 
   * Si el código es `0` y el archivo existe, el algoritmo valida el éxito y cierra el ciclo.
   * Si el código de salida indica fallo (muy común en entornos móviles donde los drivers de MediaCodec rechazan ciertos contenedores), se activa una **interrupción controlada** que redirige el flujo hacia la capa secundaria.
4. **Fase de Respaldo por Software (Software Fallback Layer)**:
   * Ante el fallo del hardware, entra en juego el codificador universal `libx264`.
   * *Base algorítmica*: Se optimiza la velocidad mediante el preset `ultrafast` para evitar el bloqueo del procesador, combinado con un Factor de Compresión Constante (`crf 28`). Matemáticamente, el CRF 28 busca el punto de equilibrio exacto (umbral psicovisual) donde la pérdida de datos es imperceptible para el ojo humano pero drástica para el peso del archivo.

## 3. Algoritmo de Protección Térmica y Gestión de Ciclos (Throttling Control)
* Durante cargas de trabajo masivas, los procesadores móviles sufren de *thermal throttling* (reducción de velocidad por sobrecalentamiento).
* **Solución algorítmica**: Se diseñó un mecanismo de **pausa dinámica por intervalos** (`sleep 3`) al finalizar cada iteración. Esto obliga al procesador a liberar ciclos de reloj y disipar temperatura de forma controlada antes de inyectar el siguiente flujo de vídeo, protegiendo la integridad física del dispositivo.

## 4. Estandarización y Aislamiento de Canales (Audio & Stream Separation)
* El sistema separa de forma independiente el flujo de vídeo y el flujo de audio para evitar conflictos de multiplexación.
* El canal de audio es remapeado de forma universal al códec `aac` con una tasa fija de `128k`, eliminando el sobrepeso de las pistas de audio originales sin sacrificar la claridad acústica.
