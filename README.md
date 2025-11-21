# Cliente FTP Concurrente en C

**Autor:** Daniel Ismael Flores Espín  
**Asignatura:** Computación Distribuida

## Descripción

Cliente FTP completo implementado en C que soporta operaciones concurrentes mediante hilos POSIX (pthreads). Permite la transferencia simultánea de múltiples archivos, así como la reanudación de descargas interrumpidas.

## Consideraciones Iniciales
- Cambio en vsftpd
  - Asegurarse que el directorio home del usuario tenga permisos de escritura
  - Configurar `allow_writeable_chroot=YES` en `/etc/vsftpd.conf`

## Características Principales

### Funcionalidades Básicas
- ✅ Autenticación FTP (USER/PASS)
- ✅ Navegación de directorios (PWD)
- ✅ Listado de archivos (LIST)
- ✅ Creación de carpetas (MKD)
- ✅ Eliminación de archivos (DELE)

### Características Avanzadas
- **Transferencias Concurrentes**: Subida y descarga de múltiples archivos en paralelo
- **Reanudación de Descargas**: Soporte para comando REST (resume downloads)
- **Modo Activo y Pasivo**: Alternancia entre PORT (activo) y PASV (pasivo)
- **Multithreading**: Cada transferencia se ejecuta en un hilo independiente

## Requisitos

- Sistema operativo Linux/Unix
- Compilador GCC
- Bibliotecas estándar de C
- pthread library

## Compilación
```bash
make
```

## Uso

### Ejecución Básica
```bash
# Conectar a localhost en puerto por defecto (21)
./FloresD-clienteFTP
# Por defecto es localhost:21

# Especificar servidor y puerto
./FloresD-clienteFTP IP PUERTO
```

### Credenciales

Al iniciar, el programa solicitará:
- **Usuario**: Nombre de usuario FTP
- **Contrasenia**: Contraseña del servidor FTP

## Menú de Opciones
```
============ CLIENTE FTP ============
Modo: [PASV (Pasivo)]
1. PWD (Directorio actual)
2. LIST (Ver archivos)
3. MKD (Crear carpeta)
4. DELE (Borrar archivo)
5. Cambiar Modo (PORT/PASV)
6. STOR MULTIPLE (Subir en lote)
7. RETR MULTIPLE (Descargar en lote)
8. REST (Reanudar descarga)
9. Salir
```

## Ejemplos de Uso

### Subir Múltiples Archivos
```
Opcion > 6
Ingrese archivos a subir (separados por espacio): archivo1.txt imagen.jpg documento.pdf
--> [1] Subiendo 'archivo1.txt'...
--> [2] Subiendo 'imagen.jpg'...
--> [3] Subiendo 'documento.pdf'...
```

### Descargar Múltiples Archivos
```
Opcion > 7
Ingrese archivos a descargar (separados por espacio): datos.csv informe.pdf
--> [1] Descargando 'datos.csv'...
--> [2] Descargando 'informe.pdf'...
```

### Reanudar Descarga Interrumpida
```
Opcion > 8
Archivo a REANUDAR (debe existir localmente): archivo_grande.zip
--> Reanudacion iniciada en segundo plano.
[Hilo archivo_grande.zip] Resumiendo desde 5242880 bytes.
```

### Cambiar entre Modo Activo y Pasivo
```
Opcion > 5
>>> MODO ACTUAL: ACTIVO (PORT) <
```

## Arquitectura Técnica

### Modos de Transferencia

#### Modo Pasivo (PASV) - Por Defecto
- El servidor abre un puerto y proporciona la dirección al cliente
- Ideal para clientes detrás de NAT/firewall
- El cliente inicia la conexión de datos

#### Modo Activo (PORT)
- El cliente abre un puerto y lo comunica al servidor
- El servidor inicia la conexión de datos
- Requiere configuración de firewall adecuada

### Hilos de Ejecución

Cada transferencia (STOR/RETR) se ejecuta en su propio hilo:

- **Hilo Principal**: Maneja la interfaz de usuario y el socket de control
- **Hilos de Descarga**: Función `hiloDescarga()` para RETR
- **Hilos de Subida**: Función `hiloEnvio()` para STOR

### Reanudación de Descargas (REST)

1. Verifica el tamaño del archivo local existente
2. Envía comando `REST <offset>` al servidor
3. Continúa la descarga desde el byte especificado
4. Abre el archivo en modo append (`"ab"`)

## Estructura del Código
```
FloresD-clienteFTP.c
├── main()                  # Punto de entrada y bucle principal
├── Utilidades
│   ├── mostrarMenu()       # Interfaz de usuario
│   ├── ftp_leer_respuesta()
│   ├── ftp_login()
│   └── enviarComandoSimple()
├── Comandos FTP
│   ├── comandoPASV()       # Modo pasivo
│   ├── comandoPORT()       # Modo activo
│   ├── comandoLIST()
│   ├── comandoMKD()
│   └── comandoDELE()
└── Hilos Concurrentes
    ├── hiloDescarga()      # RETR + REST
    └── hiloEnvio()         # STOR
```

## Constantes Importantes
```c
#define LINELEN         128    // Longitud de línea de entrada
#define CTRL_BUF_SIZE   512    // Buffer de control
#define DATA_BUF_SIZE   4096   // Buffer de datos
```

## Limitaciones Conocidas

- Los hilos se ejecutan en modo detached (no hay espera activa)
- No hay visualización de progreso durante las transferencias
- Sin soporte para comandos avanzados como SITE o CHMOD
- Manejo básico de errores en transferencias concurrentes

## Seguridad

⚠️ **Advertencias de Seguridad:**
- Las contraseñas se transmiten en texto plano (protocolo FTP estándar)
- No soporta FTPS o SFTP
- Uso recomendado solo en redes de confianza o para pruebas

## Solución de Problemas

### Error de conexión
```bash
# Verificar que el servidor FTP esté activo
telnet servidor_ftp 21
```

### Timeout en modo activo
- Revisar configuración de firewall
- Cambiar a modo pasivo (Opción 5)

### Fallos en reanudación
- Verificar que el archivo local exista
- Asegurarse de que el servidor soporte REST

## Referencias

- [RFC 959 - File Transfer Protocol](https://tools.ietf.org/html/rfc959)
- [RFC 2428 - FTP Extensions for IPv6](https://tools.ietf.org/html/rfc2428)

## Licencia

Código desarrollado con fines académicos para la asignatura de Computación Distribuida.