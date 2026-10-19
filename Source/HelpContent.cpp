#include "Help.h"

// Contenido del manual de CesarDJ.
// Formato de cada texto: "## " = subtítulo, "- " = viñeta, "! " = nota importante, línea vacía = separación.
const std::vector<Help::Section>& Help::sections()
{
    static const std::vector<Section> list = {

    { "inicio", "Primeros pasos", R"MANUAL(
CesarDJ es un programa para pinchar con dos platos (decks A y B), mezclador, efectos, sampler, stems, Automix y grabación.

## En cuatro pasos
- CONFIG (arriba a la derecha) > Audio: elige por dónde suena. Si no tienes nada especial, pulsa "Altavoces del PC".
- "+ Carpeta" (abajo a la izquierda): añade la carpeta donde tienes tu música. Se escanea en segundo plano.
- "Analizar BPM": calcula el BPM, el beatgrid (rejilla de beats) y la tonalidad de todas las canciones.
- Arrastra una canción a un deck, o haz doble clic para cargarla en el deck libre, y pulsa PLAY.

## Dónde está cada cosa
- Arriba: formas de onda de los dos decks, volumen master, AUTOMIX, REC (grabar), MIDI, CONFIG y AYUDA.
- Centro: deck A (azul), mezclador y deck B (rojo).
- Abajo: el navegador. A la izquierda el árbol (biblioteca, listas, carpetas, Spotify), en el centro las canciones y a la derecha la información de la canción elegida.

! Pulsa F1 o el botón AYUDA en cualquier momento para abrir este manual.
)MANUAL" },

    { "audio", "Audio: tarjeta, master y auriculares", R"MANUAL(
Se configura en CONFIG > Audio.

## Configuración rápida
- Altavoces del PC: todo suena por la salida normal del ordenador. No hay pre-escucha por auriculares.
- Controladora o tarjeta con auriculares: para controladoras DJ con tarjeta de sonido integrada (Pioneer, Hercules, Numark, Reloop...) o tarjetas de 4 salidas. Salidas 1-2 = master (altavoces), 3-4 = auriculares. El dispositivo debe tener 4 salidas o más; si no, CesarDJ te avisa.
- PC + auriculares (cable divisor): si no tienes tarjeta externa, con un cable divisor en Y (un jack estéreo a dos jacks) el lado izquierdo lleva el master y el derecho los auriculares. Los dos suenan en mono.

## Tarjeta de sonido
- Controlador: el sistema de audio de Windows que se usa. "Windows Audio" funciona siempre; "Low Latency Mode" o "Exclusive Mode" dan menos retardo.
- Dispositivo: la tarjeta o la controladora. Conéctala antes de abrir esta ventana.
- Frecuencia: 44,1 kHz o 48 kHz son lo normal.
- Búfer: cuanto más pequeño, menos retardo entre que tocas algo y lo oyes. Si se oyen cortes o chasquidos, súbelo.
- Panel del driver: abre el panel del fabricante, si lo tiene.

## Salidas
- Master y Auriculares se pueden mandar a cualquier par de salidas de la tarjeta (hasta 8 salidas).
- "Probar master" y "Probar auriculares" suenan un pitido de 1,5 segundos por esa salida: así compruebas que cada cosa sale por donde debe.

## Auriculares
- Volumen: volumen de los cascos, independiente del master.
- Mezcla CUE <-> MASTER: a la izquierda solo oyes lo que tienes en pre-escucha (botón de auriculares de cada canal del mezclador); a la derecha, solo el master.
- Si ningún canal tiene la pre-escucha activada, por los auriculares suena el master.

! CesarDJ usa el audio de Windows (no ASIO). Vale para casi todas las controladoras. Si alguna solo funciona con su driver ASIO, de momento no se puede usar en CesarDJ.
! Algunas controladoras aparecen en Windows como dos dispositivos separados (uno para el master y otro para los auriculares). CesarDJ solo puede abrir un dispositivo a la vez; en ese caso usa el que tenga 4 salidas o el cable divisor.
)MANUAL" },

    { "controladoras", "Controladoras MIDI", R"MANUAL(
Se configuran en CONFIG > Controladoras (o con el botón MIDI de arriba).

## Conectar
- Conecta la controladora por USB. Aparece sola en la lista en unos segundos.
- La luz de cada controladora parpadea al tocar cualquiera de sus controles: así sabes que CesarDJ la recibe.
- La casilla de la izquierda la activa o desactiva.
- Si tiene tarjeta de sonido integrada, elígela también en CONFIG > Audio.

## Asignar controles (MIDI learn)
- Pulsa "Aprender (MIDI learn)" o Ctrl+M. La pantalla se oscurece un poco.
- Haz clic en un control de CesarDJ: un botón, un mando, un fader, un pad o un plato.
- Mueve o pulsa el control correspondiente en tu controladora. Queda asignado y se marca en verde.
- Repite con todos los que quieras. Esc o el mismo botón para terminar.
- En la tabla "Asignaciones" ves todo lo asignado; selecciona una fila y pulsa "Quitar" (o Supr) para borrarla.

## Perfiles
- "Guardar perfil" guarda las asignaciones de esa controladora en un archivo (en %APPDATA%\CesarDJ\Controladoras).
- Al volver a conectarla, en este PC o en otro (copiando el archivo a la misma carpeta), el perfil se carga solo.
- "Cargar perfil" carga un perfil guardado para la controladora elegida.

## Platos (jog wheels)
- Los platos funcionan con los dos formatos MIDI que usan las marcas (complemento a dos y centrado en 64), así que valen con cualquier modelo.

! CesarDJ no trae perfiles de fábrica para modelos concretos (Pioneer, Hercules, Numark...). Sin el aparato delante no se pueden comprobar sus mensajes MIDI, y un perfil con errores sería peor que ninguno. La primera vez asigna los controles con MIDI learn y guarda el perfil: a partir de ahí se carga solo.
! Las luces (LED) de la controladora no se encienden desde CesarDJ: solo se reciben los controles, no se envían.
)MANUAL" },

    { "biblioteca", "Biblioteca, listas y carpetas", R"MANUAL(
## Árbol de la izquierda
- Todas las canciones, Añadidas recientemente e Historial (lo que has pinchado).
- Automix: la cola de canciones que mezcla el Automix.
- Por conseguir: canciones de listas importadas que no tienes en tu ordenador.
- LISTAS: tus listas. Clic derecho para crear, renombrar, exportar a M3U o borrar.
- CARPETAS: las carpetas vigiladas. Clic derecho para volver a escanear o dejar de vigilar.
- SPOTIFY: tus listas de Spotify (ver el apartado Spotify).

## Tabla de canciones
- Buscador: título, artista, álbum o género.
- BPM ±6 %: solo canciones con un BPM parecido al del deck que suena.
- ARMÓNICO: solo canciones de tonalidad compatible (rueda Camelot) con el deck que suena.
- Arrastra canciones a un deck para cargarlas o a una lista para añadirlas.
- Doble clic o Enter: carga en el deck libre. Clic derecho: más opciones.
- Las estrellas de valoración se cambian con un clic.
- Puedes soltar carpetas o archivos desde el Explorador de Windows.

! Quitar una canción de la biblioteca no borra el archivo del disco.
)MANUAL" },

    { "decks", "Decks: reproducir, cues, bucles y efectos", R"MANUAL(
## Transporte
- PLAY/PAUSA y CUE (en pausa fija el cue; sonando vuelve a él).
- SINC.: iguala el BPM del otro deck y alinea los beats.
- Emparejar: iguala solo el BPM con el otro deck.
- Pitch: el fader vertical (doble clic = 0 %). El botón de debajo cambia el rango: ±6, ±8, ±16 o ±50 %.
- KEY (key lock): cambia el tempo sin cambiar el tono.

## Plato
- Centro: scratch. Aro exterior: empujar o frenar. Rueda del ratón: ajuste fino.

## Pads
- HOT CUE: clic = fijar o saltar a ese punto; clic derecho = borrarlo.
- ROLLS: bucle mientras mantienes pulsado; al soltar, la canción sigue donde estaría.
- SALTO: salta beats hacia atrás o hacia delante.
- SAMPLER: ver el apartado Sampler.

## BPM y beatgrid
- Clic en el BPM: tap tempo (4 o más clics al ritmo).
- Clic derecho en el BPM: x2, /2, mover la rejilla o volver a analizar.
- Tonalidad en notación Camelot (8A, 11B...). En verde = combina con el otro deck.

## Efectos y bucles
- Dos efectos por deck. Clic en el nombre de un efecto para cambiarlo (Flanger, Cortar, Eco, Reverb, Bitcrush...).
- FILTRO: a la izquierda paso bajo, a la derecha paso alto, doble clic al centro.
- LOOP: el número central activa o desactiva un bucle de ese número de beats; < y > cambian el tamaño.
)MANUAL" },

    { "mezclador", "Mezclador", R"MANUAL(
- GAIN: ganancia de entrada de cada canal (mira el vúmetro).
- EQ de tres bandas (agudos, medios, graves). Del todo a la izquierda, la banda desaparece.
- Faders de canal y crossfader (de potencia constante: en el centro no baja el volumen).
- Botón de auriculares: pre-escucha del canal (necesita salida de auriculares, ver Audio).
- Botón STEMS: los mandos de EQ pasan a VOCAL / INSTRU / BEAT (ver Stems).
- Volumen master: arriba, en la barra superior.
- Un limitador final evita que la salida sature.
)MANUAL" },

    { "stems", "Stems: voz, instrumentos y percusión", R"MANUAL(
Con el botón STEMS del mezclador, los mandos pasan a VOCAL, INSTRU y BEAT: a la izquierda quitan ese elemento y a la derecha lo aíslan.

## Cómo funciona
- Al cargar una canción hay stems rápidos en segundos (separación aproximada).
- A la vez, la IA (Demucs, incluida dentro de CesarDJ, sin internet) prepara en segundo plano los de calidad y los cambia al terminar. En el mezclador verás "IA 40%" y después "IA".
- Los stems de IA se guardan en %LOCALAPPDATA%\CesarDJ\Stems y la próxima vez cargan al instante.
- Clic derecho en canciones > "Preparar stems con IA" para dejarlas listas por adelantado (por ejemplo, de noche).

! La IA tarda varios minutos por canción la primera vez (depende del ordenador; en un PC modesto puede ser más que la duración de la canción). Mientras tanto se usan los stems rápidos.
! La IA funciona en tu ordenador: no envía nada a internet.
)MANUAL" },

    { "automix", "Automix", R"MANUAL(
AUTOMIX mezcla sola la cola "Automix" del árbol.

- Añade canciones con clic derecho > "Añadir a Automix", arrastrándolas sobre "Automix" o con clic derecho en una lista > "Enviar lista a Automix".
- Iguala el BPM, alinea los beats e intercambia los graves en cada transición.
- Clic derecho en AUTOMIX: duración de la transición (4-32 beats) y "Mezclar ahora".
- Si dos canciones tienen tempos muy distintos, hace un fundido de 8 segundos antes del final.
- Puedes tocar cualquier control mientras mezcla.
)MANUAL" },

    { "sampler", "Sampler", R"MANUAL(
- Pon los pads de un deck en modo SAMPLER.
- Clic derecho en un pad para cargar tu propio sonido (y elegir si se repite en bucle).
- El mando SAMPLER del mezclador controla su volumen. El sampler va directo al master.
)MANUAL" },

    { "grabar", "Grabar la sesión", R"MANUAL(
- REC (arriba) empieza y termina la grabación.
- Se graba exactamente lo que sale por el master, en WAV de 24 bits.
- Los archivos se guardan en Música\CesarDJ Grabaciones. Al terminar puedes abrir la carpeta.

! Graba solo para tu uso personal o si tienes derechos sobre la música: publicar mezclas puede necesitar licencias.
)MANUAL" },

    { "importar", "Importar y exportar listas", R"MANUAL(
Botón "Importar" (abajo a la izquierda) o clic derecho en LISTAS.

## De dónde
- Spotify: directamente desde tu cuenta (ver el apartado Spotify).
- Un archivo CSV: de Spotify (exportify.app) o de Apple Music, Deezer, TIDAL, YouTube Music... (tunemymusic.com).
- Un archivo M3U o M3U8 de otro programa DJ o reproductor.

## Qué hace CesarDJ
- Busca cada canción en tu biblioteca comparando título, artista y duración (no confunde una versión remix con la original).
- Crea la lista con las que encuentra, en el mismo orden.
- Las que no encuentra van a "Por conseguir".

## Por conseguir
- Clic derecho (o doble clic) en una canción para buscarla en Beatport, Bandcamp, Traxsource, Amazon Music, YouTube o Google.
- Cuando la compres o descargues y esté en tu carpeta de música, se coloca sola en su sitio de la lista.

## Exportar
- Clic derecho en una lista > "Exportar a M3U" para llevarla a otro programa.

! Solo se importan los nombres de las canciones: el audio de las plataformas de streaming no se usa. La música tiene que estar en tu ordenador.
)MANUAL" },

    { "spotify", "Spotify: qué se puede y qué no", R"MANUAL(
## Iniciar sesión
- En el árbol, SPOTIFY > "Iniciar sesión con Spotify". Se abre la página oficial de Spotify dentro de CesarDJ: entra con tu correo, Google, Facebook o Apple, y acepta.
- CesarDJ nunca ve tu contraseña: Spotify solo le da un permiso de lectura.
- La sesión queda guardada al cerrar CesarDJ. Si un día abres CesarDJ sin internet, la sesión no se pierde.
- El permiso dura hasta 180 días ("Refresh token lifetime"). Después CesarDJ te avisa y vuelves a entrar con un par de clics.
- Clic derecho en SPOTIFY > "Cerrar sesión de Spotify": borra la sesión y las cookies, para poder entrar con otra cuenta.

## Qué ves
- Tus listas, "Canciones que me gustan" y tus álbumes guardados.
- Punto verde: tienes esa canción en tu ordenador y CesarDJ la ha reconocido. Se carga en A o B con doble clic o arrastrándola.
- Círculo amarillo "· falta": no está en tu ordenador. Doble clic para buscarla en tiendas.
- Clic derecho en una lista > "Copiar a mis listas de CesarDJ": crea la lista y manda las que faltan a "Por conseguir".

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
- CesarDJ está registrado en Spotify en "modo desarrollo". En ese modo solo pueden entrar las cuentas que el administrador de CesarDJ añade en developer.spotify.com > CesarDJ > User Management (nombre y correo de la cuenta). El número de cuentas permitidas es limitado y lo indica ese panel.
- Si una cuenta no está autorizada, Spotify responde con un error 403. Mientras tanto se pueden importar las listas con un CSV.
- Spotify puede exigir que la cuenta propietaria de la app (la que la registró) tenga Premium. Si esa cuenta deja de ser Premium (por ejemplo, al acabar una prueba gratuita), el inicio de sesión podría dejar de funcionar para todos. En ese caso revisa el aviso del panel de desarrollador.
- El Client ID de CesarDJ no es una contraseña: es el identificador público de la app. El "Client secret" no se usa nunca.
)MANUAL" },

    { "atajos", "Atajos de teclado", R"MANUAL(
- Deck A: Q = CUE, W = PLAY/PAUSA, 1 2 3 4 = hot cues.
- Deck B: O = CUE, P = PLAY/PAUSA, 7 8 9 0 = hot cues.
- Ctrl+F = buscar en la biblioteca.
- Ctrl+M = MIDI learn. Esc = salir del MIDI learn.
- F1 = este manual.
)MANUAL" },

    { "datos", "Tus datos, actualizar y otro PC", R"MANUAL(
## Dónde se guarda todo
- %APPDATA%\CesarDJ: la biblioteca (library.db), los ajustes (CesarDJ.settings) y los perfiles de controladoras.
- %LOCALAPPDATA%\CesarDJ: los stems de IA y la sesión de la ventana de Spotify.
- Música\CesarDJ Grabaciones: las grabaciones.

## Instalar y actualizar
- Instalador: CesarDJ-Setup-X.Y.Z.exe. No necesita permisos de administrador.
- Portable: descomprime el zip en cualquier carpeta o en un USB y abre CesarDJ.exe.
- Para actualizar, instala la versión nueva encima: la biblioteca, las listas, los cues, el sampler y las asignaciones se conservan.
- Desinstalar no borra tus datos.

## Otro ordenador
- Funciona en cualquier Windows 10 u 11 de 64 bits.
- Para llevarte tu biblioteca, copia la carpeta %APPDATA%\CesarDJ al mismo sitio del otro PC.
- Con otra cuenta de Spotify: basta con que esa cuenta esté autorizada (ver Spotify > Cuentas autorizadas).

! Si Windows muestra "Windows protegió su PC", pulsa "Más información" y "Ejecutar de todas formas" (el programa no está firmado digitalmente).
)MANUAL" },

    { "limitaciones", "Limitaciones conocidas", R"MANUAL(
Lo que CesarDJ no hace, y por qué:
- No reproduce música de Spotify ni de otras plataformas de streaming: no dan acceso al audio a aplicaciones sin acuerdo comercial (ver Spotify).
- No trae perfiles de fábrica de controladoras concretas: se asignan con MIDI learn y se guardan como perfil (ver Controladoras).
- No enciende las luces (LED) de las controladoras.
- No usa drivers ASIO: usa el audio de Windows (ver Audio).
- Solo abre un dispositivo de audio a la vez.
- La IA de stems tarda varios minutos por canción la primera vez.
)MANUAL" },

    { "problemas", "Solución de problemas", R"MANUAL(
## No suena nada
- CONFIG > Audio: comprueba el dispositivo y pulsa "Probar master".
- Mira que el volumen master (arriba) y los faders de canal estén subidos.

## Se oyen cortes o chasquidos
- CONFIG > Audio > Búfer: súbelo un escalón.
- Cierra programas pesados mientras pinchas.

## No oigo los auriculares
- CONFIG > Audio > Auriculares: debe haber una salida elegida (o el divisor). Pulsa "Probar auriculares".
- Activa el botón de auriculares del canal en el mezclador y revisa el volumen y la mezcla CUE/MASTER.

## La controladora no responde
- CONFIG > Controladoras: debe aparecer en la lista, con la casilla activada. Toca un control: la luz tiene que parpadear.
- Si la luz parpadea pero no hace nada, ese control no está asignado: usa MIDI learn.
- Si no aparece, desconéctala y vuelve a conectarla, o prueba otro puerto USB.

## Spotify
- Error 403: la cuenta no está autorizada (ver Spotify > Cuentas autorizadas).
- "La sesión ha caducado": vuelve a iniciar sesión.
- No carga las listas: comprueba la conexión a internet y usa "Actualizar listas".

## Avisos
- Si marcaste "No volver a mostrar" en algún aviso y quieres verlos otra vez, pulsa "Volver a mostrar los avisos" en la parte de abajo de este manual.
)MANUAL" },
    };
    return list;
}
