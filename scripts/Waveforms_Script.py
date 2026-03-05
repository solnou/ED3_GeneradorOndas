import serial
import time
import numpy as np

# Configuración del puerto serie
ser = serial.Serial(
    port='COM4',          # Puerto COM
    baudrate=9600,        # Velocidad de transmisión
    timeout=1             # Tiempo de espera en segundos para la lectura del puerto serie
)

# Funciones para generar diferentes formas de onda
def onda_seno(samples):
    return (np.sin(np.linspace(0, 2 * np.pi, samples)) * 127 + 128).astype(np.uint8)

def onda_coseno(samples):
    return (np.cos(np.linspace(0, 2 * np.pi, samples)) * 127 + 128).astype(np.uint8)

def onda_cuadrada(samples):
    return (np.sign(np.sin(np.linspace(0, 2 * np.pi, samples))) * 127 + 128).astype(np.uint8)

def onda_triangular(samples):
    return (2 * np.abs(2 * (np.linspace(0, 1, samples) % 1) - 1) * 127).astype(np.uint8)

def onda_sierra(samples):
    return (2 * (np.linspace(0, 1, samples) % 1) * 127).astype(np.uint8)

def onda_PWM(samples, duty_cycle=0.1):
    return (np.where(np.linspace(0, 1, samples) % 1 < duty_cycle, 255, 0)).astype(np.uint8)

# ASCII art for the menu
def print_menu():
    print("\n\033[94mOpciones disponibles:\033[0m")
    print("""
    1. Seno
    2. Coseno
    3. Cuadrada
    4. Triangular
    5. Sierra
    6. PWM
    7. Salir
    """)

# Verificamos la conexión con el puerto serie
try:
    if ser.is_open:
        print(f"\n\033[92mConexión establecida en {ser.name}\033[0m")

        while True:
            print_menu()
            opcion = input("\nSeleccione una opción: ").strip()

            if opcion == '7':
                print("\n\033[93mSaliendo del programa.\033[0m")
                break

            samples = 60

            if opcion == "1":
                data_to_send = onda_seno(samples)
            elif opcion == "2":
                data_to_send = onda_coseno(samples)
            elif opcion == "3":
                data_to_send = onda_cuadrada(samples)
            elif opcion == "4":
                data_to_send = onda_triangular(samples)
            elif opcion == "5":
                data_to_send = onda_sierra(samples)
            elif opcion == "6":
                duty_cycle = float(input("Ingrese el ciclo de trabajo (0-1): "))
                data_to_send = onda_PWM(samples, duty_cycle)
            else:
                print("\033[91mOpción no válida\033[0m")
                continue

            # ENVIAMOS LOS DATOS UNO POR UNO
            for data in data_to_send:
                ser.write(bytearray([int(data)]))  # Convertimos cada valor a un byte
                print(f"\033[96mEnviando:\033[0m {data}")
                time.sleep(0.1)

except Exception as e:
    print(f"\033[91mError:\033[0m {str(e)}")

finally:
    # Cierra el puerto serie al finalizar
    if ser.is_open:
        ser.close()
        print(f"\n\033[92mConexión cerrada en {ser.name}\033[0m")