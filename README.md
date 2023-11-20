# Descripcion

El objetivo es implementar tres funciones relacionadas con el tiempo:

```c
void time_get(struct tm *); Obtiene la fecha y la hora actuales.
void time_set(struct tm *); Establece la fecha y la hora actuales.
void time_sleep(int); Detiene la ejecución por algunos segundos.
```

La estructura struct tm está definida como

```c
struct tm {
  u8 seconds;
  u8 minutes;
  u8 hours;
  u8 day;
  u8 month;
  u32 year;
};
```

Para obtener y establecer la fecha y hora actuales es necesario acceder al Real-Time Clock (RTC), de manera
que se debe implementar un driver para el mismo. La interfaz del driver debe de seguir la interfaz de
dispositivos de caracteres más reciente, es decir, aquella que sigue la interfaz de archivos. Cómo se
establecen o se consultan los datos queda a decisión de quien desarrolle el driver.
No sólo se puede usar el RTC, también se puede utilizar el Programmable Interval Timer (PIT). En caso de
utilizarse, deberá publicarse también como un dispositivo de caracteres con la nueva interfaz. Esta parte es opcional

La organización requerida será:

src/kernel/include/time.h Declaraciones de las funciones time\_\*() y struct tm, así como de
otras declaraciones públicas que encuentre útiles.

src/kernel/time.c Implementación principal de las funciones definidas en time.h.

src/kernel/include/rtc.h Declaraciones específicas del driver del RTC. En particular,

int rtc_init() ha de estar aquí.

src/kernel/drivers/rtc.c Implementación del driver del RTC.

src/kernel/include/pit.h Declaraciones específicas del driver del PIT. En particular,
int pit_init() ha de estar aquí.

src/kernel/drivers/pit.c Implementación del driver del PIT.

## Colaboraciones

Cree un `issue` o envíe un `pull request`

## Autores

Iván Galbán Smith <ivan.galban.smith@gmail.com>

Raydel E. Alonso Baryolo <raydelalonsobaryolo@gmail.com>

3rd year Computer Science students, University of Havana
