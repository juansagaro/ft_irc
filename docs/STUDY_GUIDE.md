# Guia de Estudio - ft_irc

> Esta guia cubre el 100% del codigo del proyecto ft_irc. Esta pensada para que
> entiendas cada linea antes de la defensa. Leela en el orden propuesto.

---

## Indice

1. [Vision general y arquitectura](#1-vision-general-y-arquitectura)
2. [Punto de entrada: main.cpp](#2-punto-de-entrada-maincpp)
3. [El header maestro: Irc.hpp](#3-el-header-maestro-irchpp)
4. [Clase Server: Server.hpp y Server.cpp](#4-clase-server-serverhpp-y-servercpp)
5. [Capa de red: Network.cpp](#5-capa-de-red-networkcpp)
6. [Clase Client: Client.hpp y Client.cpp](#6-clase-client-clienthpp-y-clientcpp)
7. [Clase Channel: Channel.hpp y Channel.cpp](#7-clase-channel-channelhpp-y-channelcpp)
8. [Parsing y registro: Parse.cpp](#8-parsing-y-registro-parsecpp)
9. [Respuestas numericas: Replies.hpp](#9-respuestas-numericas-replieshpp)
10. [Comandos de autenticacion: CmdAuth.cpp](#10-comandos-de-autenticacion-cmdauthcpp)
11. [Comandos de canal: CmdChannel.cpp](#11-comandos-de-canal-cmdchannelcpp)
12. [Comandos de mensajeria: CmdMessage.cpp](#12-comandos-de-mensajeria-cmdmessagecpp)
13. [Flujos completos (de principio a fin)](#13-flujos-completos-de-principio-a-fin)
14. [Preguntas de defensa y respuestas](#14-preguntas-de-defensa-y-respuestas)
15. [Bonus: File Transfer (DCC) y Bot (Marvin)](#15-bonus-file-transfer-dcc-y-bot-marvin)
16. [Guia de testing](#16-guia-de-testing)

---

## 1. Vision general y arquitectura

### Que es este proyecto

Un servidor IRC escrito en C++98. Los clientes (irssi, WeeChat, HexChat, o incluso
`nc`/`socat`) se conectan por TCP al puerto indicado, se autentican con contraseña,
eligen nick/user, y pueden chatear en canales o en privado.

### Mapa de archivos

```
include/
  Irc.hpp          -- Header maestro: includes del sistema, constantes, forward declarations
  Replies.hpp      -- Constantes numericas IRC (RPL::WELCOME, ERR::NOSUCHNICK, etc.)
  Server.hpp       -- Clase Server: toda la logica del servidor
  Client.hpp       -- Clase Client: estado de cada conexion
  Channel.hpp      -- Clase Channel: un canal IRC (#general, etc.)
  Bot.hpp          -- Clase Bot: pseudo-cliente interno "Marvin"

src/
  main.cpp         -- Punto de entrada, validacion de args, señales
  Server.cpp       -- Constructor/destructor de Server, boot(), run(), helpers, botReply()
  Network.cpp      -- Sockets: crear, aceptar conexiones, recibir datos, desconectar
  Client.cpp       -- Implementacion de Client (getters, setters, buffer, fullId)
  Channel.cpp      -- Implementacion de Channel (miembros, modos, relay)
  Parse.cpp        -- Parser de mensajes IRC + dispatcher de comandos
  CmdAuth.cpp      -- Comandos: PASS, NICK, USER, QUIT, PING
  CmdChannel.cpp   -- Comandos: JOIN, PART, TOPIC, MODE (i,t,k,o,l)
  CmdMessage.cpp   -- Comandos: PRIVMSG, NOTICE, KICK, INVITE
  Bot.cpp          -- Implementacion del bot Marvin (11 comandos)
```

### Diagrama de flujo

```
                         main.cpp
                            |
                    Server(port, pass)
                            |
                         boot()
                            |
                      openSocket()   <-- Network.cpp
                            |
                         run()       <-- Server.cpp
                            |
                   +--------+--------+
                   |                 |
            poll() loop         SIGINT? --> salir
                   |
          +--------+--------+
          |                 |
   fd == listenFd?    fd == cliente?
          |                 |
   welcomeClient()    receiveData()    <-- Network.cpp
                            |
                     bufferAppend()    <-- Client.cpp
                            |
                   linea completa? (\n)
                            |
                     routeMessage()    <-- Parse.cpp
                            |
              +---------+---+---+---------+
              |         |       |         |
          CmdAuth   CmdChannel  CmdMsg   ...
          PASS      JOIN        PRIVMSG
          NICK      PART        NOTICE
          USER      TOPIC       KICK
          QUIT      MODE        INVITE
          PING
```

### Las 3 estructuras de datos centrales del Server

```cpp
std::vector<struct pollfd>       _pollSet;    // fds vigilados por poll()
std::map<int, Client*>           _sessions;   // fd -> Client*
std::map<std::string, Channel*>  _rooms;      // "#nombre" -> Channel*
```

- `_pollSet`: es lo que le pasamos a `poll()`. Cada entrada tiene un fd y los eventos
  que nos interesan (POLLIN). El primer elemento es siempre el socket de escucha.
- `_sessions`: mapea cada fd de cliente a su objeto Client. Si un fd no esta aqui,
  no es un cliente valido.
- `_rooms`: mapea cada nombre de canal a su objeto Channel. Los canales se crean
  al hacer JOIN y se destruyen cuando el ultimo usuario sale.

---

## 2. Punto de entrada: main.cpp

**Archivo:** `src/main.cpp` (49 lineas)

```cpp
bool g_alive = true;                    // Variable global -- controla el bucle principal
```

### handleSignal (linea 5-10)
```cpp
static void handleSignal(int sig)
{
    (void)sig;           // Ignoramos el numero de señal, solo nos interesa que llego
    g_alive = false;     // El bucle run() comprueba g_alive cada iteracion
    std::cout << std::endl;  // Salto de linea estetico tras el ^C
}
```

### main (linea 12-49)

**Paso 1: Validacion de argumentos**
- Comprueba que haya exactamente 2 argumentos (port + password).
- Convierte el puerto con `atoi()` y valida rango 1-65535.
- Verifica que la contraseña no este vacia.

**Paso 2: Señales**
```cpp
signal(SIGINT, handleSignal);   // Ctrl+C -> apagado limpio
signal(SIGPIPE, SIG_IGN);       // Ignorar SIGPIPE
```

**¿Por que se ignora SIGPIPE?**
Cuando haces `send()` a un socket cuyo otro extremo se ha desconectado, el kernel
envia SIGPIPE al proceso. Por defecto, SIGPIPE mata el proceso. Ignorandolo, el
`send()` simplemente devuelve -1 con `errno = EPIPE`, y el servidor puede manejar
la desconexion sin crashear. **Esto es CRITICO en servidores de red.**

**Paso 3: Crear y ejecutar el servidor**
```cpp
Server srv(port, pass);    // Construye el servidor (no abre socket aun)
srv.boot();                // Abre el socket y empieza a escuchar
srv.run();                 // Bucle principal con poll()
```
Todo dentro de un try/catch para capturar excepciones fatales (fallo en socket, bind, etc.).

---

## 3. El header maestro: Irc.hpp

**Archivo:** `include/Irc.hpp` (41 lineas)

Este header lo incluyen TODOS los archivos .cpp. Centraliza:

### Includes del sistema (lineas 4-25)

```
<iostream>, <string>, <sstream>   -- I/O y strings
<vector>, <map>, <set>            -- Contenedores STL
<algorithm>                       -- std::find, etc.
<cstdlib>, <cstring>, <cerrno>    -- atoi, memset, strerror, errno
<csignal>, <cctype>               -- signal, isalpha, toupper
<sys/socket.h>, <netinet/in.h>    -- socket(), bind(), listen(), accept()
<arpa/inet.h>, <netdb.h>          -- inet_ntoa(), etc.
<unistd.h>                        -- close(), read()
<fcntl.h>                         -- fcntl() para non-blocking
<poll.h>                          -- poll()
```

### Constantes (lineas 26-28)

```cpp
#define MAX_FDS    128    // Backlog de listen() -- cuantas conexiones pendientes acepta el kernel
#define READ_SIZE  512    // Bytes maximos por lectura con recv()
#define BUF_LIMIT  4096   // Limite del buffer de un cliente (proteccion anti-flood/DoS)
```

### Forward declarations (lineas 32-34)

```cpp
class Client;
class Channel;
class Server;
```

Esto resuelve las dependencias circulares: Server.hpp necesita conocer Client y Channel,
y viceversa. Con forward declarations, el compilador sabe que existen esas clases sin
necesitar su definicion completa todavia.

### Orden de includes (lineas 36-39)

```cpp
#include "Replies.hpp"    // Primero: no depende de nada
#include "Client.hpp"     // Segundo: Client es independiente
#include "Channel.hpp"    // Tercero: Channel usa Client*
#include "Server.hpp"     // Cuarto: Server usa Client* y Channel*
```

---

## 4. Clase Server: Server.hpp y Server.cpp

### Server.hpp (81 lineas)

**Atributos privados:**

| Atributo | Tipo | Descripcion |
|---|---|---|
| `_listenPort` | `int` | Puerto TCP donde escucha |
| `_connPass` | `std::string` | Contraseña de conexion |
| `_listenFd` | `int` | File descriptor del socket de escucha |
| `_pollSet` | `vector<pollfd>` | Conjunto de fds vigilados por poll() |
| `_sessions` | `map<int, Client*>` | Clientes conectados (fd -> Client*) |
| `_rooms` | `map<string, Channel*>` | Canales activos (nombre -> Channel*) |
| `_bot` | `Bot*` | Puntero al bot Marvin (pseudo-cliente interno, ver seccion 15) |

**Constructores privados (lineas 17-19):**
```cpp
Server();                           // Default: privado (no se puede usar)
Server(const Server&);              // Copia: privado (no se puede copiar)
Server& operator=(const Server&);   // Asignacion: privada (no se puede asignar)
```
Esto es el patron de C++98 para hacer una clase no-copiable. En C++11 se haria
con `= delete`.

**Metodos organizados por archivo fuente:**
- `Network.cpp`: openSocket, welcomeClient, receiveData, dropClient
- `Parse.cpp`: routeMessage, checkRegistration
- `CmdAuth.cpp`: execPass, execNick, execUser, execQuit, execPing
- `CmdChannel.cpp`: execJoin, execPart, execTopic, execMode, enterRoom, applyMode*
- `CmdMessage.cpp`: execPrivmsg, execNotice, execKick, execInvite
- `Server.cpp`: transmit, sendNumeric, locateUser, locateRoom, isLegalNick, etc.

### Server.cpp (185 lineas)

**Constructor (linea 4-8):**
```cpp
Server::Server(int port, const std::string& pass)
    : _listenPort(port), _connPass(pass), _listenFd(-1), _bot(new Bot())
{
    std::srand(std::time(NULL));    // Semilla para los juegos aleatorios del bot
}
```
Guarda parametros, crea el bot Marvin, e inicializa la semilla de numeros aleatorios.

**Destructor (linea 10-24):**
```cpp
~Server()
```
Limpieza completa:
1. Recorre `_sessions`: cierra cada fd con `close()` y hace `delete` del Client*.
2. Recorre `_rooms`: hace `delete` de cada Channel*.
3. Cierra `_listenFd` si estaba abierto.
4. `delete _bot`: libera el bot.

**¡Importante!** Toda la memoria dinamica (new Client, new Channel, new Bot) se libera aqui.
No hay memory leaks si el destructor se ejecuta correctamente.

**boot() (linea 21-25):**
```cpp
void Server::boot()
{
    openSocket();    // Crea socket, bind, listen (ver Network.cpp)
    std::cout << "Listening on port " << _listenPort << std::endl;
}
```

**run() (linea 27-56) -- EL CORAZON DEL SERVIDOR:**
```cpp
void Server::run()
{
    while (g_alive)    // Sale cuando SIGINT pone g_alive = false
    {
        // poll() bloquea hasta 1 segundo esperando actividad en cualquier fd
        if (poll(&_pollSet[0], _pollSet.size(), 1000) < 0)
        {
            if (!g_alive) break;    // Si fue interrumpido por señal, salir limpiamente
            throw std::runtime_error(...);
        }

        // *** SNAPSHOT CRITICO ***
        std::vector<struct pollfd> snap = _pollSet;

        for (size_t i = 0; i < snap.size(); i++)
        {
            // Cliente desconectado o error
            if (snap[i].revents & (POLLHUP | POLLERR))
            {
                if (snap[i].fd != _listenFd
                    && _sessions.find(snap[i].fd) != _sessions.end())
                    dropClient(snap[i].fd);
                continue;
            }
            // Hay datos para leer
            if (snap[i].revents & POLLIN)
            {
                if (snap[i].fd == _listenFd)
                    welcomeClient();        // Nueva conexion entrante
                else if (_sessions.find(snap[i].fd) != _sessions.end())
                    receiveData(snap[i].fd); // Datos de un cliente existente
            }
        }
    }
}
```

**¿Por que el snapshot `snap = _pollSet`?**

Dentro del bucle for, llamamos a `welcomeClient()` (que añade elementos a `_pollSet`)
o `dropClient()` (que elimina elementos de `_pollSet`). Si iteramos directamente sobre
`_pollSet`, modificarlo durante la iteracion invalida los iteradores/indices y causa
comportamiento indefinido. El snapshot es una copia independiente que no se modifica.

**poll() explicado:**
- `&_pollSet[0]`: puntero al primer elemento del vector (equivalente a un array C).
- `_pollSet.size()`: cuantos fds vigilar.
- `1000`: timeout en milisegundos. Si en 1 segundo no hay actividad, poll() devuelve 0
  y el bucle vuelve a comprobar `g_alive`.

**Helpers (lineas 60-159):**

```cpp
void Server::transmit(int fd, const std::string& msg)
```
Wrapper simple sobre `send()`. Envia `msg` al fd indicado.

```cpp
void Server::sendNumeric(Client* c, const std::string& code,
    const std::string& target, const std::string& body)
```
Envia un mensaje numerico IRC con formato: `:ircserv <code> <target> <body>\r\n`
Ejemplo: `:ircserv 001 alice :Welcome to the IRC Network alice!alice@127.0.0.1`

```cpp
Client* Server::locateUser(const std::string& nick)
```
Busca un cliente por nick recorriendo `_sessions`. Devuelve NULL si no existe.
Complejidad O(n) -- no es optimo pero para MAX_FDS=128 es mas que suficiente.

```cpp
Channel* Server::locateRoom(const std::string& name)
```
Busca un canal por nombre en `_rooms`. Usa `find()` del map = O(log n).

```cpp
bool Server::isLegalNick(const std::string& nick)
```
Valida un nickname segun las reglas IRC:
- No vacio, maximo 9 caracteres.
- Primer caracter: letra o `_`.
- Resto: alfanumerico o `_-[]\^{}`.

```cpp
void Server::listMembers(Client* c, Channel* ch)
```
Envia la lista de miembros de un canal (RPL_NAMREPLY + RPL_ENDOFNAMES).
Los operadores llevan prefijo `@` delante del nick. Al final de la lista siempre
se añade el nick del bot (Marvin), para que aparezca como miembro de todos los canales.

```cpp
void Server::notifyChannels(Client* c, const std::string& msg)
```
Envia un mensaje a TODOS los usuarios que comparten canal con `c`, sin duplicados.
Usa un `std::set<int> done` para evitar enviar dos veces al mismo usuario
(que podria estar en multiples canales con `c`).

```cpp
std::string Server::hostname() const { return "ircserv"; }
```
Devuelve el nombre del servidor. Usado como prefijo en mensajes numericos.

```cpp
void Server::ensureOp(Channel* ch)
```
**Auto-asignacion de operador.** Se llama cada vez que un usuario abandona un canal
(PART, KICK, o desconexion). Si el canal se queda sin operadores pero aun tiene
usuarios, promueve automaticamente al primer usuario de la lista. Envia un mensaje
MODE +o a todos los miembros para notificar el cambio:
```cpp
void Server::ensureOp(Channel* ch)
{
    if (ch->hasOps() || ch->vacant())
        return;                          // Ya hay ops o el canal esta vacio
    Client* next = ch->firstUser();      // Primer usuario en el set
    ch->promote(next);                   // Promover a operador
    ch->relay(":" + hostname() + " MODE " + ch->getLabel()
        + " +o " + next->getNick() + "\r\n");
}
```
Esto garantiza que un canal con usuarios nunca se quede sin operadores, evitando
situaciones donde nadie puede gestionar el canal.

```cpp
std::vector<std::string> Server::splitList(const std::string& s, char sep)
```
Divide un string por un separador. Se usa para parsear listas separadas por coma
en JOIN (#ch1,#ch2) y PART.

```cpp
void Server::botReply(Client* c, const std::string& dest, const std::string& text)
```
Pasa el texto del usuario al bot (via `_bot->handleCommand()`), y envia la respuesta
como un PRIVMSG del bot al destino indicado. Si el destino es un canal, usa
`ch->relay()` para que todos los miembros lo vean. Si es un nick, lo envia directamente
al usuario con `transmit()`. Soporta respuestas multilinea (las divide por `\n`).
Ver seccion 15 para detalles completos.

---

## 5. Capa de red: Network.cpp

**Archivo:** `src/Network.cpp` (135 lineas)

Este archivo contiene las 4 operaciones fundamentales de red.

### openSocket() (lineas 3-39)

Crea el socket de escucha TCP. Paso a paso:

```cpp
_listenFd = socket(AF_INET, SOCK_STREAM, 0);
```
- `AF_INET`: IPv4.
- `SOCK_STREAM`: TCP (flujo de bytes ordenado y fiable).
- Devuelve un file descriptor (numero entero).

```cpp
int on = 1;
setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
```
- `SO_REUSEADDR`: permite reutilizar el puerto inmediatamente despues de cerrar
  el servidor. Sin esto, tendrias que esperar ~60 segundos (TIME_WAIT) para
  volver a hacer bind al mismo puerto.

```cpp
fcntl(_listenFd, F_SETFL, O_NONBLOCK);
```
- Pone el socket en modo **no bloqueante**. Esto significa que `accept()` devuelve
  inmediatamente con -1 si no hay conexiones pendientes, en vez de bloquearse
  esperando.
- **Obligatorio segun el subject.**

```cpp
struct sockaddr_in addr;
std::memset(&addr, 0, sizeof(addr));
addr.sin_family      = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;    // Escucha en todas las interfaces de red
addr.sin_port        = htons(_listenPort);  // Convierte a network byte order
```

```cpp
bind(_listenFd, (struct sockaddr*)&addr, sizeof(addr));
```
- Asocia el socket a la direccion IP + puerto.

```cpp
listen(_listenFd, MAX_FDS);
```
- Pone el socket en modo de escucha. `MAX_FDS` es el backlog: cuantas conexiones
  puede tener en cola el kernel antes de rechazar nuevas.

```cpp
struct pollfd entry;
entry.fd      = _listenFd;
entry.events  = POLLIN;     // Nos interesa saber cuando hay conexiones entrantes
entry.revents = 0;
_pollSet.push_back(entry);
```
- Añade el socket de escucha como primer elemento del pollSet.

### welcomeClient() (lineas 41-60)

Se llama cuando `poll()` detecta actividad en `_listenFd` (= nueva conexion entrante).

```cpp
int fd = accept(_listenFd, (struct sockaddr*)&addr, &len);
```
- Acepta la conexion. Devuelve un **nuevo fd** para este cliente especifico.
- El `_listenFd` sigue escuchando para mas conexiones.

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);   // El nuevo fd tambien debe ser non-blocking
```

```cpp
Client* fresh = new Client(fd);
fresh->changeHost(inet_ntoa(addr.sin_addr));   // Guarda la IP del cliente
_sessions[fd] = fresh;
```
- Crea un objeto Client en el heap, guarda su IP, y lo registra en `_sessions`.
- Se añade tambien al `_pollSet` para que `poll()` vigile este fd.

**Nota:** En este punto el cliente NO esta autenticado ni registrado. Debe enviar
PASS, NICK, USER antes de poder hacer cualquier otra cosa.

### receiveData() (lineas 62-99)

Se llama cuando `poll()` detecta datos disponibles en un fd de cliente.

```cpp
char buf[READ_SIZE];              // Buffer local de 512 bytes
int n = recv(fd, buf, sizeof(buf) - 1, 0);
```
- `recv()` lee hasta 511 bytes del socket (reserva 1 byte para `\0`).
- Si `n <= 0`: el cliente se desconecto (0 = cierre limpio, -1 = error).

```cpp
buf[n] = '\0';
Client* c = _sessions[fd];
if (!c->bufferAppend(buf))     // Si el buffer supera BUF_LIMIT (4096)
{
    dropClient(fd);             // Desconecta al cliente (anti-flood)
    return;
}
```

**El bucle de extraccion de lineas:**
```cpp
std::string& raw = c->recvBuf();
size_t pos;
while ((pos = raw.find('\n')) != std::string::npos)
{
    std::string line = raw.substr(0, pos);     // Extrae hasta el \n
    raw.erase(0, pos + 1);                     // Elimina la linea del buffer

    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);           // Quita \r si es \r\n

    if (!line.empty())
        routeMessage(c, line);                 // Procesa el comando

    if (_sessions.find(fd) == _sessions.end())
        return;     // El cliente fue desconectado por routeMessage (ej: QUIT)
}
```

**¿Por que el buffer y no procesar directamente?**

TCP es un protocolo de **flujo de bytes**, no de mensajes. Esto significa que:
- Un `recv()` puede traer medio comando: `"PAS"` (el resto llega despues).
- Un `recv()` puede traer dos comandos juntos: `"NICK alice\r\nJOIN #test\r\n"`.
- Un `recv()` puede traer un comando y medio: `"NICK alice\r\nJOI"`.

El buffer acumula todo lo recibido y solo procesa cuando encuentra `\n`.
Lo que quede sin `\n` al final se mantiene en el buffer para la proxima lectura.

**Este es el test que pide el subject con `nc` y Ctrl+D.**

### dropClient() (lineas 101-135)

Desconecta un cliente completamente:

1. **Sacar de todos los canales:**
   ```cpp
   for (cada canal en _rooms)
   {
       canal->dismiss(c);        // Quita al cliente
       if (canal->vacant())      // Si el canal quedo vacio
       {
           delete canal;         // Libera memoria
           _rooms.erase(it++);   // Elimina del mapa (post-incremento para no invalidar)
       }
       else
       {
           ensureOp(it->second); // Auto-asignar operador si se quedo sin ops
           ++it;
       }
   }
   ```
   **¡Ojo con el patron `erase(it++)`!** En un `std::map`, borrar un elemento
   invalida solo el iterador borrado. Con `it++` primero avanzamos el iterador
   y luego borramos el anterior. Es un patron clasico de C++98.
   
   **Nota sobre `ensureOp()`:** Si el cliente que se desconecta era el unico
   operador del canal, `ensureOp()` promueve automaticamente al primer usuario
   restante y notifica a todos con un mensaje MODE +o.

2. **Sacar del pollSet:** busca el fd y lo elimina del vector.

3. **Cerrar y limpiar:**
   ```cpp
   close(fd);          // Cierra el socket
   delete c;           // Libera el objeto Client
   _sessions.erase(fd); // Elimina del mapa de sesiones
   ```

---

## 6. Clase Client: Client.hpp y Client.cpp

**Archivos:** `include/Client.hpp` (49 lineas) + `src/Client.cpp` (59 lineas)

### Atributos

| Atributo | Tipo | Descripcion |
|---|---|---|
| `_sockFd` | `int` | File descriptor del socket de este cliente |
| `_nick` | `string` | Nickname (max 9 chars). Vacio hasta que envie NICK |
| `_user` | `string` | Username. Vacio hasta que envie USER |
| `_fullname` | `string` | Nombre real. Se extrae del 4o parametro de USER |
| `_host` | `string` | IP del cliente (ej: "127.0.0.1"). Se asigna en welcomeClient() |
| `_recvBuf` | `string` | Buffer de datos recibidos pendientes de procesar |
| `_authorized` | `bool` | true si envio PASS con la contraseña correcta |
| `_welcomed` | `bool` | true si completo el registro (PASS+NICK+USER ok) |

### Flujo de estados del cliente

```
Conexion TCP       PASS correcto      NICK + USER validos
    |                   |                     |
    v                   v                     v
_authorized=false  _authorized=true    _welcomed=true
_welcomed=false    _welcomed=false     (recibe RPL 001-004)
                                       YA PUEDE USAR COMANDOS
```

### Metodos importantes

**OCF (Orthodox Canonical Form):**
Constructor default, constructor con fd, constructor de copia, operador de asignacion,
destructor. Requerido por 42 para todas las clases.

**fullId() (linea 54-58):**
```cpp
std::string Client::fullId() const
{
    std::string n = _nick.empty() ? "*" : _nick;
    std::string u = _user.empty() ? "*" : _user;
    return n + "!" + u + "@" + _host;
}
```
Genera el prefijo IRC estandar: `nick!user@host`.
Ejemplo: `alice!alice@127.0.0.1`.
Se usa como fuente en todos los mensajes que se envian en nombre de este cliente.

**bufferAppend() (linea 46-50):**
```cpp
bool Client::bufferAppend(const std::string& chunk)
{
    _recvBuf += chunk;
    return _recvBuf.size() <= BUF_LIMIT;   // false = buffer overflow -> desconectar
}
```
Devuelve `false` si el buffer supera 4096 bytes. Esto protege contra clientes
maliciosos que envien datos sin `\n` para agotar la memoria del servidor.

---

## 7. Clase Channel: Channel.hpp y Channel.cpp

**Archivos:** `include/Channel.hpp` (60 lineas) + `src/Channel.cpp` (86 lineas)

### Atributos

| Atributo | Tipo | Descripcion |
|---|---|---|
| `_label` | `string` | Nombre del canal (ej: "#general") |
| `_subject` | `string` | Topic del canal |
| `_passkey` | `string` | Contraseña del canal (modo +k). Vacio = sin contraseña |
| `_users` | `set<Client*>` | Conjunto de miembros |
| `_moderators` | `set<Client*>` | Subconjunto de `_users` que son operadores |
| `_whitelist` | `set<string>` | Nicks invitados (para modo +i) |
| `_modeI` | `bool` | Modo invite-only activo |
| `_modeT` | `bool` | Modo topic-restringido activo |
| `_cap` | `int` | Limite de usuarios (modo +l). 0 = sin limite |

### ¿Por que `set` y no `vector`?

- `std::set` garantiza unicidad (no puedes añadir el mismo Client* dos veces).
- Las operaciones `find()`, `insert()`, `erase()` son O(log n).
- No nos importa el orden de insercion.

### ¿Por que `_whitelist` guarda strings y no Client*?

Porque un usuario puede ser invitado a un canal **antes** de unirse. El nick es el
identificador persistente. Si guardaramos Client*, el puntero podria ser invalido
si el usuario se desconecta y reconecta.

### Metodos clave

**enroll() / dismiss():**
```cpp
void Channel::enroll(Client* c)  { _users.insert(c); }
void Channel::dismiss(Client* c) { _users.erase(c); _moderators.erase(c); }
```
`dismiss()` tambien quita de moderadores (no puedes ser op de un canal si no estas en el).

**promote() / demote():**
```cpp
void Channel::promote(Client* c) { _moderators.insert(c); }
void Channel::demote(Client* c)  { _moderators.erase(c); }
```

**hasOps() / firstUser():**
```cpp
bool Channel::hasOps() const { return !_moderators.empty(); }

Client* Channel::firstUser() const
{
    return _users.empty() ? NULL : *_users.begin();
}
```
`hasOps()` verifica si hay al menos un operador en el canal.
`firstUser()` devuelve el primer usuario del set (orden determinado por el comparador
de punteros en `std::set`). Se usa en `Server::ensureOp()` para elegir al nuevo
operador cuando el canal se queda sin ninguno.

**allow() / revoke() / isAllowed():**
Gestion de la whitelist de invitados para modo +i.

**relay() (lineas 78-86):**
```cpp
void Channel::relay(const std::string& msg, Client* skip)
{
    for (std::set<Client*>::iterator it = _users.begin(); it != _users.end(); ++it)
    {
        if (*it != skip)
            send((*it)->socketFd(), msg.c_str(), msg.size(), 0);
    }
}
```
Envia `msg` a TODOS los miembros del canal excepto `skip`.
- Cuando alguien envia un PRIVMSG al canal, `skip = sender` (no te reenvias tu propio mensaje).
- Cuando alguien hace JOIN, `skip = NULL` (todos ven el JOIN, incluyendo el que se une).

**Nota:** `relay()` usa `send()` directamente, no pasa por `Server::transmit()`.
Esto es un detalle de implementacion -- funciona igual, pero acopla Channel con
la API de sockets.

---

## 8. Parsing y registro: Parse.cpp

**Archivo:** `src/Parse.cpp` (101 lineas)

### routeMessage() (lineas 3-83)

El parser IRC. Recibe una linea cruda (sin `\r\n`) y la convierte en un comando
con argumentos.

**Formato de un mensaje IRC:**
```
[:prefix] VERBO arg1 arg2 ... :trailing message
```

**Paso 1: Quitar prefijo (lineas 7-17)**
```cpp
if (line[0] == ':')        // Si empieza con ':', es un prefijo
{
    size_t sp = line.find(' ');
    line = line.substr(sp + 1);   // Lo descartamos
}
```
El prefijo indica el origen del mensaje. En un servidor, lo ignoramos porque ya
sabemos quien es el cliente por su fd.

**Paso 2: Extraer verbo y argumentos (lineas 24-49)**
```cpp
while (!line.empty())
{
    if (line[0] == ':')                   // ':' marca el inicio del trailing
    {
        args.push_back(line.substr(1));   // Todo lo que sigue es UN solo argumento
        break;
    }
    // Si no, extraer el siguiente token separado por espacios
    size_t sp = line.find(' ');
    ...
    if (verb.empty())
        verb = tok;       // El primer token es el verbo
    else
        args.push_back(tok);  // Los demas son argumentos
}
```

**Ejemplo de parsing:**
```
Entrada:  "PRIVMSG #test :Hola a todos"
verb:     "PRIVMSG"
args[0]:  "#test"
args[1]:  "Hola a todos"    (el trailing, sin los ':')
```

```
Entrada:  "MODE #test +itk-l secreto"
verb:     "MODE"
args[0]:  "#test"
args[1]:  "+itk-l"
args[2]:  "secreto"
```

**Paso 3: Verbo a mayusculas (linea 51-52)**
```cpp
for (size_t i = 0; i < verb.size(); i++)
    verb[i] = std::toupper(verb[i]);
```
IRC es case-insensitive para los comandos: `privmsg`, `PRIVMSG`, `Privmsg` son lo mismo.

**Paso 4: Filtrar comandos especiales (linea 54-55)**
```cpp
if (verb == "CAP" || verb == "PONG")
    return;
```
- `CAP`: negociacion de capacidades IRC moderno. No lo implementamos, lo ignoramos.
- `PONG`: respuesta del cliente a nuestro PING. No necesitamos hacer nada.

**Paso 5: Verificar registro (lineas 59-65)**
```cpp
if (!c->isWelcomed()
    && verb != "PASS" && verb != "NICK"
    && verb != "USER" && verb != "QUIT")
{
    sendNumeric(c, ERR::NOTREGISTERED, who, ":You have not registered");
    return;
}
```
Si el cliente no ha completado el registro, solo puede enviar PASS, NICK, USER o QUIT.
Cualquier otro comando recibe error 451.

**Paso 6: Despachar al handler correcto (lineas 67-82)**
Un if-else chain que mapea cada verbo a su funcion `exec*()`.

### checkRegistration() (lineas 85-101)

Se llama despues de cada PASS, NICK o USER para ver si el registro esta completo.

```cpp
void Server::checkRegistration(Client* c)
{
    if (c->isWelcomed()) return;           // Ya registrado, no hacer nada
    if (!c->hasAuth() || c->getNick().empty() || c->getUser().empty())
        return;                            // Aun falta algo

    c->markWelcomed(true);                 // ¡Registro completo!
    // Enviar mensajes de bienvenida (RPL 001-004)
    sendNumeric(c, RPL::WELCOME, c->getNick(),
        ":Welcome to the IRC Network " + c->fullId());
    sendNumeric(c, RPL::YOURHOST, c->getNick(),
        ":Your host is ircserv, running version 1.0");
    sendNumeric(c, RPL::CREATED, c->getNick(),
        ":This server was created today");
    sendNumeric(c, RPL::MYINFO, c->getNick(),
        "ircserv 1.0 o itkol");
}
```

El RPL 004 (MYINFO) informa al cliente de los modos soportados:
- `o`: modo de usuario (solo hay uno)
- `itkol`: modos de canal soportados

---

## 9. Respuestas numericas: Replies.hpp

**Archivo:** `include/Replies.hpp` (46 lineas)

El protocolo IRC usa codigos numericos de 3 digitos para respuestas. Organizados en
dos namespaces:

### RPL (replies positivos)

| Constante | Codigo | Cuando se usa |
|---|---|---|
| `RPL::WELCOME` | 001 | Al completar el registro |
| `RPL::YOURHOST` | 002 | Info del servidor |
| `RPL::CREATED` | 003 | Fecha de creacion |
| `RPL::MYINFO` | 004 | Modos soportados |
| `RPL::CHANNELMODEIS` | 324 | Respuesta a MODE sin argumentos |
| `RPL::NOTOPIC` | 331 | El canal no tiene topic |
| `RPL::TOPIC` | 332 | El topic del canal |
| `RPL::INVITING` | 341 | Confirmacion de INVITE |
| `RPL::NAMREPLY` | 353 | Lista de miembros del canal |
| `RPL::ENDOFNAMES` | 366 | Fin de la lista de miembros |

### ERR (errores)

| Constante | Codigo | Significado |
|---|---|---|
| `ERR::NOSUCHNICK` | 401 | Nick o canal no existe |
| `ERR::NOSUCHCHANNEL` | 403 | Canal no existe |
| `ERR::CANNOTSENDTOCHAN` | 404 | No puedes enviar a ese canal |
| `ERR::NORECIPIENT` | 411 | PRIVMSG sin destinatario |
| `ERR::NOTEXTTOSEND` | 412 | PRIVMSG sin texto |
| `ERR::UNKNOWNCOMMAND` | 421 | Comando desconocido |
| `ERR::NONICKNAMEGIVEN` | 431 | NICK sin parametro |
| `ERR::ERRONEUSNICKNAME` | 432 | Nick con caracteres invalidos |
| `ERR::NICKNAMEINUSE` | 433 | Nick ya esta en uso |
| `ERR::USERNOTINCHANNEL` | 441 | El usuario no esta en ese canal |
| `ERR::NOTONCHANNEL` | 442 | TU no estas en ese canal |
| `ERR::USERONCHANNEL` | 443 | El usuario YA esta en ese canal |
| `ERR::NOTREGISTERED` | 451 | No has completado el registro |
| `ERR::NEEDMOREPARAMS` | 461 | Faltan parametros |
| `ERR::ALREADYREGISTERED` | 462 | Ya estas registrado |
| `ERR::PASSWDMISMATCH` | 464 | Contraseña incorrecta |
| `ERR::CHANNELISFULL` | 471 | Canal lleno (+l) |
| `ERR::INVITEONLYCHAN` | 473 | Canal invite-only (+i) |
| `ERR::BADCHANNELKEY` | 475 | Contraseña de canal incorrecta (+k) |
| `ERR::BADCHANMASK` | 476 | Nombre de canal invalido |
| `ERR::CHANOPRIVSNEEDED` | 482 | Necesitas ser operador |
| `ERR::UNKNOWNMODE` | 472 | Caracter de modo desconocido |

---

## 10. Comandos de autenticacion: CmdAuth.cpp

**Archivo:** `src/CmdAuth.cpp` (103 lineas)

### execPass (lineas 3-21)

```
Formato: PASS <password>
```

- Si ya esta registrado (`isWelcomed`): error 462 ALREADYREGISTERED.
- Si no hay argumentos: error 461 NEEDMOREPARAMS.
- Si la contraseña coincide con `_connPass`: marca `_authorized = true`.
- Si no coincide: error 464 PASSWDMISMATCH.

**Nota:** PASS no intenta completar el registro (no llama a `checkRegistration`).
Esto es intencionado: PASS solo abre la puerta. NICK y USER completan el registro.

### execNick (lineas 23-60)

```
Formato: NICK <nickname>
```

**Flujo:**
1. Sin argumentos -> error 431.
2. Nick invalido (segun `isLegalNick`) -> error 432.
3. Nick en uso por otro cliente **o es el nick del bot** ("Marvin") -> error 433.
4. Si ya esta registrado (`isWelcomed`):
   - Guarda el fullId antiguo
   - Cambia el nick
   - Notifica al cliente: `:oldnick!user@host NICK :newnick`
   - Notifica a todos los canales compartidos
5. Si NO esta registrado:
   - Solo cambia el nick
   - Llama a `checkRegistration()` (por si ya tenia PASS y USER)

### execUser (lineas 62-81)

```
Formato: USER <username> <mode> <unused> :<realname>
```

- Si ya registrado: error 462.
- Si menos de 4 args: error 461.
- Guarda `args[0]` como username, `args[3]` como fullname.
- Llama a `checkRegistration()`.

**Nota:** `args[1]` (mode) y `args[2]` (unused) se ignoran. Son parte del protocolo
pero no tienen uso practico en un servidor basico.

### execQuit (lineas 83-91)

```
Formato: QUIT [:message]
```

1. Construye el mensaje de despedida: `:nick!user@host QUIT :Quit: reason`
2. Envia ERROR al cliente: `ERROR :Closing link (fullId) [Quit: reason]`
3. Notifica a todos los canales
4. Llama a `dropClient()`

### execPing (lineas 93-103)

```
Formato: PING <token>
```

Responde con: `:ircserv PONG ircserv :<token>`

Los clientes IRC envian PING periodicamente para verificar que la conexion sigue
viva. Si el servidor no responde PONG, el cliente asume desconexion.

---

## 11. Comandos de canal: CmdChannel.cpp

**Archivo:** `src/CmdChannel.cpp` (340 lineas) - El archivo mas largo.

### execJoin (lineas 5-19)

```
Formato: JOIN #canal1,#canal2 key1,key2
```

Parsea las listas separadas por coma y llama a `enterRoom()` para cada canal.

### enterRoom (lineas 21-72)

El metodo central de JOIN. Pasos:

1. **Validar nombre:** debe empezar con `#` o `&`. Si no -> error 476 BADCHANMASK.
2. **Buscar o crear canal:**
   ```cpp
   Channel* ch = locateRoom(name);
   bool fresh = false;
   if (!ch) {
       ch = new Channel(name);
       _rooms[name] = ch;
       fresh = true;
   }
   ```
3. **Si ya esta en el canal:** return (silencioso, segun RFC).
4. **Verificar restricciones:**
   - `+i` (invite-only) y no esta en whitelist -> error 473.
   - `+k` (password) y la key no coincide -> error 475.
   - `+l` (user limit) y el canal esta lleno -> error 471.
5. **Unirse:**
   ```cpp
   ch->enroll(c);
   if (fresh) ch->promote(c);    // El creador es automaticamente operador
   ch->revoke(c->getNick());     // Quitar de whitelist (ya entro)
   ch->relay(":" + c->fullId() + " JOIN " + name + "\r\n");  // Notificar a todos
   ```
6. **Enviar info del canal:**
   - Topic (RPL 332) o "No topic is set" (RPL 331)
   - Lista de miembros (RPL 353 + 366)

### execPart (lineas 76-114)

```
Formato: PART #canal1,#canal2 [:reason]
```

Para cada canal:
1. Verificar que existe (error 403 si no).
2. Verificar que el cliente esta en el (error 442 si no).
3. Enviar mensaje PART a todos los miembros.
4. `ch->dismiss(c)`.
5. Si el canal queda vacio, `delete ch` y quitar de `_rooms`.
6. **Si el canal NO queda vacio, llamar a `ensureOp(ch)`** para auto-asignar
   operador si el usuario que salio era el unico operador.

### execTopic (lineas 118-158)

```
Formato: TOPIC #canal           (consultar topic)
         TOPIC #canal :nuevo    (cambiar topic)
```

- Sin canal: error 461.
- Canal no existe: error 403.
- No estas en el canal: error 442.
- Solo consultar (1 arg): envia RPL 332 o 331.
- Cambiar topic (2 args):
  - Si `+t` activo y no eres operador: error 482.
  - Si no, cambia el topic y notifica a todos.

### MODE (lineas 160-340)

Es el comando mas complejo del servidor. Tiene varias subfunciones.

#### execMode (lineas 263-340)

```
Formato: MODE #canal                    (consultar modos)
         MODE #canal +flags [params]    (cambiar modos)
```

**Consultar modos (1 arg):**
```cpp
if (args.size() == 1)
{
    std::string flags = "+";
    if (ch->flagInvite()) flags += "i";
    if (ch->flagTopic())  flags += "t";
    if (!ch->getPasskey().empty()) { flags += "k"; extra += " " + passkey; }
    if (ch->getCap() > 0) { flags += "l"; extra += " " + cap_str; }
    sendNumeric(c, RPL::CHANNELMODEIS, ...);
}
```

**Cambiar modos (2+ args):**

Solo operadores pueden cambiar modos (error 482 si no lo eres).

Recorre el string de flags caracter a caracter:
```cpp
std::string flagStr = args[1];    // Ej: "+itk-ol"
bool plus = true;                 // Empieza en modo "activar"

for (size_t i = 0; i < flagStr.size(); i++)
{
    char f = flagStr[i];
    if (f == '+') { plus = true;  continue; }  // Cambiar a modo "activar"
    if (f == '-') { plus = false; continue; }  // Cambiar a modo "desactivar"

    if      (f == 'i') applyModeI(ch, plus, applied);
    else if (f == 't') applyModeT(ch, plus, applied);
    else if (f == 'k') applyModeK(c, ch, plus, args, pi, applied, appliedArgs);
    else if (f == 'o') applyModeO(c, ch, plus, args, pi, applied, appliedArgs);
    else if (f == 'l') applyModeL(c, ch, plus, args, pi, applied, appliedArgs);
    else               error 472 UNKNOWNMODE
}
```

La variable `pi` (parameter index) empieza en 2 y avanza cada vez que un modo
consume un parametro (k, o, l con +).

Al final, se construye un mensaje con los modos que realmente se aplicaron:
```cpp
if (!applied.empty())
    ch->relay(":" + c->fullId() + " MODE " + rn + " " + applied + appliedArgs + "\r\n");
```

#### applyModeI / applyModeT (lineas 162-172)

Simples: activan/desactivan el flag y añaden al string de cambios.

#### applyModeK (lineas 174-196)

```
+k password   -> ch->changePasskey("password"), consume 1 argumento
-k            -> ch->changePasskey(""), no consume argumento
```

#### applyModeO (lineas 198-231)

```
+o nick   -> ch->promote(user), consume 1 argumento
-o nick   -> ch->demote(user), consume 1 argumento
```

Verifica que el nick existe (error 401) y que esta en el canal (error 441).

#### applyModeL (lineas 233-261)

```
+l 10     -> ch->changeCap(10), consume 1 argumento. Valor debe ser > 0.
-l        -> ch->changeCap(0), no consume argumento
```

---

## 12. Comandos de mensajeria: CmdMessage.cpp

**Archivo:** `src/CmdMessage.cpp` (192 lineas)

### execPrivmsg (lineas 5-58)

```
Formato: PRIVMSG <destino> :<texto>
```

- Sin destino: error 411 NORECIPIENT.
- Sin texto: error 412 NOTEXTTOSEND.
- **Destino es canal** (empieza con # o &):
  - Canal no existe: error 403.
  - No estas en el canal: error 404 CANNOTSENDTOCHAN.
  - Envia a todos menos al emisor: `ch->relay(msg, c)`.
  - **Si el texto empieza con `!`:** llama a `botReply(c, dest, text)` para que
    Marvin procese el comando y responda al canal (ver seccion 15).
- **Destino es el bot** (`dest == _bot->getNick()`, es decir "Marvin"):
  - Llama a `botReply(c, c->getNick(), text)` directamente. El bot responde
    en privado al usuario que le escribio. No hace falta que el texto empiece con `!`.
- **Destino es otro usuario:**
  - Usuario no existe: error 401.
  - Envia directamente con `transmit()`.

### execNotice (lineas 56-80)

Identico a PRIVMSG pero **silencioso**: si hay error (destino no existe, etc.),
no envia ningun error al emisor. Simplemente ignora. Esto es parte del protocolo
IRC: NOTICE nunca genera respuestas automaticas para evitar bucles entre bots.

### execKick (lineas 84-133)

```
Formato: KICK #canal usuario [:razon]
```

1. Menos de 2 args: error 461.
2. Canal no existe: error 403.
3. No estas en el canal: error 442.
4. No eres operador: error 482.
5. Victima no existe o no esta en el canal: error 441.
6. Notifica a todos: `:kicker KICK #canal victima :razon`
7. `ch->dismiss(victim)`.
8. Si el canal queda vacio, borrarlo.
9. **Si el canal NO queda vacio, llamar a `ensureOp(ch)`** para auto-asignar
   operador si la victima kickeada era el unico operador del canal.

**Nota:** La razon por defecto (si no se da) es el nick del que kickea.

### execInvite (lineas 137-186)

```
Formato: INVITE <nick> <canal>
```

1. Menos de 2 args: error 461.
2. Nick no existe: error 401.
3. Canal no existe: error 403.
4. No estas en el canal: error 442.
5. El invitado ya esta en el canal: error 443 USERONCHANNEL.
6. Si el canal es +i y no eres operador: error 482.
7. Añade a whitelist: `ch->allow(who->getNick())`.
8. Confirma al invitador: RPL 341 INVITING.
9. Notifica al invitado: `:invitador INVITE nick #canal`.

---

## 13. Flujos completos (de principio a fin)

### Flujo 1: Conexion y registro de un cliente

```
Cliente                          Servidor
  |                                |
  |--- (conexion TCP) ----------->|  welcomeClient(): accept(), crear Client, añadir a pollSet
  |                                |
  |--- PASS mypassword ---------->|  execPass(): verifica contraseña, marca _authorized=true
  |                                |
  |--- NICK alice --------------->|  execNick(): valida nick, guarda. checkRegistration(): falta USER
  |                                |
  |--- USER alice 0 * :Alice ---->|  execUser(): guarda user/fullname. checkRegistration():
  |                                |    _authorized + nick + user = TODO OK -> markWelcomed(true)
  |<-- :ircserv 001 alice :Welcome|
  |<-- :ircserv 002 alice :Your...|
  |<-- :ircserv 003 alice :This...|
  |<-- :ircserv 004 alice ircserv..|
  |                                |
  | (Ahora puede usar comandos)    |
```

### Flujo 2: Unirse a un canal y chatear

```
Alice                            Servidor                          Bob
  |                                |                                |
  |--- JOIN #test --------------->|  enterRoom(): canal no existe   |
  |                                |    -> new Channel("#test")     |
  |                                |    -> enroll(alice)            |
  |                                |    -> promote(alice) (es op)   |
  |<-- :alice!...@... JOIN #test --|                                |
  |<-- :ircserv 331 alice #test..--|  (no topic)                    |
  |<-- :ircserv 353 alice = #test..|  (lista: @alice)               |
  |<-- :ircserv 366 alice #test..--|  (fin de lista)                |
  |                                |                                |
  |                                |<-- JOIN #test -------- --------|  enterRoom(): canal existe
  |                                |    -> enroll(bob)              |
  |<-- :bob!...@... JOIN #test ----|-----> :bob!...@... JOIN #test ->|
  |                                |<--   (topic + names a bob)     |
  |                                |                                |
  |--- PRIVMSG #test :Hola! ----->|  execPrivmsg():                |
  |                                |    ch->relay(msg, alice)       |
  |                                |------:alice PRIVMSG #test...-->|  (bob lo recibe)
  |   (alice NO recibe su propio   |                                |
  |    mensaje porque skip=alice)  |                                |
```

### Flujo 3: Canal invite-only con password

```
Alice (operadora de #secret)     Servidor                          Bob
  |                                |                                |
  |--- MODE #secret +ik pass123 ->|  applyModeI: _modeI=true       |
  |                                |  applyModeK: _passkey="pass123"|
  |<-- :alice MODE #secret +ik ..--|                                |
  |                                |                                |
  |                                |<-- JOIN #secret pass123 -------|  enterRoom():
  |                                |    flagInvite() && !isAllowed  |
  |                                |----:ircserv 473 bob #secret -->|  INVITEONLYCHAN
  |                                |                                |
  |--- INVITE bob #secret ------->|  execInvite():                 |
  |                                |    ch->allow("bob")            |
  |<-- :ircserv 341 alice bob.. --|                                |
  |                                |----:alice INVITE bob #secret ->|
  |                                |                                |
  |                                |<-- JOIN #secret pass123 -------|  enterRoom():
  |                                |    isAllowed("bob") = true     |
  |                                |    passkey=="pass123" = true   |
  |                                |    -> enroll(bob), revoke("bob")|
  |<-- :bob JOIN #secret ----------|-----> :bob JOIN #secret ------>|
```

### Flujo 4: Datos parciales (test del subject)

```
Cliente (nc)                     Servidor
  |                                |
  |--- "PRI" --(Ctrl+D)--------->|  recv(): buf="PRI", bufferAppend("PRI")
  |                                |  recvBuf = "PRI" -- no hay \n, no procesa nada
  |                                |
  |--- "VMS" --(Ctrl+D)--------->|  recv(): buf="VMS", bufferAppend("VMS")
  |                                |  recvBuf = "PRIVMS" -- no hay \n, espera
  |                                |
  |--- "G #t :hi\n" ------------>|  recv(): buf="G #t :hi\n", bufferAppend(...)
  |                                |  recvBuf = "PRIVMSG #t :hi\n"
  |                                |  find('\n') = encontrado!
  |                                |  line = "PRIVMSG #t :hi"
  |                                |  routeMessage(c, "PRIVMSG #t :hi")
  |                                |  recvBuf = "" (limpio)
```

---

## 14. Preguntas de defensa y respuestas

### Preguntas sobre la arquitectura

**P: ¿Por que usais `poll()` y no threads?**
R: El subject lo prohibe explicitamente: "Forking is prohibited. All I/O operations
must be non-blocking." `poll()` permite un unico hilo que multiplexe todas las
conexiones. Es el modelo clasico de servidores de red de alto rendimiento
(event-driven / reactor pattern).

**P: ¿Que pasaria si no pusierais los sockets en modo non-blocking?**
R: Si un socket esta en modo blocking, `recv()` se bloquea hasta que haya datos.
Esto paralizaria el servidor entero (un solo hilo) esperando a un cliente
que no envia nada, mientras otros clientes esperan. Con non-blocking,
`recv()` devuelve -1 inmediatamente si no hay datos, y `poll()` es quien
espera eficientemente.

**P: ¿Para que sirve el snapshot de `_pollSet` en `run()`?**
R: Dentro del bucle for iteramos sobre los fds. Pero `welcomeClient()` añade
elementos y `dropClient()` los elimina de `_pollSet`. Modificar un contenedor
mientras lo iteras causa comportamiento indefinido. El snapshot (`snap`) es una
copia que no se modifica.

**P: ¿Que pasa si un cliente envia 10MB de datos sin ningun `\n`?**
R: El `bufferAppend()` devuelve false cuando `_recvBuf` supera `BUF_LIMIT` (4096
bytes). En `receiveData()`, si devuelve false, se llama a `dropClient()`.
Esto protege contra ataques de denegacion de servicio (DoS).

**P: ¿Hay memory leaks?**
R: No. Toda la memoria dinamica (new Client, new Channel) se libera en:
- `dropClient()`: delete Client, y delete Channel si queda vacio.
- `~Server()`: recorre `_sessions` y `_rooms` y hace delete de todo.
Esto cubre tanto el shutdown limpio como las desconexiones individuales.

### Preguntas sobre el protocolo IRC

**P: ¿Cual es la diferencia entre PRIVMSG y NOTICE?**
R: PRIVMSG genera errores si el destino no existe. NOTICE no genera ninguna
respuesta de error. Segun RFC 1459, NOTICE esta diseñado para evitar bucles
infinitos entre bots que se responden mutuamente.

**P: ¿Que hace el primer usuario que entra a un canal?**
R: Se convierte automaticamente en operador del canal (`ch->promote(c)` en
`enterRoom()`, linea 61). Solo pasa si el canal es nuevo (`fresh == true`).

**P: ¿Puede un operador kickearse a si mismo?**
R: Si. El codigo no lo impide. Verifica que seas op y que la victima este
en el canal, pero no comprueba que no te estes kickeando a ti mismo.

**P: ¿Que pasa si cambias de nick mientras estas en canales?**
R: `execNick()` (linea 47-54) notifica el cambio a todos los canales compartidos
via `notifyChannels()`. Pero la whitelist de invitacion guarda nicks como strings,
asi que si estabas invitado como "alice" y cambias a "bob", la invitacion se
pierde (el nick "alice" sigue en la whitelist pero ya no es tu nick).

**P: ¿Que modos de canal soportais?**
R: Cinco modos:
- `i`: invite-only (solo invitados pueden entrar)
- `t`: topic restringido (solo operadores pueden cambiar el topic)
- `k`: password del canal
- `o`: dar/quitar status de operador a un usuario
- `l`: limite de usuarios

**P: ¿Que pasa si el unico operador de un canal sale o es kickeado?**
R: El servidor auto-asigna un nuevo operador automaticamente. La funcion
`ensureOp()` se llama despues de cada PART, KICK y desconexion. Si el canal
se queda sin operadores pero aun tiene usuarios, el primer usuario del set
`_users` es promovido a operador y se envia un mensaje MODE +o a todos los
miembros del canal. Esto evita canales "huerfanos" que nadie puede gestionar.

**P: ¿Que pasa si haces `MODE #ch +k` sin dar la contraseña?**
R: Error 461 NEEDMOREPARAMS (applyModeK, linea 180-185).

### Preguntas tecnicas sobre C++

**P: ¿Por que usais `std::set` en vez de `std::vector` para los miembros del canal?**
R: `set` garantiza unicidad (no puedes añadir el mismo Client* dos veces) y
tiene busqueda O(log n) vs O(n) de vector. Como hacemos `find()` frecuentemente
(ej: verificar si un usuario esta en el canal), set es mas apropiado.

**P: ¿Donde esta la forma canonica ortodoxa (OCF)?**
R: Todas las clases (Client, Channel, Server) tienen:
- Constructor por defecto
- Constructor de copia
- Operador de asignacion
- Destructor
Server ademas tiene sus constructores de copia/asignacion en `private` para
impedir copias accidentales.

**P: ¿Por que `static` en `splitList()`?**
R: Porque no usa ningun atributo de la instancia. Es una funcion utilitaria
pura. Declararla `static` documenta esta intencion y permite llamarla sin
una instancia (aunque en la practica siempre se llama desde un metodo de Server).

---

## 15. Bonus: File Transfer (DCC) y Bot (Marvin)

El subject define dos bonus, ambos implementados en nuestro servidor.

---

### A) File Transfer (DCC) -- Funciona sin cambios

**¿Que es DCC?**

DCC (Direct Client-to-Client) es un mecanismo del protocolo IRC para transferir
archivos **directamente** entre dos clientes. El servidor solo actua como
intermediario del mensaje inicial -- los archivos NUNCA pasan por el servidor.

**Protocolo paso a paso:**

```
Alice (emisora)                  Servidor                          Bob (receptor)
  |                                |                                |
  | 1. PRIVMSG bob :              |                                |
  |    \x01DCC SEND foto.jpg      |                                |
  |    3232235777 5000 1024\x01   |                                |
  |------------------------------->|                                |
  |                                | 2. Reenvia el PRIVMSG tal cual |
  |                                |------------------------------->|
  |                                |                                |
  |                                |  (servidor ya no interviene)   |
  |                                |                                |
  |<============ 3. Bob abre conexion TCP directa a Alice =========|
  |                   IP: 3232235777 (192.168.1.1)                  |
  |                   Puerto: 5000                                  |
  |                                                                 |
  |============= 4. Alice envia foto.jpg (1024 bytes) ============>|
  |                   por conexion directa                          |
  |                                                                 |
  |<==== 5. Conexion directa se cierra al completar ===============|
```

**Formato del mensaje DCC SEND:**
```
\x01DCC SEND <filename> <ip_decimal> <port> <filesize>\x01
```

- `\x01` (0x01): delimitador CTCP (Client-To-Client Protocol). Son bytes literales
  que envuelven el mensaje DCC.
- `<filename>`: nombre del archivo.
- `<ip_decimal>`: IP del emisor en formato entero decimal (ej: 192.168.1.1 = 3232235777).
  Se calcula: `(192*256^3) + (168*256^2) + (1*256) + 1`.
- `<port>`: puerto TCP donde el emisor esta escuchando, esperando la conexion del receptor.
- `<filesize>`: tamaño del archivo en bytes.

**¿Por que funciona sin tocar nuestro codigo?**

El secreto esta en nuestro parser (`Parse.cpp`, lineas 27-30). Cuando el parser
encuentra `:` al inicio de un token, toma **todo lo que sigue como un unico argumento**,
sin modificarlo:

```cpp
if (line[0] == ':')
{
    args.push_back(line.substr(1));   // Todo lo que sigue, intacto
    break;
}
```

Esto significa que el contenido CTCP con los bytes `\x01` se preserva intacto
dentro de `args[1]`. Luego `execPrivmsg()` lo reenvia al destinatario tal cual:

```cpp
transmit(target->socketFd(), ":" + c->fullId() + " PRIVMSG "
    + dest + " :" + text + "\r\n");
```

El `text` contiene `\x01DCC SEND foto.jpg 3232235777 5000 1024\x01` sin ninguna
modificacion. El cliente receptor (irssi, WeeChat, HexChat) lo interpreta y ofrece
al usuario aceptar la transferencia.

**¿Por que el servidor NO necesita manejar la transferencia?**

Porque DCC es **peer-to-peer por diseño**. El servidor es solo un "cartero" del
mensaje inicial. La transferencia real ocurre por una conexion TCP separada que
los clientes negocian entre ellos. Esto es exactamente lo que dice el subject:
> "You should be able to transfer files."

Los clientes IRC (irssi, WeeChat, HexChat) ya implementan la logica de DCC
SEND/RECEIVE. Nuestro servidor solo necesita reenviar el PRIVMSG correctamente,
que es exactamente lo que hace.

**Preguntas de defensa sobre DCC:**

**P: ¿Donde esta el codigo de DCC?**
R: No hay codigo especifico de DCC. El servidor simplemente reenvia PRIVMSG
sin modificar el contenido. DCC es un subprotocolo que vive DENTRO del payload
de PRIVMSG. El parser ya preserva los bytes CTCP (`\x01`) intactos.

**P: ¿Que pasa si los clientes estan detras de NAT?**
R: DCC requiere que el emisor tenga un puerto accesible desde el exterior. Detras
de NAT, necesitarias port forwarding o DCC REVERSE (que no es parte del subject).

**P: ¿El servidor valida el contenido CTCP?**
R: No. Y no debe hacerlo. El servidor es agnostico al contenido -- solo transporta
mensajes. Esta es la filosofia correcta de IRC: el servidor enruta, los clientes
interpretan.

**Test de DCC con irssi:**

```bash
# Terminal 1: Servidor
./ircserv 6667 pass123

# Terminal 2: Alice (irssi)
irssi -c 127.0.0.1 -p 6667 -w pass123 -n alice
/join #test

# Terminal 3: Bob (irssi)
irssi -c 127.0.0.1 -p 6667 -w pass123 -n bob
/join #test

# En Alice:
/dcc send bob /ruta/al/archivo.txt

# En Bob (aparece solicitud de DCC):
/dcc get alice
```

---

### B) Bot IRC: Marvin -- El robot depresivo

Implementamos un bot integrado como pseudo-cliente interno del servidor. El bot se
llama **Marvin**, inspirado en el Androide Paranoide de *The Hitchhiker's Guide to
the Galaxy* de Douglas Adams. Todas sus respuestas tienen la personalidad melancolica
y sarcastica del personaje.

#### Archivos del bot

| Archivo | Lineas | Descripcion |
|---|---|---|
| `include/Bot.hpp` | 51 | Definicion de la clase Bot |
| `src/Bot.cpp` | 245 | Implementacion de los 11 comandos |

#### Archivos modificados para integrar el bot

| Archivo | Que se cambio |
|---|---|
| `include/Irc.hpp` | Forward declaration `class Bot;` + `#include "Bot.hpp"` |
| `include/Server.hpp` | Atributo `Bot* _bot;` + declaracion de `botReply()` |
| `src/Server.cpp` | Constructor: `_bot(new Bot())`, `std::srand()`. Destructor: `delete _bot`. Nuevo metodo `botReply()` (con logica de `!info` y `!who`). `listMembers()` añade Marvin al NAMES |
| `src/CmdMessage.cpp` | `execPrivmsg()`: intercepta `!comandos` en canales + mensajes directos a Marvin |
| `src/CmdAuth.cpp` | `execNick()`: bloquea el nick "Marvin" para que ningun usuario lo use |
| `include/Channel.hpp` / `src/Channel.cpp` | Añadido getter `getWhitelist()` para que `!who` pueda mostrar la lista de invitados |
| `Makefile` | Añadido `Bot.cpp` a SRCS |

#### Arquitectura: ¿por que un pseudo-cliente interno?

El bot **NO** es un cliente real. No tiene socket, no tiene fd, no esta en `_sessions`.
Es un objeto `Bot*` dentro de `Server` que genera respuestas como strings. Las
ventajas de este enfoque:

1. **Sin overhead de red:** no consume un fd ni un slot de poll(). No genera trafico
   de socket.
2. **Sin complejidad de conexion:** no necesita PASS/NICK/USER, no puede desconectarse.
3. **Acceso directo a datos del servidor:** `botReply()` en Server.cpp tiene acceso
   completo a `_sessions` y `_rooms`, lo que permite a los comandos `!info` y `!who`
   mostrar informacion detallada (nicknames, canales, modos, whitelists, etc.).
4. **Sin programa externo:** un solo binario `ircserv` con todo incluido.

El bot tiene identidad IRC completa para que sus mensajes se vean naturales:

```cpp
#define BOT_NICK "Marvin"
#define BOT_USER "marvin"
#define BOT_HOST "hitchhiker.galaxy"

// fullId() devuelve:  Marvin!marvin@hitchhiker.galaxy
```

#### Bot.hpp: la clase Bot

```cpp
class Bot
{
    private:
        std::string _nick;       // "Marvin"
        std::string _user;       // "marvin"
        std::string _host;       // "hitchhiker.galaxy"
        time_t      _bootTime;   // Momento de creacion (para calcular uptime)

        Bot(const Bot&);                // OCF: copia privada (no copiable)
        Bot& operator=(const Bot&);     // OCF: asignacion privada

        // 9 handlers de comandos simples (privados)
        std::string cmdHelp() const;
        std::string cmdAsk() const;
        std::string cmdQuote() const;
        std::string cmdCheer() const;
        std::string cmdRps(const std::string& choice) const;
        std::string cmdRoll(const std::string& dice) const;
        std::string cmdCoin() const;
        std::string cmdTime() const;
        std::string cmdRules() const;

        static const char* pickRandom(const char* const arr[], size_t len);

    public:
        Bot();
        ~Bot();

        const std::string& getNick() const;
        std::string         fullId() const;
        time_t              uptime() const;

        // Comandos simples (sin datos del servidor)
        std::string handleCommand(const std::string& text) const;

        // Formateadores para comandos ricos en datos (llamados desde botReply)
        std::string fmtInfo(size_t clients, size_t channels,
                        const std::vector<std::string>& nickList,
                        const std::vector<std::string>& chanList) const;
        std::string fmtWho(const std::string& chanName,
                        const std::vector<std::string>& users,
                        const std::string& modes,
                        const std::vector<std::string>& whitelist,
                        size_t curUsers, int cap) const;
};
```

**Puntos clave:**
- Los `cmd*()` son **privados**. Para comandos simples, el punto de entrada publico es
  `handleCommand()`, que actua como router.
- Para comandos que necesitan datos del servidor (`!info`, `!who`), se usan los metodos
  publicos `fmtInfo()` y `fmtWho()`, que son llamados directamente desde `botReply()`
  en Server.cpp, donde los datos estan disponibles.
- `pickRandom()` es `static` porque no usa ningun atributo de instancia: simplemente
  elige un elemento aleatorio de un array de strings constantes.
- El `_bootTime` se guarda al construir el bot y se usa en `!info` para mostrar
  cuanto tiempo lleva el servidor encendido.

#### Bot.cpp: implementacion

**Constructor:**
```cpp
Bot::Bot()
    : _nick(BOT_NICK), _user(BOT_USER), _host(BOT_HOST),
      _bootTime(std::time(NULL)) {}
```

**handleCommand() -- el router de comandos simples:**
```cpp
std::string Bot::handleCommand(const std::string& text) const
{
    if (text.empty() || text[0] != '!')
        return "";                        // Ignora si no es un !comando

    // Extraer el comando y argumento
    std::string cmd, arg;
    size_t sp = text.find(' ');
    if (sp == std::string::npos)
        cmd = text.substr(1);             // "!help" -> cmd="help"
    else
    {
        cmd = text.substr(1, sp - 1);     // "!rps rock" -> cmd="rps"
        arg = text.substr(sp + 1);        //                arg="rock"
    }

    // Convertir a minusculas (case-insensitive)
    for (size_t i = 0; i < cmd.size(); i++)
        cmd[i] = std::tolower(cmd[i]);

    // Despachar al handler correspondiente
    if (cmd == "help")   return cmdHelp();
    if (cmd == "ask")    return cmdAsk();
    if (cmd == "quote")  return cmdQuote();
    if (cmd == "cheer")  return cmdCheer();
    if (cmd == "rps")    return cmdRps(arg);
    if (cmd == "roll")   return cmdRoll(arg);
    if (cmd == "coin")   return cmdCoin();
    if (cmd == "time")   return cmdTime();
    if (cmd == "rules")  return cmdRules();
    return "";           // Comando desconocido: no responder
}
```

**Nota:** `!info` y `!who` NO estan en este router. Son interceptados en
`Server::botReply()` antes de llegar aqui, porque necesitan datos del servidor
(lista de clientes, canales, modos, whitelists, etc.).

**Los comandos organizados por categoria:**

##### Personalidad Marvin: `!ask`, `!quote`, `!cheer`

Estos tres comandos eligen una respuesta aleatoria de arrays constantes usando
`pickRandom()`:

- **`!ask`** (10 respuestas): Simula que le haces una pregunta a Marvin. Respuestas
  como *"The answer is 42. It's always 42."* o *"My brain is the size of a planet
  and you ask me THIS?"*

- **`!quote`** (12 respuestas): Citas reales del personaje de Douglas Adams.
  *"Life? Don't talk to me about life."*, *"Here I am, brain the size of a planet..."*

- **`!cheer`** (8 respuestas): Intentos fallidos de animar al usuario.
  *"Cheer up? ...I was going to say something encouraging, but I forgot what
  joy feels like."*

```cpp
static const char* Bot::pickRandom(const char* const arr[], size_t len)
{
    return arr[std::rand() % len];
}
```

**Nota sobre `std::srand()`:** La semilla se inicializa UNA sola vez en el
constructor de Server con `std::srand(std::time(NULL))`. Si no inicializaras
la semilla, `std::rand()` devolveria la misma secuencia cada vez que arranques
el servidor.

##### Juegos: `!rps`, `!roll`, `!coin`

- **`!rps <rock|paper|scissors>`**: Piedra-papel-tijeras contra Marvin.

  ```cpp
  char human = std::tolower(choice[0]);   // Solo mira la primera letra: r, p, o s
  char bot = moves[std::rand() % 3];       // Marvin elige al azar

  // Logica de victoria circular: r>s, s>p, p>r
  // Si (bot+1)%3 == human -> humano gana
  // Si no es empate ni victoria humana -> bot gana
  ```

  Ejemplo: `!rps rock` -> *"I chose scissors. You win. Enjoy it. Joy is fleeting,
  after all."*

- **`!roll <NdM>`**: Tirada de dados estilo D&D. Acepta formato NdM (N dados de M caras).

  ```cpp
  // Parsea "2d6" -> count=2, sides=6
  // Limites: 1-20 dados, 2-100 caras
  // Tira cada dado individualmente, muestra detalle y total
  ```

  Ejemplo: `!roll 3d6` -> *"Rolled 3d6: [4+2+6] = 12"*
  Si sacas todos 1: *"All ones. The dice understand my existence."*
  Si sacas maximo: *"Perfect roll. The universe mocks us with false hope."*

- **`!coin`**: Lanzamiento de moneda. 50/50 heads/tails.
  *"Heads. Not that either side leads anywhere meaningful."*

##### Utilidad del servidor: `!time`, `!info`, `!who`, `!rules`

- **`!time`**: Muestra la hora actual del servidor usando `std::ctime()`.
  *"Current time: Sat Mar 14 12:00:00 2026. Another moment closer to the heat
  death of the universe."*

- **`!info`**: Muestra estadisticas completas del servidor en tiempo real.
  Este comando es manejado directamente por `Server::botReply()` porque necesita
  acceso a `_sessions` y `_rooms`. Llama a `_bot->fmtInfo()` pasando:
  - Numero de clientes y canales
  - Vector con los nicknames de todos los clientes conectados
  - Vector con los nombres de todos los canales activos

  ```cpp
  // En Server::botReply():
  std::vector<std::string> nickList;
  for (it = _sessions.begin(); it != _sessions.end(); ++it)
      nickList.push_back(it->second->getNick());

  std::vector<std::string> chanList;
  for (it = _rooms.begin(); it != _rooms.end(); ++it)
      chanList.push_back(it->first);

  reply = _bot->fmtInfo(_sessions.size(), _rooms.size(), nickList, chanList);
  ```

  Ejemplo de respuesta (multilinea con formato IRC):
  ```
  --- Server Info ---
  Clients: 3 | Channels: 2 | Uptime: 1h 23m 45s
  Connected users: alice, bob, charlie
  Active channels: #general, #random
  I've been running for 1 hours. Each one more pointless than the last.
  ```

- **`!who <#canal>[,#canal2,...]`**: Muestra informacion detallada de uno o mas
  canales. Acepta nombres separados por comas. Este comando tambien es manejado
  por `botReply()`. Para cada canal, muestra:
  - Lista de usuarios (con `@` para operadores, + el bot)
  - Modos activos del canal (+i, +t, +k, +l)
  - Si tiene modo `+l`: capacidad actual/maxima
  - Si tiene modo `+i`: la whitelist de invitados

  ```cpp
  // En Server::botReply():
  std::vector<std::string> users;
  for (it = grp.begin(); it != grp.end(); ++it)
  {
      std::string entry;
      if (ch->isModerator(*it))
          entry += "@";
      entry += (*it)->getNick();
      users.push_back(entry);
  }
  users.push_back(_bot->getNick());  // Marvin siempre aparece

  // Construye string de modos: "+itk", "+l", etc.
  // Obtiene whitelist con ch->getWhitelist()
  reply += _bot->fmtWho(chanName, users, modes, wl,
      ch->headcount() + 1, ch->getCap());
  ```

  Ejemplo: `!who #test`
  ```
  --- #test ---
  Users (3): @alice, bob, Marvin
  Modes: +itl
  Capacity: 3/20
  Invite whitelist: charlie, dave
  ```

  Ejemplo con multiples canales: `!who #test,#general`
  -> Muestra info de ambos canales, uno despues del otro.

- **`!rules`**: Reglas del canal con humor Marvin.
  *"1) Be respectful. 2) No spam. 3) Don't ask me to be happy.
  4) The answer is always 42. 5) Don't panic. (I always panic.)"*

- **`!help`**: Lista todos los comandos disponibles con descripcion breve.
  Muestra una respuesta multilinea con cada comando en su propia linea:
  ```
  I suppose you want me to list my capabilities. How tedious.
  !ask       - Ask me anything (don't expect comfort)
  !quote     - A quote from yours truly
  !cheer     - Attempt at encouragement (don't get your hopes up)
  !rps <r|p|s> - Rock, paper, scissors (utterly pointless)
  !roll <NdM>  - Roll dice (e.g. 2d6)
  !coin      - Flip a coin
  !time      - Current server time
  !info      - Server statistics
  !who <#ch>  - Channel details (comma-separated for multiple)
  !rules     - Channel rules
  ```

#### Flujo de integracion: como llega un !comando al bot

```
Usuario                          Servidor                          Bot (interno)
  |                                |                                |
  |--- PRIVMSG #test :!roll 2d6 ->|                                |
  |                                |  execPrivmsg():                |
  |                                |    1. Valida destino y texto   |
  |                                |    2. ch->relay(msg, c)        |
  |                                |       (todos ven el !roll)     |
  |                                |    3. text[0]=='!' -> true     |
  |                                |    4. botReply(c, "#test",     |
  |                                |       "!roll 2d6")             |
  |                                |       |                        |
  |                                |       +-- Parsea cmd="roll"    |
  |                                |       |   No es "info"/"who"   |
  |                                |       |   -> handleCommand()   |
  |                                |       |                        |
  |                                |       |            cmd="roll"--|-->cmdRoll("2d6")
  |                                |       |            arg="2d6"   |   return "Rolled
  |                                |       |                        |   2d6: [3+5] = 8"
  |                                |       |<----- reply string ----|
  |                                |       |                        |
  |                                |    5. ch->relay(               |
  |                                |       ":Marvin!marvin@         |
  |                                |       hitchhiker.galaxy        |
  |                                |       PRIVMSG #test            |
  |                                |       :Rolled 2d6: ...")       |
  |                                |                                |
  |<-- :Marvin!... PRIVMSG #test --|                                |
  |    :Rolled 2d6: [3+5] = 8     |                                |
```

**Flujo especial para !info / !who:**
```
  |--- PRIVMSG #test :!info ------>|                                |
  |                                |  botReply():                   |
  |                                |    1. Parsea cmd="info"        |
  |                                |    2. cmd == "info" -> true    |
  |                                |    3. Recopila nickList de     |
  |                                |       _sessions, chanList de   |
  |                                |       _rooms                   |
  |                                |    4. _bot->fmtInfo(sizes,  ---|-->fmtInfo()
  |                                |       nickList, chanList)      |   formatea y
  |                                |       |                        |   devuelve string
  |                                |       |<----- reply string ----|   multilinea
  |                                |    5. Envia cada linea como    |
  |                                |       un PRIVMSG separado      |
```

#### botReply(): el puente entre execPrivmsg y Bot (Server.cpp)

```cpp
void Server::botReply(Client* c, const std::string& dest,
    const std::string& text)
{
    std::string reply;

    // Parsea el comando y argumento
    std::string cmd, arg;
    if (!text.empty() && text[0] == '!')
    {
        size_t sp = text.find(' ');
        if (sp == std::string::npos)
            cmd = text.substr(1);
        else
        {
            cmd = text.substr(1, sp - 1);
            arg = text.substr(sp + 1);
        }
        for (size_t i = 0; i < cmd.size(); i++)
            cmd[i] = std::tolower(cmd[i]);
    }

    // Comandos ricos en datos: manejados aqui con acceso a _sessions/_rooms
    if (cmd == "info")
    {
        // Recopila nicknames y nombres de canales
        std::vector<std::string> nickList, chanList;
        for (it = _sessions.begin(); it != _sessions.end(); ++it)
            nickList.push_back(it->second->getNick());
        for (it = _rooms.begin(); it != _rooms.end(); ++it)
            chanList.push_back(it->first);
        reply = _bot->fmtInfo(_sessions.size(), _rooms.size(),
            nickList, chanList);
    }
    else if (cmd == "who")
    {
        // Parsea canales separados por comas, recopila datos de cada uno
        // Construye lista de usuarios con @prefix para ops
        // Construye string de modos (+itk, +l)
        // Obtiene whitelist con ch->getWhitelist()
        // Llama a _bot->fmtWho() para cada canal
    }
    else
    {
        // Comandos simples: delegados al router del Bot
        reply = _bot->handleCommand(text);
    }

    if (reply.empty())
        return;

    // Envia cada linea de la respuesta como un PRIVMSG separado
    std::string prefix = ":" + _bot->fullId() + " PRIVMSG " + dest + " :";
    std::istringstream ss(reply);
    std::string line;
    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            continue;                       // Salta lineas vacias
        if (dest[0] == '#' || dest[0] == '&')
        {
            Channel* ch = locateRoom(dest);
            if (ch)
                ch->relay(prefix + line + "\r\n");
        }
        else
            transmit(c->socketFd(), prefix + line + "\r\n");
    }
}
```

**Detalles importantes:**
- `botReply()` ahora tiene un patron de despacho en tres niveles:
  1. Primero parsea el comando y argumento del texto.
  2. Si es `!info` o `!who`, recopila datos del servidor y llama a los formateadores
     publicos del bot (`fmtInfo()`, `fmtWho()`).
  3. Para todo lo demas, delega a `_bot->handleCommand()`.
- Si `reply` es vacio (comando desconocido), el bot no responde nada.
- Las lineas vacias se saltan para no enviar PRIVMSG vacios.
- En canal: usa `ch->relay()` **sin skip** (NULL), asi que TODOS los miembros ven
  la respuesta del bot, incluyendo el que hizo la pregunta.
- En privado: usa `transmit()` al fd del usuario que envio el mensaje.

#### Las 3 formas de hablar con Marvin

**1. En un canal (con `!`):**
```
PRIVMSG #test :!quote
```
Marvin responde en el canal. Todos los miembros lo ven.

**2. Directamente a Marvin (con o sin `!`):**
```
PRIVMSG Marvin :!help
PRIVMSG Marvin :cualquier cosa
```
En `execPrivmsg()`, hay una rama especifica:
```cpp
else if (dest == _bot->getNick())
{
    botReply(c, c->getNick(), text);
}
```
Si el destino es "Marvin", se pasa directamente a `botReply()` con el nick del
emisor como destino de respuesta. El bot responde en privado. Nota: si el texto
no empieza con `!`, `handleCommand()` devuelve vacio y no se envia nada.

**3. Marvin en la lista de miembros:**

En `listMembers()` (Server.cpp), se añade el nick del bot al final de la lista
de miembros de CADA canal:
```cpp
if (!names.empty())
    names += " ";
names += _bot->getNick();    // Marvin aparece en todos los canales
```
Resultado en el cliente IRC:
```
353 alice = #test :@alice bob Marvin
```

#### Proteccion del nick del bot

En `execNick()` (CmdAuth.cpp), se bloquea el nick "Marvin":
```cpp
if ((clash && clash != c) || wanted == _bot->getNick())
{
    sendNumeric(c, ERR::NICKNAMEINUSE, who,
        wanted + " :Nickname is already in use");
    return;
}
```
Si alguien intenta `/NICK Marvin`, recibe error 433 como si el nick estuviera
en uso. Esto previene conflictos de identidad con el bot.

#### Preguntas de defensa sobre el bot

**P: ¿Por que el bot no es un cliente externo?**
R: Porque como pseudo-cliente interno no consume un fd de socket, no necesita
autenticacion, no puede desconectarse accidentalmente, y tiene acceso directo a
las estadisticas del servidor para `!info` y `!who`. Es mas eficiente y mas robusto.

**P: ¿Puede el bot responder a NOTICE?**
R: No. Solo responde a PRIVMSG. Esto es correcto segun RFC 1459: NOTICE no debe
generar respuestas automaticas para evitar bucles infinitos entre bots.

**P: ¿Que pasa si dos usuarios envian un !comando simultaneamente?**
R: No hay problema de concurrencia porque el servidor es single-threaded. Los
mensajes se procesan secuencialmente en el bucle de poll(). Cada !comando se
procesa completamente antes de pasar al siguiente.

**P: ¿El bot tiene memoria? ¿Guarda estado entre comandos?**
R: No. Cada comando es independiente (stateless). El unico "estado" es `_bootTime`
para calcular el uptime. Los juegos (rps, roll, coin) usan `std::rand()` que es
global, pero no se guarda ningun historial.

**P: ¿Como se asegura la aleatoriedad en los juegos?**
R: `std::srand(std::time(NULL))` se llama una sola vez en el constructor de Server.
`std::rand()` genera numeros pseudo-aleatorios a partir de esa semilla. No es
criptograficamente seguro, pero es mas que suficiente para juegos de un bot IRC.

**P: ¿Hay memory leaks del bot?**
R: No. `_bot` se crea con `new Bot()` en el constructor de Server y se libera con
`delete _bot` en el destructor. Las respuestas del bot son `std::string` que se
gestionan automaticamente por RAII.

#### Test del bot con socat

```bash
# Terminal 1: Servidor
./ircserv 6667 pass123

# Terminal 2: Cliente
socat - TCP:127.0.0.1:6667
PASS pass123
NICK alice
USER alice 0 * :Alice
JOIN #test

# Comandos del bot en canal:
PRIVMSG #test :!help
PRIVMSG #test :!quote
PRIVMSG #test :!rps rock
PRIVMSG #test :!roll 2d6
PRIVMSG #test :!coin
PRIVMSG #test :!time
PRIVMSG #test :!info
PRIVMSG #test :!who #test
PRIVMSG #test :!rules
PRIVMSG #test :!ask what is the meaning of life?
PRIVMSG #test :!cheer

# Mensaje directo al bot:
PRIVMSG Marvin :!help

# Intentar robar el nick del bot (debe fallar):
NICK Marvin
```

Respuesta esperada de `!help` (multilinea, cada linea es un PRIVMSG separado):
```
:Marvin!marvin@hitchhiker.galaxy PRIVMSG #test :I suppose you want me to list my capabilities. How tedious.
:Marvin!marvin@hitchhiker.galaxy PRIVMSG #test :!ask       - Ask me anything (don't expect comfort)
:Marvin!marvin@hitchhiker.galaxy PRIVMSG #test :!quote     - A quote from yours truly
...etc
```

---

## 16. Guia de testing

### Test basico con socat

```bash
# Terminal 1: Servidor
make && ./ircserv 6667 pass123

# Terminal 2: Cliente
socat - TCP:127.0.0.1:6667
PASS pass123
NICK alice
USER alice 0 * :Alice
JOIN #test
PRIVMSG #test :Hola!
QUIT :bye
```

### Test de datos parciales (requerido por subject)

```bash
# Terminal 2:
nc -C 127.0.0.1 6667
# Escribe "PAS" y pulsa Ctrl+D (envia sin newline)
# Escribe "S pa" y pulsa Ctrl+D
# Escribe "ss123" y pulsa Enter (envia \r\n)
# Si el servidor acepta el PASS, funciona correctamente
```

### Test multi-cliente con canales

```bash
# Terminal 2 (Alice):
socat - TCP:127.0.0.1:6667
PASS pass123
NICK alice
USER alice 0 * :Alice
JOIN #general

# Terminal 3 (Bob):
socat - TCP:127.0.0.1:6667
PASS pass123
NICK bob
USER bob 0 * :Bob
JOIN #general
PRIVMSG #general :Hola alice!
PRIVMSG alice :Mensaje privado
```

### Test de modos de canal

```bash
# Como alice (operadora de #test):
MODE #test +i                          # Invite-only
MODE #test +k secreto                  # Password
MODE #test +l 5                        # Limite 5 usuarios
MODE #test +t                          # Topic solo para ops
MODE #test +o bob                      # Dar op a bob
TOPIC #test :Solo ops pueden cambiar   # Funciona porque alice es op

# Combinado:
MODE #test +itk-l clave                # Activar i,t,k con clave. Desactivar l.
```

### Test con cliente IRC real (irssi)

```bash
irssi -c 127.0.0.1 -p 6667 -w pass123 -n miNick
```

Dentro de irssi:
```
/join #test
/msg #test Hola desde irssi!
/msg bob Hola en privado
/topic #test Nuevo topic
/mode #test +i
/invite bob #test
/kick #test bob :razon
/quit bye
```

### Checklist de edge cases para probar

- [ ] Conectar sin enviar PASS e intentar JOIN -> debe dar error 451
- [ ] Enviar PASS con contraseña incorrecta -> error 464
- [ ] Intentar NICK con un nick ya en uso -> error 433
- [ ] Intentar NICK con caracteres invalidos (ej: "ni ck", "123nick") -> error 432
- [ ] JOIN a canal +i sin invitacion -> error 473
- [ ] JOIN a canal +k con clave incorrecta -> error 475
- [ ] JOIN a canal +l lleno -> error 471
- [ ] KICK sin ser operador -> error 482
- [ ] TOPIC con +t sin ser operador -> error 482
- [ ] MODE con flag desconocido (ej: +x) -> error 472
- [ ] PRIVMSG a nick que no existe -> error 401
- [ ] PRIVMSG a canal que no existe -> error 403
- [ ] Desconexion abrupta (cerrar terminal) -> servidor NO debe crashear
- [ ] Enviar datos parciales con Ctrl+D -> se acumulan y procesan al recibir \n
- [ ] Buffer overflow (mas de 4096 bytes sin \n) -> cliente desconectado
- [ ] Ctrl+C en el servidor -> shutdown limpio, sin leaks
- [ ] Auto-operador: unico op sale con PART -> otro usuario recibe +o automatico
- [ ] Auto-operador: unico op es KICKeado -> otro usuario recibe +o automatico
- [ ] Auto-operador: unico op se desconecta -> otro usuario recibe +o automatico

### Checklist de bonus para probar

- [ ] `!help` en canal -> Marvin responde con lista de comandos (multilinea)
- [ ] `!quote` en canal -> Marvin responde con cita aleatoria
- [ ] `!ask` en canal -> respuesta depresiva aleatoria
- [ ] `!cheer` en canal -> intento fallido de animar
- [ ] `!rps rock` / `!rps paper` / `!rps scissors` -> resultado del juego
- [ ] `!rps` sin argumento -> mensaje de uso
- [ ] `!rps banana` -> error de eleccion invalida
- [ ] `!roll 2d6` -> resultado con detalle de dados
- [ ] `!roll` sin argumento -> mensaje de uso
- [ ] `!roll 100d100` -> error de limites (max 20 dados)
- [ ] `!coin` -> heads o tails
- [ ] `!time` -> hora actual del servidor
- [ ] `!info` -> clientes, canales, uptime, lista de nicks, lista de canales
- [ ] `!who #test` -> usuarios (con @ops), modos, capacidad, whitelist
- [ ] `!who #test,#general` -> info de multiples canales
- [ ] `!who #noexiste` -> mensaje de error
- [ ] `!rules` -> reglas del canal
- [ ] `PRIVMSG Marvin :!help` -> respuesta en privado
- [ ] `NICK Marvin` -> error 433 (nick protegido)
- [ ] `!comandofalso` -> Marvin no responde (silencio)
- [ ] Marvin aparece en NAMES de todos los canales
- [ ] DCC SEND entre dos clientes irssi -> transferencia funciona

---

> **Ultima nota:** El codigo cubre todos los requisitos mandatory y ambos bonus
> (file transfer via DCC y bot Marvin). Estudia esta guia en el orden propuesto,
> y practica especialmente las preguntas de las secciones 14 y 15, y los tests de
> la seccion 16.
