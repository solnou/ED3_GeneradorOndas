# LPC1769 Waveform Generator

Proyecto desarrollado para la materia **Electrónica Digital III** 2024.

Este sistema implementa un **generador de formas de onda programable** utilizando la placa de desarrollo **LPC1769**. La señal es definida externamente mediante un script en Python y transmitida al microcontrolador a través de comunicación **UART**. Posteriormente, el microcontrolador reproduce la señal utilizando su **DAC** con transferencia de datos mediante **DMA**, permitiendo una generación eficiente y continua de la forma de onda.

---

## Características del sistema

El sistema integra múltiples periféricos del microcontrolador LPC1769:

- **UART** → recepción de muestras de la señal desde una PC  
- **DMA** → transferencia eficiente de muestras al DAC  
- **DAC** → generación analógica de la forma de onda  
- **ADC** → lectura de un potenciómetro para ajustar la frecuencia  
- **TIMER0** → control del período de muestreo del ADC  
- **I2C** → comunicación con pantalla LCD  
- **LEDs** → indicadores de estado de recepción de datos

---

## Funcionamiento general

1. Un **script en Python** genera una forma de onda (senoidal, triangular, cuadrada, etc.).
2. Las muestras de la señal se envían por **UART** hacia la LPC1769.
3. El microcontrolador guarda las muestras en memoria.
4. Mediante **DMA**, las muestras se transfieren continuamente al **DAC**.
5. El **potenciómetro** conectado al ADC permite modificar la **frecuencia de salida**.
6. El valor de frecuencia se muestra en una **pantalla LCD conectada por I2C**.
7. Los **LEDs** indican el estado de recepción de datos por UART.

---

## Estructura del repositorio

ED3_GeneradorOndas
│

├── srcs/ # Código en C para el LPC1769

├── scripts/ # Script Python para generar formas de onda

├── docs/ # Informe y diagramas del proyecto

└── demo/ # Video de demostración

---

## Requisitos

### Hardware

- Placa de desarrollo **LPC1769**
- Potenciómetro
- Pantalla **LCD I2C**
- LEDs de estado
- Conexión UART con PC

### Software

- Compilador ARM (Keil / GCC ARM)
- Python 3
- Librerías Python estándar

  
