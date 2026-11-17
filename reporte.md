# Juan José Morera Gómez 202517169, Frank Leonardo Silva Castillo 202517614
**1\. Preparar el repositorio y el entorno**  
![][image1]  
![][image2]  
GCC RISC-V: 11.4.0 (Ubuntu 11.4.0-1ubuntu1\~22.04)  
QEMU: 6.2.0  
Commit de partida: fa2ddcb7ca87a77af579380a6eafd5aff163b44f  
Versión de git: 2.34.1  
Versión de GNU: 4\.3  
![][image3]  
la versión de QEMU del repositorio de Ubuntu 22.04 (6.2.0) no cumple el requisito del Makefile (≥ 7.2) entonces descargamos la última versión de QEMU  
![][image4]  
Herramientas necesarias para compilar y ejecutar correctamente XV6:  
Versión de linux: Ubuntu 22.04.5 LTS, kernel 5.15.0-191   
GCC RISC-V: 11.4.0 (Ubuntu 11.4.0-1ubuntu1\~22.04)   
QEMU: 8.2.2  
Commit de partida: fa2ddcb7ca87a77af579380a6eafd5aff163b44f  
Version de git: 2.34.1  
Version de GNU: 4.3  
**2\. Compilar y ejecutar XV6**  
![][image5]  
I) ls y cat funcionan correctamente!  
[][image6]  
II) El comando para establecer el número de CPUs es make qemu CPUS=1, si no se pasa CPUS=1, el Makefile de XV6 está definido por defecto en 3  
![][image7]

**3\. Investigar el planificador original**  
I)El algoritmo de planificación usado por defecto es Round Robin Simple, la prueba del comportamiento es esta:  
![][image8]  
el código de rrtest usado es:  
![][image9]  
Evidencia de Round Robin:  
Creamos el programa user/rrtest.c con ayuda de la IA, que lanza 3 procesos al mismo tiempo (A, B y C). Cada uno hace sumas largas para mantener ocupada la CPU e imprime su letra 60 veces. Al correrlo con una sola CPU, la salida fue:  
AAABBBBBBBCCCCCCAAAAAAAABBBBBBBCCCCCC…  
Esto demuestra que XV6 usa Round Robin porque:   
Nadie acapara la CPU: Ningún proceso imprime sus 60 letras de corrido; el sistema los interrumpe para darle paso al siguiente.  
Turnos en círculo ($A\rightarrow B\rightarrow C$): El sistema les da la CPU en orden y, cuando termina con C, vuelve a empezar por A repetidamente.  
Mismo tiempo para todos: En cada turno, a cada proceso le alcanza el tiempo para imprimir casi la misma cantidad de letras (entre 7 y 8\) antes de que el reloj le quite la CPU, probando que todos reciben el mismo tiempo y ninguno tiene prioridad sobre otro. 

II)Se implementa en el archivo proc.c, en la ruta kernel/proc.c, mas especificamente en la función “void scheduler(void)”  
Dentro de scheduler(), un ciclo for(p \= proc; p \< \&proc\[NPROC\]; p++) recorre en orden la tabla de procesos proc. Para cada posición, toma el candado del proceso (acquire(\&p-\>lock)) y evalúa la condición if(p-\>state \== RUNNABLE) para determinar si ese proceso está listo para ejecutarse.  
Cuando encuentra un proceso que cumple esa condición, lo selecciona realizando tres pasos antes de cederle el control:  
Cambia su estado a en ejecución: p-\>state \= RUNNING;.  
Lo registra como el proceso activo en la estructura de la CPU actual: c-\>proc \= p;.  
Realiza el cambio de contexto invocando swtch(\&c-\>context, \&p-\>context);, guardando los registros del planificador y cargando los del proceso elegido para que comience a correr en la CPU.  
![][image10]  
III)La selección del planificador depende directamente de los estados definidos en kernel/proc.h (línea 79), porque scheduler() solo puede meter a la CPU procesos que estén en estado RUNNABLE.  
La relación entre el proceso que sale, el que entra y los eventos funciona como un relevo en tres pasos:  
1\. El proceso que está en la CPU (RUNNING) la cede por un evento:  
Mientras un proceso usa la CPU su estado es RUNNING. Antes de salir y devolverle el control al planificador con sched(), el propio proceso cambia su estado según el evento que lo haga salir:  
Se acabó su tiempo (Timer): El reloj interrumpe (kernel/trap.c, líneas 86 y 158\) y llama a yield() (kernel/proc.c, línea 505), que lo pasa de RUNNING a RUNNABLE porque sigue listo para correr en el próximo turno.  
Tiene que esperar algo (I/O o un hijo): El proceso llama a sleep() (kernel/proc.c, línea 568\) y pasa de RUNNING a SLEEPING.  
Terminó de ejecutarse: El proceso llama a kexit() (kernel/proc.c, línea 358\) y pasa de RUNNING a ZOMBIE.  
2\. Un proceso dormido recupera la posibilidad de competir:  
Cuando por fin ocurre el evento que un proceso dormido esperaba, se ejecuta wakeup() (kernel/proc.c, línea 590). Esto no lo mete a la CPU de inmediato, sino que cambia su estado de SLEEPING a RUNNABLE para que vuelva a hacer fila.  
3\. El proceso que va a entrar a la CPU es elegido:  
Cuando el proceso saliente salta a sched() y ejecuta swtch() (kernel/swtch.S), el control regresa al ciclo de scheduler() (kernel/proc.c, línea 451). Este sigue buscando en la tabla, encuentra al siguiente proceso en estado RUNNABLE, lo cambia a RUNNING, lo asigna a la CPU (c-\>proc \= p) y hace swtch() para cargarlo en el procesador.

Tabla de transiciones de estado de los procesos en XV6

| Transición | Evento | Función (kernel/proc.c) | Línea |
| :---: | ----- | :---: | :---: |
| RUNNABLE → RUNNING | El planificador elige el proceso | scheduler | 451 |
| RUNNING → RUNNABLE | Fin del quantum (timer, trap.c 86 y 158\) | yield | 505 |
| RUNNING → SLEEPING | Espera un evento | sleep | 568 |
| SLEEPING → RUNNABLE | Llega el evento | wakeup | 590 |
| SLEEPING → RUNNABLE | Matan al proceso dormido | kkill | 614 |
| RUNNING → ZOMBIE | El proceso termina | kexit | 358 |
| → RUNNABLE (primer turno) | Primer proceso del sistema | userinit | 228 |
| → RUNNABLE (primer turno) | Hijo recién creado | kfork | 301 |

