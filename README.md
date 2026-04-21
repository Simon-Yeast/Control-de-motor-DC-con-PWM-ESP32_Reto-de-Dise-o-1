# Agitador de Muestras con ESP32 — Control de Motor DC

Implementación de un sistema embebido para el control de un agitador de laboratorio usando un motor DC de 12V. Permite ajustar la velocidad, seleccionar el sentido de giro y visualizar el porcentaje de potencia en displays de 7 segmentos. Desarrollado en C con ESP-IDF, sin librerías externas.

---

## Hardware requerido

| Componente | Cantidad |
|---|---|
| ESP32 DevKit | 1 |
| Motor DC 12V / 2A | 1 |
| MOSFET IRFZ44N (canal N) | 2 |
| MOSFET IRF9630 (canal P) | 2 |
| Optoacopladores 4N35 | 4 |
| Displays 7 segmentos (3 dígitos) | 1 |
| Potenciómetro 10kΩ | 1 |
| Pulsadores | 2 |
| LEDs (rojo y verde) | 2 |
| Resistencias y protoboard | — |

---

## Interfaz de usuario

Potenciómetro: control de velocidad (0–100%)
Pulsadores: selección de sentido de giro
LED verde: indica un sentido de giro
LED rojo: indica el sentido contrario
Displays: muestran el porcentaje de potencia aplicado

---

## Arquitectura del sistema
main.c
├── Inicialización de GPIO, ADC y PWM (LEDC)
├── Lectura ADC (potenciómetro) y escalamiento a porcentaje
├── Generación de señal PWM para control de velocidad
├── Lógica de control de dirección con protección
├── Multiplexación de displays de 7 segmentos
└── Loop principal con tareas concurrentes

---

## Asignación de pines

### Displays 7 segmentos
| GPIO | Segmento |
|------|----------|
| 13 | A |
| 12 | B |
| 14 | C |
| 27 | D |
| 26 | E |
| 25 | F |
| 33 | G |

### Selección de dígitos
| GPIO | Dígito |
|------|--------|
| 21 | Centenas |
| 22 | Decenas |
| 23 | Unidades |

### Entradas
| GPIO | Función |
|------|---------|
| 4 | Giro sentido A |
| 5 | Giro sentido B |

### Indicadores
| GPIO | Función |
|------|---------|
| 18 | LED verde |
| 19 | LED rojo |

### Etapa de potencia
| GPIO | Función |
|------|---------|
| 32 | High-side izquierdo |
| 16 | PWM bajo izquierdo |
| 17 | High-side derecho |
| 15 | PWM bajo derecho |

### ADC
| Canal | Función |
|-------|--------|
| ADC1_CH6 | Potenciómetro |

---

## Etapa de potencia
El control del motor se realiza mediante un puente H discreto:

High-side: MOSFET canal P IRF9630
Low-side: MOSFET canal N IRFZ44N
Aislamiento: optoacopladores 4N35

Los optoacopladores separan eléctricamente el ESP32 de la etapa de potencia, evitando daños por ruido, picos de voltaje o transientes generados por el motor de 12V.

---

**Notas de diseño**
El sistema evita la inversión instantánea del motor para prevenir sobrecorrientes y esfuerzos mecánicos. Se implementa una pausa antes de cambiar el sentido de giro.

El control de velocidad se realiza mediante PWM a 12 bits, permitiendo una regulación fina de la potencia aplicada.

La visualización se realiza por multiplexación de los tres displays, manteniendo una frecuencia suficiente para evitar parpadeo visible.


