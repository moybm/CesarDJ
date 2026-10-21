#include "Help.h"

// Contenido del manual de CesarDJ.
// Formato de cada texto: "## " = subtítulo, "- " = viñeta, "! " = nota importante, línea vacía = separación.
// Al añadir funciones nuevas, actualizar el apartado que corresponda y "Novedades".
const std::vector<Help::Section>& Help::sections()
{
    static const std::vector<Section> list = {

    { "inicio", "Primeros pasos", R"MANUAL(
CesarDJ es un programa para pinchar con dos platos (decks A y B), mezclador, efectos, sampler, stems (voz e instrumentos por separado), Automix, grabación, controladoras MIDI y tus listas de Spotify.

## En cuatro pasos
- CONFIG (arriba a la derecha) > Audio: elige por dónde suena. Si no tienes nada especial, pulsa "Altavoces del PC".
- "+ Carpeta" (abajo a la izquierda): añade la carpeta donde tienes tu música. Se escanea en segundo plano y se vigila: lo que añadas después aparece solo.
- "Analizar BPM": calcula el BPM, el beatgrid (rejilla de beats) y la tonalidad de todas las canciones. Se puede seguir pinchando mientras tanto.
- Arrastra una canción a un deck, o haz doble clic para cargarla en el deck libre, y pulsa PLAY.

## Dónde está cada cosa
- Barra superior: logo (clic = versión y carpeta de datos), estado (CPU, frecuencia, latencia, hora y mensajes), volumen master, AUTOMIX, REC (grabar), luz y botón MIDI, CONFIG y AYUDA.
- Debajo: las formas de onda de los dos decks, una encima de otra, para ver si los beats van a la par.
- Centro: deck A (azul), mezclador y deck B (rojo).
- Abajo: el navegador. A la izquierda el árbol (biblioteca, listas, carpetas, Spotify), en el centro las canciones y a la derecha la información y la carátula de la canción elegida.

## Este manual
- Pulsa F1 o el botón AYUDA en cualquier momento.
- El buscador de arriba a la izquierda encuentra palabras en todos los apartados y las resalta en amarillo.
- "Abrir para imprimir" (abajo) abre el manual completo en tu navegador, para leerlo con calma, imprimirlo o guardarlo como PDF.
- Si no sabes qué significa alguna palabra (BPM, cue, stems...), mira el Glosario.
)MANUAL" },

    { "audio", "Audio: tarjeta, master y auriculares", R"MANUAL(
Se configura en CONFIG > Audio.

## Configuración rápida
- Altavoces del PC: todo suena por la salida normal del ordenador. No hay pre-escucha por auriculares.
- Controladora o tarjeta con auriculares: para controladoras DJ con tarjeta de sonido integrada (Pioneer, Hercules, Numark, Reloop...) o tarjetas de 4 salidas. Salidas 1-2 = master (altavoces), 3-4 = auriculares. El dispositivo debe tener 4 salidas o más; si no, CesarDJ te avisa.
- PC + auriculares (cable divisor): si no tienes tarjeta externa, con un cable divisor en Y (un jack estéreo a dos jacks) el lado izquierdo lleva el master y el derecho los auriculares. Los dos suenan en mono.

## Tarjeta de sonido
- Controlador: el sistema de audio de Windows que se usa. "Windows Audio" funciona siempre; "Low Latency Mode" o "Exclusive Mode" dan menos retardo (en modo exclusivo, el resto de programas no suenan mientras CesarDJ está abierto).
- Dispositivo: la tarjeta o la controladora. Conéctala antes de abrir esta ventana.
- Frecuencia: 44,1 kHz o 48 kHz son lo normal.
- Búfer: cuanto más pequeño, menos retardo entre que tocas algo y lo oyes. Si se oyen cortes o chasquidos, súbelo. Entre 5 y 12 ms va bien en casi todos los PC.
- Panel del driver: abre el panel del fabricante, si lo tiene.
- Abajo verás el nombre del dispositivo, cuántas salidas tiene y la latencia aproximada.

## Salidas
- Master y Auriculares se pueden mandar a cualquier par de salidas de la tarjeta (hasta 8 salidas).
- "Probar master" y "Probar auriculares" suenan un pitido de 1,5 segundos por esa salida: así compruebas que cada cosa sale por donde debe.
- Todo se guarda solo y se recupera al abrir CesarDJ.

## Auriculares
- Volumen: volumen de los cascos, independiente del master (doble clic = al máximo).
- Mezcla CUE <-> MASTER: a la izquierda solo oyes lo que tienes en pre-escucha (botón de auriculares de cada canal del mezclador); a la derecha, solo el master (doble clic = a la izquierda).
- Si ningún canal tiene la pre-escucha activada, por los auriculares suena el master.

! CesarDJ usa el audio de Windows (no ASIO). Vale para casi todas las controladoras. Si alguna solo funciona con su driver ASIO, de momento no se puede usar en CesarDJ.
! Algunas controladoras aparecen en Windows como dos dispositivos separados (uno para el master y otro para los auriculares). CesarDJ solo puede abrir un dispositivo a la vez; en ese caso usa el que tenga 4 salidas o el cable divisor.
)MANUAL" },

    { "controladoras", "Controladoras MIDI", R"MANUAL(
Se configuran en CONFIG > Controladoras (o con el botón MIDI de arriba). La luz junto al botón MIDI parpadea cuando llega cualquier mensaje de una controladora.

## Conectar
- Conecta la controladora por USB. Aparece sola en la lista en unos segundos, aunque CesarDJ ya esté abierto. Arriba verás el aviso "Controladora conectada".
- La luz de cada controladora parpadea al tocar cualquiera de sus controles: así sabes que CesarDJ la recibe.
- La casilla de la izquierda la activa o desactiva (por ejemplo, si tienes un teclado MIDI conectado que no quieres usar).
- A la derecha de cada una pone cuántos controles tiene asignados.
- Si tiene tarjeta de sonido integrada, elígela también en CONFIG > Audio.

## Asignar controles (MIDI learn)
- Pulsa "Aprender (MIDI learn)" o Ctrl+M. La pantalla se oscurece un poco y los controles ya asignados se marcan en verde.
- Haz clic en un control de CesarDJ: un botón, un mando, un fader, un pad o un plato. Arriba pone cuál has elegido.
- Mueve o pulsa el control correspondiente en tu controladora. Queda asignado y se marca en verde.
- Repite con todos los que quieras. Esc o el mismo botón para terminar.
- Si asignas un control que ya estaba asignado, la asignación nueva sustituye a la anterior.

## Tabla de asignaciones
- Muestra cada control de CesarDJ, el mensaje MIDI (nota, CC o pitch bend y canal) y de qué controladora viene.
- Selecciona una fila y pulsa "Quitar" (o Supr) para borrarla.
- "Borrar de esta" borra las de la controladora seleccionada en la lista de arriba; si no hay ninguna seleccionada, "Borrar todas".
- Cada asignación va ligada a su controladora: con dos controladoras conectadas, un mismo botón de cada una puede hacer cosas distintas.

## Perfiles
- "Guardar perfil" guarda las asignaciones de esa controladora en un archivo .xml (en %APPDATA%\CesarDJ\Controladoras).
- Al volver a conectarla, en este PC o en otro (copiando el archivo a la misma carpeta), el perfil se carga solo si esa controladora aún no tiene asignaciones.
- "Cargar perfil" carga un perfil guardado para la controladora elegida (sustituye sus asignaciones).
- "Carpeta de perfiles" abre esa carpeta en el Explorador, para copiar perfiles o hacer copia de seguridad.

## Cómo responde cada tipo de control
- Botones: actúan al pulsar.
- Mandos y faders: siguen la posición del control de la controladora.
- Un botón asignado a un mando o fader lo devuelve a su posición central o de reposo.
- Pads: suenan mientras los mantienes pulsados (rolls, sampler...).
- Platos (jog wheels): funcionan con los dos formatos MIDI que usan las marcas (complemento a dos y centrado en 64), así que valen con cualquier modelo.

! CesarDJ no trae perfiles de fábrica para modelos concretos (Pioneer, Hercules, Numark...). Sin el aparato delante no se pueden comprobar sus mensajes MIDI, y un perfil con errores sería peor que ninguno. La primera vez asigna los controles con MIDI learn y guarda el perfil: a partir de ahí se carga solo.
! Las luces (LED) de la controladora no se encienden desde CesarDJ: solo se reciben los controles, no se envían.
)MANUAL" },

    { "biblioteca", "Biblioteca, listas y carpetas", R"MANUAL(
## Árbol de la izquierda
- Todas las canciones: toda tu biblioteca (el número es cuántas hay).
- Añadidas recientemente: las de los últimos 30 días.
- Historial: lo que has pinchado, con fecha y hora. Clic derecho para borrarlo.
- Automix: la cola de canciones que mezcla el Automix.
- Por conseguir: canciones de listas importadas que no tienes en tu ordenador (solo aparece si hay alguna).
- LISTAS: tus listas. Clic derecho en LISTAS para crear o importar; en una lista para renombrar (o doble clic), enviarla a Automix, exportarla a M3U o eliminarla.
- CARPETAS: las carpetas vigiladas. Clic derecho para volver a escanear, mostrarla en el Explorador o dejar de vigilarla.
- SPOTIFY: tus listas de Spotify (ver el apartado Spotify).

## Tabla de canciones
- Buscador: título, artista, álbum o género. Esc lo borra; Enter salta a la primera canción.
- BPM ±6 %: solo canciones con un BPM parecido al del deck que suena.
- ARMÓNICO: solo canciones de tonalidad compatible (rueda Camelot) con el deck que suena.
- Clic en el título de una columna para ordenar; arrastra los bordes para cambiar el ancho (se recuerda).
- Las barras de color a la izquierda de una fila indican que está cargada en el deck A (azul) o B (rojo). Un punto verde en el número indica que ya la has pinchado en esta sesión.
- BPM con "~" delante: aproximado (de las etiquetas del archivo); sin "~": analizado.
- Las estrellas de valoración se cambian con un clic (clic en la misma estrella = quitarla).
- Arrastra canciones a un deck para cargarlas o a una lista o a Automix para añadirlas.
- Doble clic o Enter: carga en el deck libre. Supr: quitar de la lista (o de la biblioteca, preguntando).
- Puedes soltar carpetas o archivos desde el Explorador de Windows.

## Clic derecho en una canción
- Cargar en deck A o B.
- Añadir a una lista (o crear una nueva con la selección).
- Añadir a Automix (o quitar de Automix).
- Volver a analizar BPM.
- Preparar stems con IA para las canciones seleccionadas.
- Mostrar en el Explorador.
- Quitar de la biblioteca.

! Quitar una canción de la biblioteca no borra el archivo del disco.
)MANUAL" },

    { "decks", "Decks: reproducir, cues, bucles y efectos", R"MANUAL(
## Transporte
- PLAY/PAUSA y CUE (en pausa fija el cue; sonando vuelve a él).
- SINC.: iguala el BPM del otro deck y alinea los beats.
- Emparejar: iguala solo el BPM con el otro deck.
- Pitch: el fader vertical (doble clic = 0 %). El botón de debajo cambia el rango: ±6, ±8, ±16 o ±50 %.
- KEY (key lock): cambia el tempo sin cambiar el tono.

## Formas de onda
- Las de arriba avanzan mientras suena la canción. Arrastrarlas con el ratón hace scratch; la rueda del ratón acerca o aleja la vista.
- Los colores indican graves (azul), medios (naranja) y agudos (blanco).
- La forma de onda completa de cada deck muestra la canción entera: clic (o arrastrar) para saltar a cualquier punto.

## Plato
- Centro: scratch. Aro exterior: empujar o frenar (para cuadrar los beats a mano). Rueda del ratón: ajuste fino.

## Pads
- HOT CUE: clic = fijar o saltar a ese punto; clic derecho = borrarlo. Se guardan con la canción.
- ROLLS: bucle mientras mantienes pulsado; al soltar, la canción sigue donde estaría.
- SALTO: salta beats hacia atrás o hacia delante.
- SAMPLER: ver el apartado Sampler.

## BPM y beatgrid
- Clic en el BPM: tap tempo (4 o más clics al ritmo).
- Clic derecho en el BPM: BPM × 2, BPM ÷ 2, "El primer beat está aquí" (en la posición actual), mover la rejilla 10 ms a la izquierda o a la derecha, y volver a analizar.
- Tonalidad en notación Camelot (8A, 11B...). En verde = combina con el otro deck.

## Efectos y bucles
- FILTRO: a la izquierda paso bajo, a la derecha paso alto, doble clic al centro.
- Dos efectos por deck. El mando controla la cantidad; clic en su nombre para elegir otro: FLANGER, CORTAR, ECO, REVERB o BITCRUSH.
- LOOP: el número central activa o desactiva un bucle de ese número de beats; < y > cambian el tamaño.
)MANUAL" },

    { "mezclador", "Mezclador", R"MANUAL(
- GAIN: ganancia de entrada de cada canal. Ajústala mirando el vúmetro para que las dos canciones suenen igual de fuerte.
- EQ de tres bandas (agudos, medios, graves). Del todo a la izquierda, la banda desaparece; a la derecha, se refuerza.
- Faders de canal y crossfader (de potencia constante: en el centro no baja el volumen).
- Botón de auriculares: pre-escucha del canal (necesita salida de auriculares, ver Audio).
- Botones EQ / STEMS: cambian los mandos entre ecualizador y VOCAL / INSTRU / BEAT (ver Stems).
- SAMPLER: volumen del sampler.
- Volumen master: arriba, en la barra superior.
- Un limitador final evita que la salida sature.
)MANUAL" },

    { "stems", "Stems: voz, instrumentos y percusión", R"MANUAL(
Con el botón STEMS del mezclador, los mandos pasan a VOCAL, INSTRU y BEAT: a la izquierda quitan ese elemento y a la derecha lo aíslan. Por ejemplo, VOCAL a la izquierda = versión instrumental (karaoke).

## Cómo funciona
- Al cargar una canción hay stems rápidos en segundos (separación aproximada).
- A la vez, la IA (Demucs, incluida dentro de CesarDJ, sin internet) prepara en segundo plano los de calidad y los cambia al terminar. En el mezclador verás "IA 40%" y después "IA".
- Los stems de IA se guardan en %LOCALAPPDATA%\CesarDJ\Stems y la próxima vez cargan al instante.
- Clic derecho en canciones > "Preparar stems con IA" para dejarlas listas por adelantado (por ejemplo, de noche). Se procesan de una en una.

! La IA tarda varios minutos por canción la primera vez (depende del ordenador; en un PC modesto puede ser más que la duración de la canción). Mientras tanto se usan los stems rápidos.
! La IA funciona en tu ordenador: no envía nada a internet. Los stems guardados ocupan espacio en disco (unos 12 MB por minuto de canción: unos 70 MB una canción de 6 minutos); puedes borrar esa carpeta cuando quieras y se vuelven a preparar si hacen falta.
)MANUAL" },

    { "automix", "Automix", R"MANUAL(
AUTOMIX mezcla sola la cola "Automix" del árbol, una canción tras otra.

- Añade canciones con clic derecho > "Añadir a Automix", arrastrándolas sobre "Automix" o con clic derecho en una lista > "Enviar lista a Automix".
- Pulsa AUTOMIX para empezar o parar. Arriba verás qué está haciendo.
- Iguala el BPM, alinea los beats e intercambia los graves en cada transición.
- Clic derecho en AUTOMIX: duración de la transición (4-32 beats) y "Mezclar ahora".
- Si dos canciones tienen tempos muy distintos, hace un fundido de 8 segundos antes del final.
- Puedes tocar cualquier control mientras mezcla.
)MANUAL" },

    { "sampler", "Sampler", R"MANUAL(
- Pon los pads de un deck en modo SAMPLER. Clic en un pad = suena.
- Clic derecho en un pad: cargar tu propio sonido, modo bucle (se repite hasta que lo detengas), detener o volver al sonido de serie.
- El mando SAMPLER del mezclador controla su volumen. El sampler va directo al master.
- Los sonidos que cargues se recuerdan al volver a abrir CesarDJ.
)MANUAL" },

    { "grabar", "Grabar la sesión", R"MANUAL(
- REC (arriba) empieza y termina la grabación.
- Se graba exactamente lo que sale por el master, en WAV de 24 bits.
- Los archivos se guardan en Música\CesarDJ Grabaciones, con la fecha y la hora en el nombre. Al terminar puedes abrir la carpeta.

! Graba solo para tu uso personal o si tienes derechos sobre la música: publicar mezclas (YouTube, Mixcloud...) puede necesitar licencias.
)MANUAL" },

    { "importar", "Importar y exportar listas", R"MANUAL(
Botón "Importar" (abajo a la izquierda) o clic derecho en LISTAS.

## De dónde
- Spotify: directamente desde tu cuenta (ver el apartado Spotify).
- Un archivo CSV: de Spotify (exportify.app) o de Apple Music, Deezer, TIDAL, YouTube Music... (tunemymusic.com). En el menú Importar, "Cómo conseguir el CSV..." lo explica paso a paso.
- Un archivo M3U o M3U8 de otro programa DJ o reproductor (rutas completas o relativas).

## Qué hace CesarDJ
- Busca cada canción en tu biblioteca comparando título, artista y duración. No confunde una versión remix o una "live" con la original.
- Crea la lista con las que encuentra, en el mismo orden. Si ya hay una lista con ese nombre, crea otra con un número.
- Al terminar te dice cuántas ha encontrado y cuántas faltan.
- Las que no encuentra van a "Por conseguir".

## Por conseguir
- Clic derecho (o doble clic) en una canción para buscarla en Beatport, Bandcamp, Traxsource, Amazon Music, YouTube o Google, o copiar "artista - título".
- Cuando la compres o descargues y esté en tu carpeta de música, se coloca sola en su sitio de la lista y desaparece de "Por conseguir".
- Clic derecho en "Por conseguir": buscar coincidencias ahora o vaciar la lista.

## Exportar
- Clic derecho en una lista > "Exportar a M3U" para llevarla a otro programa.

! Solo se importan los nombres de las canciones: el audio de las plataformas de streaming no se usa. La música tiene que estar en tu ordenador.
)MANUAL" },

    { "spotify", "Spotify: qué se puede y qué no", R"MANUAL(
## Iniciar sesión
- En el árbol, SPOTIFY > "Iniciar sesión con Spotify". Se abre la página oficial de Spotify dentro de CesarDJ: entra con tu correo, Google, Facebook o Apple, y pulsa "Aceptar".
- CesarDJ nunca ve tu contraseña: Spotify solo le da un permiso de lectura de tus listas, canciones que te gustan y álbumes.
- La ventana de inicio de sesión solo muestra las páginas para entrar. La web de desarrolladores de Spotify no se abre, y el resto de enlaces (registrarse, condiciones...) se abren en tu navegador normal.
- La sesión queda guardada al cerrar CesarDJ. Si un día abres CesarDJ sin internet, la sesión no se pierde.
- El permiso dura hasta 180 días ("Refresh token lifetime"). Después CesarDJ te avisa y vuelves a entrar con un par de clics.
- Si pulsas "Cancelar" en la página de Spotify, CesarDJ no se conecta y te lo dice.
- Clic derecho en SPOTIFY > "Cerrar sesión de Spotify": borra la sesión y las cookies, para poder entrar con otra cuenta.

## Qué ves
- Tu nombre de Spotify, "Canciones que me gustan", tus listas y tus álbumes guardados (con el número de canciones).
- Al abrir una, CesarDJ la compara con tu biblioteca. Punto verde: tienes esa canción en tu ordenador y CesarDJ la ha reconocido; se carga en A o B con doble clic o arrastrándola.
- Círculo amarillo "· falta": no está en tu ordenador. Doble clic para buscarla en tiendas. Si la arrastras a un deck, CesarDJ te explica por qué no se carga.
- Si seleccionas varias y las arrastras, se cargan o añaden solo las que tienes (verdes).
- Clic derecho en una lista > "Copiar a mis listas de CesarDJ": crea la lista y manda las que faltan a "Por conseguir".
- Clic derecho en SPOTIFY: "Actualizar listas" (si has cambiado algo en Spotify) e "Importar varias listas".

## Por qué no se pueden pinchar las canciones de Spotify en CesarDJ
- Spotify solo deja a las aplicaciones normales leer los nombres de canciones, listas y álbumes (su "Web API"). El audio no lo da.
- Su reproductor para desarrolladores suena dentro de un reproductor protegido que no se puede meter en un deck, y sus condiciones prohíben expresamente usarlo para mezclar o hacer de DJ.
- Desde septiembre de 2026, Spotify sí permite pinchar su catálogo con cuenta Premium en algunas aplicaciones con las que tiene un acuerdo comercial (VirtualDJ, Cross DJ, Edjing Mix). Ese acceso es exclusivo de esos acuerdos y no está disponible para una aplicación personal como CesarDJ.
- Capturar el audio de Spotify de otra forma sería saltarse su protección: es ilegal y Spotify podría cerrar la cuenta. CesarDJ no lo hace.
- Incluso en esas aplicaciones, las canciones de Spotify no se pueden separar en stems ni usar sin internet.

## Para pinchar tus listas de Spotify con CesarDJ
- Añade tu carpeta de música: lo que coincida pasa a verde.
- Lo que falte, cómpralo o descárgalo de una tienda (Beatport y Bandcamp dan archivos de calidad para DJ). En cuanto esté en tu carpeta, aparece en verde.

## Cuentas autorizadas
- CesarDJ está registrado en Spotify en "modo desarrollo". En ese modo solo pueden entrar las cuentas que el administrador de CesarDJ añade en developer.spotify.com > Dashboard > CesarDJ > User Management (nombre y correo de la cuenta). El número de cuentas permitidas es limitado y lo indica ese panel.
- Si una cuenta no está autorizada, Spotify deja iniciar sesión pero después responde con un error 403: CesarDJ lo avisa, explica qué hacer y no guarda esa sesión. Mientras tanto se pueden importar las listas con un CSV.
- Spotify puede exigir que la cuenta propietaria de la app (la que la registró) tenga Premium. Si esa cuenta deja de ser Premium (por ejemplo, al acabar una prueba gratuita), el inicio de sesión podría dejar de funcionar para todos. En ese caso revisa el aviso del panel de desarrollador.
- El Client ID de CesarDJ no es una contraseña: es el identificador público de la app. El "Client secret" no se usa nunca.

## Usar CesarDJ con la cuenta de otra persona (un amigo, otro PC)
- Esa persona instala CesarDJ y, antes de iniciar sesión, te pasa el nombre y el correo con los que entra en Spotify (lo ve en spotify.com > Cuenta).
- Tú (el administrador) entras en developer.spotify.com > Dashboard > CesarDJ > User Management y la añades con ese nombre y ese correo. Spotify puede tardar unos minutos en aplicarlo.
- Ella pulsa SPOTIFY > "Iniciar sesión con Spotify", entra con su cuenta y acepta. Ya está.
- La otra persona no tiene que crear nada en Spotify ni entrar en la web de desarrolladores.
- Si sigue saliendo el error 403, revisa que el correo sea exactamente el de su cuenta de Spotify.
! Si alguien te envía un texto que empieza por "Authorization token" o "const token =", es una página de ejemplo de la web de desarrolladores de Spotify, no un error de CesarDJ. Ese texto incluye una clave de acceso temporal a su cuenta: no hay que usarlo ni reenviarlo.
)MANUAL" },

    { "avisos", "Avisos y mensajes", R"MANUAL(
CesarDJ muestra un aviso la primera vez que haces algo importante. Cada aviso tiene:
- "Entendido": cierra el aviso.
- "Ver en el manual": abre el apartado que lo explica.
- "No volver a mostrar este aviso": si la marcas, ese aviso ya no vuelve a salir.

## Qué avisos hay
- Al iniciar sesión con Spotify por primera vez: qué va a pasar y que la cuenta tiene que estar autorizada.
- Al abrir una lista de Spotify: qué significan el punto verde y "falta", y por qué no se pueden pinchar sus canciones.
- Al hacer doble clic en una canción de Spotify que no tienes: por qué no se carga y dónde buscarla.
- Al entrar en CONFIG > Controladoras: que no hay perfiles de fábrica y cómo asignar los controles.
- Al elegir "Controladora o tarjeta con auriculares": cómo van las salidas y lo de ASIO.
- Al activar STEMS: cuánto tarda la IA y cómo dejarlos preparados.
- Al empezar a grabar: dónde se guarda y lo de los derechos de la música.

## Volver a verlos
- Abajo en este manual, "Volver a mostrar los avisos" hace que todos vuelvan a salir.

! Los mensajes de error (por ejemplo "cuenta no autorizada" o "sesión caducada") no tienen casilla: salen siempre que pasa el problema, y te indican qué apartado del manual leer.
)MANUAL" },

    { "atajos", "Atajos de teclado", R"MANUAL(
## Decks
- Deck A: Q = CUE, W = PLAY/PAUSA, 1 2 3 4 = hot cues 1 a 4.
- Deck B: O = CUE, P = PLAY/PAUSA, 7 8 9 0 = hot cues 1 a 4.

## General
- Ctrl+F = buscar en la biblioteca.
- Ctrl+M = MIDI learn. Esc = salir del MIDI learn.
- F1 = este manual.

## En la tabla de canciones
- Enter = cargar en el deck libre. Supr = quitar.
- En el buscador: Esc = borrar, Enter = ir a la primera canción.

## Con el ratón
- Doble clic en un mando o fader = volver a su posición central o de reposo.
- Rueda del ratón sobre el plato = ajuste fino; sobre las formas de onda = acercar o alejar.
)MANUAL" },

    { "datos", "Tus datos, actualizar y otro PC", R"MANUAL(
## Dónde se guarda todo
- %APPDATA%\CesarDJ: la biblioteca (library.db), los ajustes (CesarDJ.settings, incluida la sesión de Spotify) y los perfiles de controladoras.
- %LOCALAPPDATA%\CesarDJ: los stems de IA y las cookies de la ventana de Spotify.
- Música\CesarDJ Grabaciones: las grabaciones.
- Clic en el logo de arriba a la izquierda: versión de CesarDJ y botón para abrir la carpeta de datos.

## Instalar y actualizar
- Instalador: CesarDJ-Setup-X.Y.Z.exe. No necesita permisos de administrador.
- Portable: descomprime CesarDJ-X.Y.Z-portable.zip en cualquier carpeta o en un USB y abre CesarDJ.exe.
- Para actualizar, instala la versión nueva encima (o sustituye el .exe del portable): la biblioteca, las listas, los cues, el sampler, las asignaciones y la sesión de Spotify se conservan.
- Desinstalar no borra tus datos.

## Descargar versiones nuevas
- Cada versión se publica en GitHub: github.com/moybm/CesarDJ > Releases, con el instalador y el portable.
- El repositorio es privado: para descargar hay que entrar en GitHub con la cuenta del administrador. A otra persona, pásale el instalador directamente.

## Otro ordenador
- Funciona en cualquier Windows 10 u 11 de 64 bits. La ventana de Spotify usa Microsoft Edge WebView2, que suele venir ya instalado en Windows 10 y 11; si faltara, CesarDJ abre el inicio de sesión en tu navegador normal.
- Para llevarte tu biblioteca, copia la carpeta %APPDATA%\CesarDJ al mismo sitio del otro PC (las canciones tienen que estar en las mismas rutas).
- Con otra cuenta de Spotify: basta con que esa cuenta esté autorizada (ver Spotify > Usar CesarDJ con la cuenta de otra persona).

! Si Windows muestra "Windows protegió su PC", pulsa "Más información" y "Ejecutar de todas formas" (el programa no está firmado digitalmente).
)MANUAL" },

    { "limitaciones", "Limitaciones conocidas", R"MANUAL(
Lo que CesarDJ no hace, y por qué:
- No reproduce música de Spotify ni de otras plataformas de streaming: no dan acceso al audio a aplicaciones sin acuerdo comercial (ver Spotify).
- Spotify solo admite un número limitado de cuentas autorizadas, que hay que añadir a mano (ver Spotify > Cuentas autorizadas).
- No trae perfiles de fábrica de controladoras concretas: se asignan con MIDI learn y se guardan como perfil (ver Controladoras).
- No enciende las luces (LED) de las controladoras.
- No usa drivers ASIO: usa el audio de Windows (ver Audio).
- Solo abre un dispositivo de audio a la vez.
- La IA de stems tarda varios minutos por canción la primera vez.
- No busca actualizaciones sola: las versiones nuevas se descargan de GitHub (ver Tus datos).
)MANUAL" },

    { "problemas", "Solución de problemas", R"MANUAL(
## No suena nada
- CONFIG > Audio: comprueba el dispositivo y pulsa "Probar master".
- Mira que el volumen master (arriba), los faders de canal y el crossfader estén bien colocados.
- Si has conectado o desconectado la tarjeta con CesarDJ abierto, vuelve a elegirla en CONFIG > Audio.

## Se oyen cortes o chasquidos
- CONFIG > Audio > Búfer: súbelo un escalón.
- Cierra programas pesados mientras pinchas. Si la IA de stems está trabajando, espera a que termine o pínchalo más tarde.

## No oigo los auriculares
- CONFIG > Audio > Auriculares: debe haber una salida elegida (o el divisor). Pulsa "Probar auriculares".
- Activa el botón de auriculares del canal en el mezclador y revisa el volumen y la mezcla CUE/MASTER.

## El BPM o los beats no cuadran
- Clic derecho en el BPM: prueba BPM × 2 o ÷ 2, o "El primer beat está aquí" en un golpe claro.
- Clic derecho en la canción > "Volver a analizar BPM".

## La controladora no responde
- CONFIG > Controladoras: debe aparecer en la lista, con la casilla activada. Toca un control: la luz tiene que parpadear.
- Si la luz parpadea pero no hace nada, ese control no está asignado: usa MIDI learn.
- Si no aparece, desconéctala y vuelve a conectarla, o prueba otro puerto USB.

## Spotify
- Error 403: la cuenta no está autorizada. El administrador de CesarDJ debe añadirla (ver Spotify > Usar CesarDJ con la cuenta de otra persona).
- Ha salido un texto con "const token =": es la web de desarrolladores de Spotify, no un error. Cierra esa ventana y vuelve a iniciar sesión.
- "La sesión ha caducado": vuelve a iniciar sesión.
- No carga las listas: comprueba la conexión a internet y usa "Actualizar listas".
- La ventana de inicio de sesión se queda en blanco: comprueba la conexión a internet. Si sigue igual, cierra la ventana y vuelve a pulsar "Iniciar sesión con Spotify".
- Una canción que tienes sale como "falta": revisa que el título y el artista de tu archivo se parezcan a los de Spotify (CesarDJ no empareja versiones distintas, como un remix con la original).

## Para pedir ayuda
- Haz una captura de pantalla (tecla Windows + Mayús + S) del mensaje y de la ventana donde sale, mejor que copiar el texto.
- Indica la versión de CesarDJ (clic en el logo de arriba a la izquierda).
)MANUAL" },

    { "glosario", "Glosario", R"MANUAL(
- BPM: pulsaciones (beats) por minuto, la velocidad de la canción.
- Beatgrid (rejilla): las marcas de cada beat sobre la canción. SINC., los bucles y el Automix la usan.
- Cue: punto de la canción desde el que empezar. Hot cue: punto guardado en un pad para saltar a él al instante.
- Pitch: la velocidad de reproducción. Key lock (KEY): cambiar la velocidad sin que cambie el tono.
- Tonalidad / Camelot: la nota musical de la canción, en un código de número y letra (8A, 11B...). Las vecinas en la rueda combinan bien.
- Mezcla armónica: pinchar seguidas canciones de tonalidad compatible.
- Crossfader: el fader horizontal que pasa de un deck al otro.
- GAIN: el volumen de entrada de cada canal, antes del fader.
- EQ (ecualizador): graves, medios y agudos de cada canal.
- Pre-escucha (PFL, CUE de auriculares): oír un canal por los auriculares sin que suene por los altavoces.
- Master: lo que suena por los altavoces (y lo que se graba).
- Stems: la canción separada en voz, instrumentos y percusión.
- Loop (bucle): repetir un trozo de la canción. Roll: bucle mientras mantienes pulsado.
- Scratch: mover la canción adelante y atrás con el plato.
- Latencia / búfer: el retardo entre que tocas algo y lo oyes.
- MIDI: el idioma con el que las controladoras hablan con el ordenador. MIDI learn: enseñar a CesarDJ qué hace cada control.
- Controladora DJ: aparato con platos, faders y botones que se conecta por USB.
- ASIO: un tipo de driver de audio de baja latencia que CesarDJ no usa (usa el de Windows).
- CSV / M3U: archivos de texto con listas de canciones.
- Release: una versión publicada para descargar, con su instalador.
)MANUAL" },

    { "novedades", "Novedades (historial de versiones)", R"MANUAL(
## 0.12.2
- Manual revisado y ampliado: glosario, avisos, atajos, formas de onda, menús, descargas y más soluciones de problemas.
- El buscador del manual resalta las palabras encontradas.
- Arrastrar a un deck una canción que falta (Spotify, "Por conseguir") muestra por qué no se carga, en vez de no hacer nada.
- "Acerca de" (clic en el logo): botones para abrir la carpeta de datos y el manual.
- "Abrir para imprimir": el manual completo en el navegador.

## 0.12.1
- La ventana de inicio de sesión de Spotify solo abre las páginas para entrar (la web de desarrolladores se bloquea).
- Cuenta de Spotify no autorizada: mensaje claro y no se guarda la sesión.
- Aviso antes del primer inicio de sesión con Spotify.

## 0.12.0
- Manual de ayuda integrado (AYUDA / F1) y avisos con "No volver a mostrar".

## 0.11.0
- Ventana de Configuración: Audio (master y auriculares a cualquier salida, divisor, pruebas) y Controladoras (MIDI learn, asignaciones por controladora, perfiles).

## 0.10.0
- Spotify integrado: inicio de sesión dentro de CesarDJ, tus listas, canciones que te gustan y álbumes.

## 0.9.0
- Importar listas (Spotify, CSV, M3U) y "Por conseguir".

## 0.8.0
- Stems: VOCAL / INSTRU / BEAT con IA.

## 0.7.0
- Automix.

## 0.6.0
- Marca CesarDJ, tonalidad (Camelot), beatgrid editable, instalador y versión portable.

## Versiones anteriores
- Dos decks, mezclador con EQ, formas de onda, efectos, bucles, hot cues, sampler, grabación, biblioteca con listas y carpetas, y controladoras MIDI.
)MANUAL" },
    };
    return list;
}