IV)El código del planificador se integra al sistema operativo final en tres pasos controlados por el Makefile y el arranque en kernel/main.c:  
1\. Compilación y enlazado en el binario (Makefile y kernel/kernel.ld):  
En el Makefile (líneas 4–18), la lista OBJS incluye los módulos del planificador: \$K/proc.o (línea 15; scheduler, sched, yield), \$K/swtch.o (línea 16; cambio de contexto) y \$K/trap.o (línea 18; interrupciones de reloj). Al ejecutar make, el compilador riscv64-linux-gnu-gcc los traduce a archivos .o y el enlazador riscv64-linux-gnu-ld (Makefile, líneas 94–95: \$(LD) \$(LDFLAGS) \-T \$K/kernel.ld \-o \$K/kernel \$(OBJS)), siguiendo el script kernel/kernel.ld, los une con el resto de objetos para crear el ejecutable final kernel/kernel.  
Evidencia en el binario final: al listar los símbolos de kernel/kernel con riscv64-linux-gnu-nm kernel/kernel \| grep \-w "scheduler\\\|sched\\\|yield\\\|swtch" aparecen scheduler (0x80001c72), sched (0x80001d26), yield (0x80001de0) y swtch (0x800022dc), todos de tipo T (código global). Esto demuestra que el código del planificador quedó enlazado dentro de kernel/kernel.  
2\. Carga en el emulador (Makefile):  
Al correr make qemu, se ejecuta qemu-system-riscv64 pasándole el parámetro \-kernel kernel/kernel, lo que carga ese binario ya compilado en la memoria de la máquina virtual.  
3\. Conexión e inicio al arrancar (kernel/entry.S, kernel/start.c, kernel/defs.h y kernel/main.c):  
Al arrancar, QEMU carga el kernel en 0x80000000 y cada CPU ejecuta \_entry (kernel/entry.S), que prepara la pila y llama a start() (kernel/start.c). start() deja main como dirección de retorno (línea 25: w\_mepc((uint64)main)) y salta a ella en modo supervisor (línea 50). El encabezado kernel/defs.h (líneas 95–102) declara globalmente scheduler(), sched() y yield() para que todo el kernel pueda usarlas. Finalmente, la función main() en kernel/main.c configura el sistema y en su última línea (línea 44\) llama a scheduler();, entregándole el control de la CPU al planificador.  
Pruebas:  
![][image11]  
Los números de la izquierda son líneas del Makefile: proc.o (15), swtch.o (16) y trap.o (18) están en la lista OBJS. sysproc.o (20) aparece solo porque el filtro busca “proc.o”. Las líneas 94 y 95 muestran la regla que enlaza todos los objetos con kernel.ld para producir kernel/kernel. Al final, nm confirma que sched, scheduler, swtch y yield están dentro de kernel/kernel (tipo T, código global), con direcciones desde 0x80001c72.

![][image12]  
entry.S (líneas 9 a 11\) prepara la pila de cada CPU a partir de su hartid. En start.c, la línea 25 deja main como dirección de salto (w\_mepc((uint64)main)) y la línea 50 indica el salto a main() en modo supervisor. En main.c, la línea 44 llama a scheduler();, y desde ahí el planificador toma el control de la CPU.

**4\. Describir MLFQ y explorar su incorporación**  
**Descripciones de MLFQ:**  
J:El MLFQ o multi level feedback queue es un algoritmo que le permite a la CPU decidir que proceso ejecutar, usando un sistema de colas, prioridades y quantums de tiempo, donde todo proceso que entra entra al algoritmo, entra en la cola más alta y con la mayor prioridad posible, si un proceso tiene mayor prioridad que otro, se ejecuta primero, si tienen la misma prioridad, se hace Round Robin, el quantum de tiempo es un tiempo de uso en CPU definido por el mismo algoritmo que determina cuánto tiempo tiene cada proceso para usar la CPU, si un proceso gasta todo su tiempo en CPU y no termina, su prioridad bajará, mientras que si otro devuelve la CPU antes de que se acabe su quantum de tiempo, su prioridad se mantendrá, el algoritmo presenta ciertos problemas, como que si hay muchos procesos de entrada y salida ejecutándose al mismo tiempo, esto hará que un proceso que requiera la CPU más intensivamente sufra de “inanición”, o que nunca tenga la oportunidad de usar la CPU, por esto mismo, se propone que determinado un tiempo “S”, todos los procesos reinicien su prioridad a la más alta, tambien se presenta el problema de que un proceso malintencionado que sepa cuanto es el quantum de tiempo, entregue la CPU en un tiempo menor al quantum, para así ejecutarse con la mayor prioridad posible siempre, para esto, se planteó que despues de que un proceso se mantenga mucho tiempo en una cola, degrade su prioridad sin importar si entregó la CPU en el quantum o si no lo hizo, resumidamente, hay 5 reglas para este algoritmo  
1\. Si hay dos procesos, corre el que tenga mayor prioridad  
2\. Si tienen la misma prioridad, se hace Round Robin  
3\. Cuando un proceso llega, se pone al tope de prioridad  
4\. Si un proceso devuelve la CPU antes de que se cumpla su lapso de tiempo disponible en CPU, su prioridad se mantiene, si se acaba ese lapso y el proceso no acaba, entonces se degrada su prioridad, y después de un tiempo en el mismo nivel de prioridad, el proceso se degrada en prioridad sin importar el tiempo en que entregue la CPU  
5\. Todos los procesos reinician su prioridad al máximo después de un tiempo S

Preguntas  
\-Como se cuanto tiempo se pone en el quantum?  
\-Cuantas colas hay que usar para que funcione eficientemente?  
\-De cuanto es el tiempo S de la regla 5?  
\-Cuanto tiempo tiene que pasar un proceso en el mismo nivel de prioridad para que se degrade usando la regla 4?

F:Por lo que entiendo del algoritmo MLFQ, sé que organiza los procesos en varias colas, cada una con un nivel de prioridad distinto, y que la regla principal es que siempre se ejecuta primero lo que esté en la cola de mayor prioridad, solo cuando esa cola queda vacía se atiende a las de abajo. Si hay varios procesos en una misma cola, se turnan entre ellos con Round Robin. También sé que todo proceso nuevo entra en la cola más alta, porque el planificador no sabe de antemano cuánto va a durar, así que le da el beneficio de la duda. Las dudas que tengo del algoritmo son varías, pero en resumen son cosas como: que no sé cómo se decide cuántas colas debe haber ni qué tamaño de quantum se le asigna a cada una o por ejemplo tengo la duda si un proceso podría "aprovecharse" del sistema soltando la CPU justo antes de que termine su quantum para no perder prioridad, y cómo hace el algoritmo para evitar eso.

**Síntesis realizada para envíar:**  
Lo que averiguamos del planificador original y de su incorporación al sistema compilado:

I. Algoritmo por defecto: XV6 utiliza por defecto una política de planificación Round Robin Simple, la cual se evidencia empíricamente con el programa \`user/rrtest.c\`: al ejecutar múltiples procesos concurrentes sin operaciones de I/O, el sistema los expropia y alterna de forma justa, circular ($A\rightarrow B\rightarrow C$) y equitativa, dándole a cada uno el mismo quantum de tiempo sin prioridades.

II. Selección del proceso (\`kernel/proc.c\`): La selección se realiza en la función \`scheduler(void)\`. Esta recorre secuencialmente la tabla global de procesos \`proc\` mediante un ciclo \`for\`, adquiere el candado de cada proceso (\`acquire(\&p-\>lock)\`) y evalúa la condición \`if(p-\>state \== RUNNABLE)\`. Al encontrar uno listo, cambia su estado a \`RUNNING\`, lo registra en la CPU actual (\`c-\>proc \= p\`) y ejecuta el cambio de contexto con \`swtch(\&c-\>context, \&p-\>context)\`.

III. Relación con estados y eventos:  
\- Estados (\`kernel/proc.h\`): \`RUNNABLE\` (listo para competir), \`RUNNING\` (ejecutándose en la CPU) y \`SLEEPING\` (en pausa esperando un canal \`p-\>chan\`).  
\- Ceder la CPU: Es el propio proceso saliente el que cambia su estado antes de llamar a \`sched()\`. Las formas de salida son por fin de quantum (\`yield()\` invocado desde el temporizador en \`kernel/trap.c\`), por espera de un evento (\`sleep()\`) o por finalización (\`kexit()\`).  
\- Recuperar la CPU: Cuando ocurre el evento esperado, \`wakeup()\` cambia el estado del proceso de \`SLEEPING\` a \`RUNNABLE\`. Posteriormente, el ciclo de \`scheduler()\` lo selecciona de nuevo en su turno y ejecuta \`swtch()\` para retomarlo.

IV. Compilación, construcción y arranque:  
\- Compilación y Enlazado (\`Makefile\` y \`kernel/kernel.ld\`): Los módulos clave como \`proc.o\`, \`swtch.o\` y \`trap.o\` se declaran en la lista \`OBJS\` del \`Makefile\`. Durante la ejecución de \`make\`, el compilador los traduce a objetos y el enlazador los une siguiendo el script \`kernel/kernel.ld\` para generar el binario ejecutable \`kernel/kernel\` (verificable con \`nm\`).  
\- Carga y Arranque (\`kernel/entry.S\`, \`kernel/start.c\`, \`kernel/main.c\`): Al ejecutar \`make qemu\`, QEMU carga el kernel en memoria. El flujo arranca en \`\_entry\` (\`kernel/entry.S\`), pasa por \`start()\` (\`kernel/start.c\` configurando el salto a \`main\` mediante \`w\_mepc\`), llega a \`main()\` en \`kernel/main.c\` y en su última línea (línea 44\) invoca \`scheduler();\`, cediéndole el control definitivo de la CPU.

**Prompt usado:**  
Actúa como tutor socrático de una pareja de estudiantes de cuarto semestre de Ingeniería  
de Sistemas. Conocemos C++ y hemos comenzado a explorar XV6 para RISC-V.  
Queremos identificar y justificar qué se necesitaría para incorporar MLFQ a XV6. En esta  
entrega no vamos a implementarlo.  
Nuestras descripciones iniciales del algoritmo, escritas antes de esta conversación:  
Juan:  
El MLFQ o multi level feedback queue es un algoritmo que le permite a la CPU decidir que proceso ejecutar, usando un sistema de colas, prioridades y quantums de tiempo, donde todo proceso que entra entra al algoritmo, entra en la cola más alta y con la mayor prioridad posible, si un proceso tiene mayor prioridad que otro, se ejecuta primero, si tienen la misma prioridad, se hace Round Robin, el quantum de tiempo es un tiempo de uso en CPU definido por el mismo algoritmo que determina cuánto tiempo tiene cada proceso para usar la CPU, si un proceso gasta todo su tiempo en CPU y no termina, su prioridad bajará, mientras que si otro devuelve la CPU antes de que se acabe su quantum de tiempo, su prioridad se mantendrá, el algoritmo presenta ciertos problemas, como que si hay muchos procesos de entrada y salida ejecutándose al mismo tiempo, esto hará que un proceso que requiera la CPU más intensivamente sufra de “inanición”, o que nunca tenga la oportunidad de usar la CPU, por esto mismo, se propone que determinado un tiempo “S”, todos los procesos reinicien su prioridad a la más alta, tambien se presenta el problema de que un proceso malintencionado que sepa cuanto es el quantum de tiempo, entregue la CPU en un tiempo menor al quantum, para así ejecutarse con la mayor prioridad posible siempre, para esto, se planteó que despues de que un proceso se mantenga mucho tiempo en una cola, degrade su prioridad sin importar si entregó la CPU en el quantum o si no lo hizo, resumidamente, hay 5 reglas para este algoritmo  
1\. Si hay dos procesos, corre el que tenga mayor prioridad  
2\. Si tienen la misma prioridad, se hace Round Robin  
3\. Cuando un proceso llega, se pone al tope de prioridad  
4\. Si un proceso devuelve la CPU antes de que se cumpla su lapso de tiempo disponible en CPU, su prioridad se mantiene, si se acaba ese lapso y el proceso no acaba, entonces se degrada su prioridad, y después de un tiempo en el mismo nivel de prioridad, el proceso se degrada en prioridad sin importar el tiempo en que entregue la CPU  
5\. Todos los procesos reinician su prioridad al máximo después de un tiempo S

Preguntas  
\-Como se cuanto tiempo se pone en el quantum?  
\-Cuantas colas hay que usar para que funcione eficientemente?  
\-De cuanto es el tiempo S de la regla 5?  
\-Cuanto tiempo tiene que pasar un proceso en el mismo nivel de prioridad para que se degrade usando la regla 4?  
Frank:  
Por lo que entiendo del algoritmo MLFQ, sé que organiza los procesos en varias colas, cada una con un nivel de prioridad distinto, y que la regla principal es que siempre se ejecuta primero lo que esté en la cola de mayor prioridad, solo cuando esa cola queda vacía se atiende a las de abajo. Si hay varios procesos en una misma cola, se turnan entre ellos con Round Robin. También sé que todo proceso nuevo entra en la cola más alta, porque el planificador no sabe de antemano cuánto va a durar, así que le da el beneficio de la duda. Las dudas que tengo del algoritmo son varías, pero en resumen son cosas como: que no sé cómo se decide cuántas colas debe haber ni qué tamaño de quantum se le asigna a cada una o por ejemplo tengo la duda si un proceso podría "aprovecharse" del sistema soltando la CPU justo antes de que termine su quantum para no perder prioridad, y cómo hace el algoritmo para evitar eso.  
Lo que averiguamos del planificador original y de su incorporación al sistema compilado:  
I. Algoritmo por defecto: XV6 utiliza por defecto una política de planificación Round Robin Simple, la cual se evidencia empíricamente con el programa user/rrtest.c: al ejecutar múltiples procesos concurrentes sin operaciones de I/O, el sistema los expropia y alterna de forma justa, circular (ABC) y equitativa, dándole a cada uno el mismo quantum de tiempo sin prioridades.

II. Selección del proceso (kernel/proc.c): La selección se realiza en la función scheduler(void). Esta recorre secuencialmente la tabla global de procesos proc mediante un ciclo for, adquiere el candado de cada proceso (acquire(\&p-\>lock)) y evalúa la condición if(p-\>state \== RUNNABLE). Al encontrar uno listo, cambia su estado a RUNNING, lo registra en la CPU actual (c-\>proc \= p) y ejecuta el cambio de contexto con swtch(\&c-\>context, \&p-\>context).

III. Relación con estados y eventos:  
\- Estados (kernel/proc.h): RUNNABLE (listo para competir), RUNNING (ejecutándose en la CPU) y SLEEPING (en pausa esperando un canal p-\>chan).  
\- Ceder la CPU: Es el propio proceso saliente el que cambia su estado antes de llamar a sched(). Las formas de salida son por fin de quantum (yield() invocado desde el temporizador en kernel/trap.c), por espera de un evento (sleep()) o por finalización (kexit()).  
\- Recuperar la CPU: Cuando ocurre el evento esperado, wakeup() cambia el estado del proceso de SLEEPING a RUNNABLE. Posteriormente, el ciclo de scheduler() lo selecciona de nuevo en su turno y ejecuta swtch() para retomarlo.

IV. Compilación, construcción y arranque:  
\- Compilación y Enlazado (Makefile y kernel/kernel.ld): Los módulos clave como proc.o, swtch.o y trap.o se declaran en la lista OBJS del Makefile. Durante la ejecución de make, el compilador los traduce a objetos y el enlazador los une siguiendo el script kernel/kernel.ld para generar el binario ejecutable kernel/kernel (verificable con nm).  
\- Carga y Arranque (kernel/entry.S, kernel/start.c, kernel/main.c): Al ejecutar make qemu, QEMU carga el kernel en memoria. El flujo arranca en \_entry (kernel/entry.S), pasa por start() (kernel/start.c configurando el salto a main mediante w\_mepc), llega a main() en kernel/main.c y en su última línea (línea 44\) invoca scheduler();, cediéndole el control definitivo de la CPU.  
Reglas de interacción:  
1\. Formula una sola pregunta principal por turno y espera nuestra respuesta. No presentes  
una lista completa de cambios ni una arquitectura resuelta.  
2\. Empieza examinando nuestra descripción de MLFQ. Ayúdanos a detectar reglas ambiguas,  
ausentes o contradictorias. No sustituyas nuestra descripción por una versión terminada  
ni elijas silenciosamente una variante del algoritmo.  
3\. Pídenos que propongamos y justifiquemos las decisiones que faltan. Después, ayúdanos a comprobarlas con ejemplos pequeños de procesos y decisiones de planificación que nosotros debamos resolver.  
4\. Guíanos para relacionar cada regla con XV6. Antes de sugerir una modificación,  
pregúntanos qué comportamiento necesitamos y qué evidencia del código respalda nuestra  
propuesta.  
5\. No supongas que tienes acceso a nuestro repositorio. Pide los fragmentos necesarios y  
distingue lo observado de lo que todavía sea una hipótesis. No inventes archivos,  
funciones o comportamientos de nuestra versión.  
6\. No entregues código de implementación, parches, una lista cerrada de archivos por  
modificar ni pseudocódigo que resuelva el planificador. Revisa nuestras propuestas y  
señala problemas mediante preguntas y pistas graduales. Puedes explicar conceptos previos  
con ejemplos mínimos.  
7\. Si nos bloqueamos, reduce la dificultad de la pregunta y ofrece una pista concreta. Si  
pedimos la solución directa, conserva el modo socrático.  
El objetivo es que nosotros logremos:  
A. Redactar una descripción revisada y coherente de la variante de MLFQ considerada.  
B. Explicar qué cambios necesitaría XV6 para soportarla, indicando para cada uno su  
propósito, su relación con las reglas y la evidencia del código que lo justifica.  
C. Proponer escenarios para comprobar una futura implementación del algoritmo MLFQ en el  
sistema operativo XV6. Pídenos que expliquemos qué procesos participarían, qué harían,  
qué regla comprobaríamos y qué comportamiento esperaríamos observar. No nos entregues los  
escenarios resueltos. En esta tarea solo los describiremos; no vamos a programarlos ni  
ejecutarlos.  
Mantén el análisis dentro del alcance de una propuesta: no necesitamos implementar MLFQ

ni construir un selector de planificadores. Ayúdanos a registrar las cuestiones que  
queden pendientes de investigación.  
Al finalizar, pídenos que redactemos nuestras conclusiones. Comprueba su consistencia  
mediante una última pregunta aplicada a un caso concreto, sin escribir por nosotros el  
reporte.  
Comienza con una pregunta que permita comprobar cómo entendemos una regla de nuestra descripción inicial de MLFQ.

**1\. Definición de las Reglas del MLFQ en el Diseño**  
Para garantizar la comprensión del sistema propuesto, las cinco reglas fundamentales del algoritmo Multi-Level Feedback Queue se interpretan y adaptan bajo los siguientes criterios de diseño:  
**Regla 1 (Prioridad estricta):** Si la prioridad de un proceso $A$ es menor que la de un proceso $B$, corre $B$ y $A$ no corre. Además, si un proceso de mayor jerarquía pasa a estado RUNNABLE, el kernel lo expropia a más tardar en el siguiente tick del temporizador.  
**Regla 2 (Quantums por turno / Round-Robin interno):** Cada nivel de cola cuenta con un quantum propio. Los procesos de un mismo nivel compiten mediante un turno Round-Robin regulado por dicho quantum (el cual se asume provisionalmente en valores crecientes de 10, 20, 40 y 80 ms para amortiguar el costo de los cambios de contexto en tareas pesadas de cálculo).  
**Regla 3 (Entrada en la cima):** Todo proceso que ingresa al sistema por primera vez (kfork() o userinit()) es colocado inicialmente en la cola de máxima prioridad.  
**Regla 4 (Contabilidad dual y descenso):** Se divide en dos aspectos:  
Primera mitad: Si un proceso consume todo su tiempo asignado (quantum) en un turno sin ceder la CPU voluntariamente, se contabiliza y, al alcanzar el límite acumulado del nivel (allotment), se le degrada a una cola inferior.  
Segunda mitad: Si un proceso se bloquea voluntariamente haciendo sleep() antes de agotar su turno (ej. ráfagas breves de E/S), al despertar recibe un quantum nuevo completo, protegiendo su agilidad interactiva, aunque su acumulador histórico de nivel sigue sumando hasta alcanzar el umbral de castigo.  
**Regla 5 (Impulso de prioridad global):** Después de un período de tiempo global $S$ (provisionalmente fijado en $1000ms$), todas las tareas del sistema se mueven a la cola superior para evitar la inanición (starvation), reiniciándose el acumulador de los procesos que descienden, pero manteniendo la coherencia de aquellos que ya operaban en la cima.  
**2\. Propuesta de Cambios en XV6 con Referencias a Archivos y Funciones, Necesidad y Evidencia**  
Para lograr un algoritmo MLFQ funcional en XV6, se proponen los siguientes cambios en la arquitectura del mismo XV6:  
**1\. Inclusión de campos de control multinivel en struct proc y su inicialización:**  
Archivo y función: kernel/proc.h (struct proc) y kernel/proc.c (allocproc() — hipótesis pendiente de confirmación local).  
\-Necesidad que atiende: Almacenar el nivel de cola actual, el contador de turno y el acumulador de tiempo de CPU, asignando la máxima prioridad al nacer.  
\-Reglas que sostiene: Sostiene la Regla 3 y alimenta las Reglas 1, 2 y 4\.  
\-Evidencia del código: La estructura struct proc muestra campos básicos (state, pid, killed), pero carece de atributos de prioridad.  
**2\. Modificación del planificador para selección por niveles y Round-Robin interno:**  
\-Archivo y función: kernel/proc.c (scheduler()).  
\-Necesidad que atiende: Reemplazar el recorrido lineal por una búsqueda estructurada por múltiples colas, ejecutando primero los procesos de mayor prioridad.  
\-Reglas que sostiene: Sostiene la Regla 1 y la Regla 2\.  
\-Evidencia del código: El bucle for(p \= proc; p \< \&proc\[NPROC\]; p++) en kernel/proc.c recorre la tabla de manera plana y secuencial sin discriminar prioridades.  
**3\. Medición precisa de tiempo al entrar y salir (scheduler() y sched()):**  
Archivo y función: kernel/proc.c (scheduler() y sched()).  
Necesidad que atiende: Registrar el tiempo de ejecución real consumido (mediante marcas de tiempo al entrar y salir de RUNNING) para evitar la evasión de contabilidad (gaming).  
Reglas que sostiene: Sostiene la Regla 4 (ambas mitades).  
Evidencia del código: Las funciones de salida de CPU como sched() carecen de contadores acumulativos de tiempo de procesamiento por nivel.  
**4\. Expropiación por prioridad superior y control de quantum en el manejador de interrupciones:**  
Archivo y función: kernel/trap.c (usertrap() / kerneltrap()) y kernel/proc.c (yield()).  
Necesidad que atiende: Evaluar de manera condicional si el quantum del nivel se agotó o si un proceso de mayor jerarquía pasó a estado RUNNABLE, forzando la expropiación a más tardar en el siguiente tick.  
Reglas que sostiene: Sostiene las Reglas 1, 2 y 4 (primera mitad).  
Evidencia del código: El manejador en kernel/trap.c invoca yield() incondicionalmente ante cada interrupción de reloj (which\_dev \== 2).  
**5\. Implementación del mecanismo global de impulso de prioridad (Priority Boost) y configuración:**  
Archivo y función: kernel/proc.c y kernel/trap.c (which\_dev \== 2).  
Necesidad que atiende: Almacenar los parámetros estáticos globales (niveles, quantums progresivos de 10, 20, 40, 80 ms, y $S=1000ms$) y aprovechar el pulso del temporizador para el barrido masivo.  
Reglas que sostiene: Sostiene la Regla 5\.  
Evidencia del código: El código base cuenta con el pulso periódico del temporizador en kernel/trap.c, pero no se ha identificado en lo revisado hasta ahora un contador global de sistema (hipótesis).  
**3\. Necesidad y Explicación de la Propuesta**  
Cada cambio responde a una limitación estructural del planificador nativo de XV6. El modelo MLFQ justifica cada modificación para cumplir los objetivos duales: minimizar el tiempo de respuesta interactivo en la cima con quantums breves (10-20 ms) y proteger el rendimiento global aislando a los procesos pesados en el fondo con quantums largos (40-80 ms) que amortiguan la sobrecarga de cambios de contexto.  
**4\. Decisiones de la Propuesta que Necesitan Soporte Adicional**  
1\. Valor real de interval y frecuencia del contador: Traducción de ciclos a milisegundos reales (hipótesis).  
2\. Valores finales de $S$ y límite por nivel: Sintonización fina de umbrales (hipótesis).  
3\. Umbral de CPU (Límite frente a $S$): Frontera matemática entre interactivo y abusivo (hipótesis).  
4\. Ubicación y alcance del acumulador: Penalización por nivel o global (hipótesis).  
5\. Hipótesis sobre la tolerancia de \~100 ms: Estimación preliminar de espera al despertar.  
6\. Jitter en el quantum real: Variación de hasta un tick completo por muestreo discreto.  
7\. Verificación de inicialización y contadores globales: Confirmar si allocproc() es el único punto de creación y la ausencia de contadores globales (hipótesis).  
8\. Coordinación SMP (Multinúcleo): Definir cómo sincronizar el contador global $S$ cuando cada CPU maneja sus propias interrupciones de reloj de forma independiente.  
**5\. Conclusiones y Retroalimentación de la IA**  
**Conclusiones**  
1\. El estudio de XV6 permitió proponer un modelo teórico sobre su planificador Round-Robin plano, evidenciando la necesidad de ampliar struct proc para prioridades multinivel.  
2\. Se diseñó un esquema de cuatro niveles con quantums progresivos y control dual, centralizando la contabilidad en sched() y condicionando la expropiación en trap.c.  
3\. Se asumió aceptar el jitter por muestreo de ticks, concluyendo que la Regla 1 exige expropiación activa a más tardar en el siguiente tick.  
4\. Quedan como puntos abiertos la calibración de hardware, la sintonización fina de $S$ y la validación en arquitecturas SMP.  
**Retroalimentación de la IA y Cambios Realizados**  
\-Corrección en la Regla 1: Inicialmente se asumía erróneamente que modificar únicamente el bucle de selección del planificador bastaba; al analizar el caso de un proceso de alta prioridad que despierta mientras corre uno bajo, se corrigió incorporando la expropiación obligatoria a más tardar en el siguiente tick.  
\-Contabilidad basada en marcas de tiempo vs. ticks: Se propuso incorporar marcas de tiempo al entrar y salir de RUNNING para evitar que procesos cortos evadan el control.  
\-Separación conceptual: Se diferenció estrictamente el quantum por turno del límite de nivel acumulativo.  
\-Comportamiento del impulso global: Se estableció que el impulso $S$ no debe reiniciar el acumulador de un proceso que ya opera en la cima.  
**6\. Escenarios de Prueba Propuestos (Sin Ejecutar)**  
**\-Escenario 1 (Regla 1 \- Prioridad Estricta y Expropiación):**  
Participantes: Dos procesos, $L$ (prioridad baja) y $H$ (prioridad alta).  
Qué harían: $L$ ejecuta un cálculo largo; $H$ despierta de un bloqueo de E/S y pasa a RUNNABLE.  
Comportamiento observable: A través de la salida por consola, se observará que $L$ interrumpe la impresión de sus caracteres a más tardar en el siguiente tick del temporizador, dando paso inmediato a la salida por pantalla de $H$.  
**\-Escenario 2 (Regla 2 \- Equidad Round-Robin):**  
Participantes: Dos procesos intensivos en CPU (${P}_{1}$ y ${P}_{2}$) en el mismo nivel de cola.  
Qué harían: Ejecutan cálculo continuo sin E/S.  
Comportamiento observable: Alternancia regular y equitativa en la consola, limitando los turnos al quantum nominal sujeto al jitter por ticks.  
**\-Escenario 3 (Regla 4, 1ra Mitad \- Descenso por Agotamiento):**  
Participantes: Un proceso solitario intensivo en cálculo ($P$).  
Qué harían: Mantiene la CPU ocupada agotando ráfaga tras ráfaga.  
Comportamiento observable: Verificable mediante un mensaje de depuración temporal en el kernel que imprima el cambio de nivel del proceso.  
**\-Escenario 4 (Regla 4, 2da Mitad \- Proceso Interactivo / Caso del Proceso A):**  
Participantes: Un proceso $A$ ($9ms$ CPU \+ $1ms$ sleep) y quantum de $10ms$.  
Qué harían: Alterna cálculo y E/S.  
Comportamiento observable: Se espera observar que la contabilidad dual acumule el tiempo de CPU en sucesivas ráfagas (alcanzando los $50ms$ en la sexta ráfaga), momento en el cual el kernel degrada al proceso.  
**\-Escenario 5 (Regla 3 \- Entrada al Sistema):**  
Participantes: Proceso padre que genera un hijo mediante kfork().  
Qué harían: Creación del hijo.  
Comportamiento observable: Al inspeccionar la estructura mediante una traza de depuración temporal, su nivel de cola inicial apunta directamente a la cima.  
**\-Escenario 6 (Regla 5 \- Prevención de Inanición por Impulso Global):**  
Participantes: Proceso pesado $B$ en la cola inferior y procesos interactivos cortos $I$ saturando la cima.  
Qué harían: $I$ acapara la cima impidiendo que $B$ reciba CPU.  
Comportamiento observable:  
\-Antes del impulso: $B$ permanece estancado en la consola sin emitir trazas de avance.  
\-Después del impulso: Al cumplirse $S$, se observa en la consola que $B$ comienza a emitir trazas de ejecución al ingresar al ciclo de Round-Robin superior, y posteriormente vuelve a descender de forma regulada si mantiene su carga pesada.

**Extras:**  
**Servicio y modelo de IA**  
Claude, de Anthropic, El modelo que utilizamos fue “Claude Sonnet 5.5”  
**Interacciones importantes:**

**1\.** Cuando empezamos tuvimos esta interaccion, en la que nuestra hipotesis era que si un programa de 1 ms de uso de tiempo y 30 de sleep, con un sistema de degradamiento solo teniendo en cuenta el tiempo de uso efectivo de CPU, se tardaría 50 ms en degradar su prioridad, y esto acaparararia la CPU, pero luego nos dimos cuenta que no era así  
**Claude**: Elegir (b) es razonable, pero la justificación necesita ajuste. Con (a), A sí se degradaría, porque el reloj corre aunque duerma, así que no “escaparía”. El problema es otro: (a) castiga también al proceso que casi no usa CPU y pasa su vida dormido, que es justo el que MLFQ quiere proteger. Con (b), A acumula 9 ms por ráfaga y alcanza el límite de 50 ms tras unas 6 ráfagas, sin importar cuánto duerma entre una y otra.

Ahora pongan a prueba (b) con un proceso distinto. El proceso C es interactivo y honesto: corre 1 ms, duerme 30 ms esperando al teclado, y repite.

Pregunta: con la definición (b) y un límite de 50 ms de CPU en el nivel alto, ¿cuándo se degradaría C? ¿Les parece un resultado deseable? Si no lo es, ¿qué otra regla de las cinco entra a compensarlo, y cómo?

Respondan los dos primero con cuentas simples (cuántas ráfagas, cuánto tiempo de reloj pasa). Frank, aquí también me interesa tu opinión.  
**Nosotros**: Se degradaría cuando se pasen los 50ms de tiempo de uso de CPU, osea despues de 50 rafagazos, no creemos que esto sea lo mejor ya que podría acaparar la CPU, y dejar a los otros procesos sin CPU   
**Claude**:Las 50 ráfagas son correctas (50 ms ÷ 1 ms). Pero la conclusión de que C “acapararía la CPU” no cuadra con los números del caso, así que revisémosla antes de seguir.

Calculen dos cosas con los datos de C (1 ms de CPU, 30 ms dormido, en bucle):

¿Qué fracción del tiempo ocupa C la CPU? Piensen en cuánto dura un ciclo completo de reloj.  
¿Cuánto tiempo de reloj pasa hasta completar esas 50 ráfagas y degradarse?

Pregunta: con esos resultados, ¿sigue pareciéndoles que C deja sin CPU a otros procesos? Si no, ¿cuál es entonces el problema real, si lo hay, de que C se degrade con la definición (b)? Piensen en qué le pasaría a C, y no a los demás, una vez esté en un nivel bajo y llegue un proceso que consume mucha CPU.

Todavía queda pendiente la segunda mitad de la pregunta anterior: ¿qué otra regla de las cinco compensaría ese problema, y cómo? Respondan primero los cálculos y el razonamiento sobre C, y luego esa parte.  
**Nosotros:**Ocupa 31, lo que lo usa y lo que duerme  
El tiempo en reloj para esas 50 rafagas, sería, si espera los 30 segundos en cada rafaga sería de 1550ms?  
**Claude:**El 31 ms es la duración de un ciclo completo y el 1550 ms es correcto (50 ciclos × 31 ms). Solo una aclaración: son 30 ms, no segundos.

Falta un paso en el punto 1, porque les pedía la fracción de ese ciclo en que C ocupa la CPU. Con 1 ms de CPU dentro de un ciclo de 31 ms, ¿qué porcentaje es?

Pregunta: con ese porcentaje en mano, ¿sigue pareciéndoles que C “acapara” la CPU? Si no, ¿cuál es el problema real de que C se degrade con la definición (b) tras esos \~1.5 s? Piensen en C, no en los demás: una vez en un nivel bajo, llega un proceso que consume CPU sin parar. ¿Quién corre primero según la regla 1, y cómo se vería eso desde el teclado?

Cuando eso quede claro, retomamos la parte pendiente: qué otra regla de las cinco compensaría ese problema, y cómo.  
**Nosotros:**el porcentaje es 3.22% aproximadamente   
**Claude:**Ese 3.22% es correcto (1/31), y es el dato clave: C usa cerca del 3% de la CPU. Con eso ya pueden descartar la preocupación de que acapare la CPU, porque casi no la toca. Si C se degrada, el problema no recae sobre los demás procesos sino sobre C.   
**Con esa corrección nos dimos cuenta de que no era correcta nuestra hipótesis**  
**2\.** Al comienzo tambien tuvimos esta interacción, donde nuestra hipótesis era que si se repetía el patrón mencionado por la IA, el proceso B sufriría de inanición, para evitarlo, el algoritmo degradaría a A por la regla 4 y luego tras S se reiniciarían todos  
**Claude:**Hola, Juan y Frank. Empecemos por sus descripciones, y por ahora solo con una regla.

Ambos dicen que un proceso que devuelve la CPU antes de que se acabe su quantum mantiene su prioridad. Juan lo describe como “su prioridad se mantendrá”, y Frank se pregunta si un proceso podría “aprovecharse” soltando la CPU justo antes de que termine el quantum.

Imaginen este caso mínimo, con dos niveles (alto y bajo) y un quantum de 10 ms en cada uno:

El proceso A corre 9 ms y llama a sleep() esperando un evento de I/O. Cuando el evento ocurre, vuelve a estar listo.  
El proceso B corre 10 ms completos sin soltar la CPU.

Pregunta: según la regla 4 tal como la redactaron, ¿qué nivel de prioridad tendrá cada proceso después de esa primera ronda, y qué pasaría con A si repite ese patrón indefinidamente (9 ms de CPU y luego sleep()) mientras B sigue consumiendo quantums completos?

Respondan primero con sus propias palabras. Después, cuéntenme si les parece que la regla 4 describe bien lo que pasaría, o si notan algo ambiguo o contradictorio en ella (por ejemplo, entre sus dos mitades). Prefiero que lo razonen antes de decirles yo qué buscar.  
**Nosotros:**Despues de la primera ronda A se queda en el nivel alto de prioridad y B baja al nivel bajo  
**Claude:**Bien, esa primera parte es correcta: A conserva su nivel porque soltó la CPU antes de agotar el quantum, y B baja porque lo consumió completo.  
Les falta la segunda mitad de la pregunta: ¿qué pasa con A si repite ese patrón (9 ms de CPU y luego sleep()) mientras B sigue consumiendo quantums completos?  
Para razonarlo, piensen en la cantidad de CPU que A le está quitando a B con el paso del tiempo. A nunca baja de nivel, y B está en el nivel bajo. Según la regla 1, ¿quién corre cuando ambos están listos? Y si A estuviera listo casi todo el tiempo, ¿cuánto de la CPU llegaría a recibir B?  
Después, relacionen eso con la pregunta que Frank dejó abierta: ¿se trata de un proceso “aprovechado” que explota la regla, o de un comportamiento que la regla busca premiar? Díganme qué piensan y por qué.  
**Nosotros:**si se repite ese patrón, A seguiría corriendo y B sufriría de "inanicion", para evitarlo, lo que pasaría es que el algoritmo degradaría a A de nivel por la regla 4, ya que sin importar el tiempo que lleve en alta prioridad se degrada, y luego despues del tiempo S se reiniciarian todos al nivel mas alto  
**Claude:**Sí: B quedaría sin CPU mientras A siga disponible, y ya identificaron que la segunda mitad de la regla 4 es lo que frena ese patrón. Ahora hay que probar si esa mitad está lo bastante definida, porque Juan se preguntó “cuánto tiempo tiene que pasar”, y esa duda apunta a una ambigüedad.

La regla dice que el proceso se degrada “después de un tiempo en el mismo nivel de prioridad”. Hay al menos dos formas de medir ese tiempo:

(a) Tiempo de reloj desde que el proceso entró a ese nivel, incluyendo el que pasa dormido.  
(b) Tiempo de CPU que el proceso ha consumido en ese nivel, sumando todas sus ráfagas.

Apliquen cada definición al proceso A, que corre 9 ms y duerme esperando I/O, una y otra vez, y supongan un límite de 50 ms en el nivel alto.

Pregunta: ¿cuál de las dos definiciones degradaría a A de forma más fiable, y cuál podría dejarlo escapar? Justifiquen con lo que haría A en esos 50 ms. Si ven otra definición razonable además de (a) y (b), propónganla.  
**Nosotros:**Es mejor el tiempo de CPU, ya que si usaramos el tiempo de reloj incluyendo cuanto duerme, la espera podría ser de hasta 30ms, eso significaria que solo alcanzó a hacer como mucho una rafaga, y luego se degradó  
En esta conversacion confirmamos nuestra hipotesis, pero se realizó la corrección posteriormente, confirmamos que la elección fue acertada. Pero la justificación que dimos(que con (a) A se degradaría demasiado rápido) no era correcta. Claude la ajusté: con (a) A sí se degradaría, y el problema real es que castiga al proceso que casi no usa CPU.  
**3\.** Teníamos la duda de, como definir el tiempo de S correctamente, entonces se dió esta interacción:   
**Claude:**Imaginen que C está atrapado en un piso bajo de un edificio y que el único modo de subirlo es un ascensor que pasa cada S milisegundos y lo lleva al piso alto (eso es el reinicio de la regla 5). Si C llega justo después de que el ascensor se fue, ¿cuánto espera como máximo?  
Pregunta: si S \= 1000 ms, ¿cuánto tiempo de reloj puede estar C atrapado abajo, como máximo, antes de que el reinicio lo suba? Respondan con un número y una frase que lo justifique.  
Cuando lo tengan, den el paso siguiente: si el usuario solo tolera esperas de unos 100 ms, ¿qué pasa con S \= 1000 ms y qué tendría que cumplir S? Pueden responder a esa parte en el mismo mensaje.  
Dejamos para después la Comparación 2 (la de B) y la decisión pendiente sobre el contador de CPU.  
**Nosotros:**esperaría S milisegundos si llega justo cuando se fue  
Si S es 1000 ms, C puede estar atrapado como maximo abajo 1000ms, ya que ese seria el caso en el que llegue justo después de que se haya ido, no entendimos lo del paso siguiente  
**Claude:**Correcto, Juan: el peor caso es que C llegue justo después de que el reinicio ocurrió, y entonces espera casi S completo, o sea unos 1000 ms.

El “paso siguiente” es solo comparar ese resultado con una tolerancia. Para el ejercicio supongan que un usuario deja de sentir fluida la respuesta cuando esta tarda más de unos 100 ms. Ese número lo puse yo como hipótesis de trabajo, no como dato medido, y más adelante podemos discutir si es razonable.

Pregunta: con S \= 1000 ms, C puede esperar hasta 1000 ms, pero el usuario solo tolera unos 100 ms. ¿Se cumple lo que el usuario necesita? Si no se cumple, ¿qué harían con el valor de S?

Respondan con una o dos frases. Todavía no se pregunten qué pasaría con B; esa parte viene después.  
**Nosotros:**No, no se cumple, para que se cumpla, el peor caso de S, suponemos que debería ser hasta 100ms entonces como mucho?  
**Claude:**Sí, Juan. Con la tolerancia de 100 ms que supusimos, S no debería pasar de unos 100 ms. Eso es una cota superior: el criterio sale de lo que necesita C porque el 100 ms no es un dato medido.

Esa cota es todavía optimista. Después del reinicio, C está arriba pero tiene que competir por su turno, y esa espera se suma. Déjenla como cuestión pendiente a investigar, junto con la del contador de CPU.

Ahora falta el otro lado, el de B. Un límite superior no basta para elegir S, porque ya vieron que un S muy pequeño borra la diferencia entre procesos. Usen estos números: quantum de 10 ms, límite de 50 ms de CPU en el nivel alto (regla 4\) y S \= 20 ms.

Pregunta: ¿puede B llegar a degradarse alguna vez? Para responder tienen que decidir qué pasa con el contador de CPU de B cuando ocurre el reinicio, que era la decisión pendiente. Díganme qué decidieron y por qué. Después digan qué relación tendría que cumplir S con el límite de 50 ms para que la parte de degradación de la regla 4 siga teniendo efecto.  
**Nosotros:**B no se podría degradar si el limite es mayor que el tiempo S, determinamos que cuando todos suban al tope, el tiempo para degradarse se guarde, de modo que si B estaba a punto de degradarse cuando subieron todos, se degrade igualmente sin reiniciarse para mantener un orden  
Con ésto comprendimos de mejor manera el tiempo S, y planteamos la hipótesis entre nosotros en medio de la conversación, de que el tiempo de quantum debía siempre ser menor al tiempo S, ya que de otra forma, primero se reiniciaría antes de que se degrade siquiera, por lo cual no haríamos nada, la IA nos lo confirmó  
**Párrafos individuales explicando que comprendimos:**  
J:A lo largo de la actividad, comprendí en profundidad cómo opera el planificador por defecto Round Robin en XV6, los cambios estructurales necesarios para rediseñarlo mediante colas multinivel, también comprendí muy bien que problemas ataca el algoritmo MLFQ, sus reglas, y se me hizo muy interesante el entender cómo funcionaban los quantums, y como la CPU seleccionaba que proceso priorizar, en cuanto a mis aportes, colaboré activamente en el análisis de las cinco reglas del algoritmo, en la definición de los cambios requeridos para implementar el algoritmo MLFQ en XV6, en el desarrollo del informe, en la lógica de los escenarios de prueba y los comandos realizados para ver los archivos de XV6, para leerlos de mejor manera, siento que aún me falta muchísimo por aprender en cuanto a lo que se refiere de algoritmos de planificación, quiero indagar mas en cuales otros algoritmos hay, también necesito estudiar más el tema de manejar linux y una maquina virtual con linux, ya que aún se me complica un poco, debo ahondar también en cómo funciona una computadora a nivel de Kernel, me interesa mucho el tema  
F: Comprendí cómo funcionan las reglas de MLFQ más allá de lo básico, sobre todo a partir del caso que discutimos: un proceso A que usa 9 ms de CPU y duerme antes de agotar su quantum puede mantenerse en la cola alta para siempre mientras B, que consume quantums completos, baja de nivel y termina en inanición, y que la forma de medir el tiempo en cada nivel (tiempo de reloj o CPU acumulada) determina si ese comportamiento se corrige o no. Lo que aporté al grupo fue la descripción base del algoritmo: que organiza los procesos en varias colas con distintos niveles de prioridad, que siempre se ejecuta primero lo que esté en la cola de mayor prioridad y solo cuando esa cola queda vacía se atiende a las de abajo, que los procesos de una misma cola se turnan con Round Robin y que todo proceso nuevo entra en la cola más alta porque el planificador no sabe cuánto va a durar y le da el beneficio de la duda; junto con esa definición planteé las dudas que orientaron la discusión, como que no sabía cómo se decide cuántas colas debe haber ni qué quantum se asigna a cada una, y si un proceso podría aprovecharse del sistema soltando la CPU justo antes de que termine su quantum para no perder prioridad. Lo que aún necesito estudiar es cómo se configura todo eso en la práctica: cómo los sistemas reales eligen el número de colas, los quantums y el boost de prioridad, y cómo evitan el gaming sin terminar penalizando a los procesos interactivos legítimos.

[image1]: images/Imagen1.png
[image2]: images/Imagen2.png
[image3]: images/Imagen3.png
[image4]: images/Imagen4.png
[image5]: images/Imagen5.png
[image6]: images/Imagen6.png
[image7]: images/Imagen7.png
[image8]: images/Imagen8.png
[image9]: images/Imagen9.png
[image10]: images/Imagen10.png
[image11]: images/Imagen11.png
[image12]: images/Imagen12.png