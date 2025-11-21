
/* ---- FloresD-clienteFTP ---- */

/****************************************
*		DANIEL ISMAEL FLORES ESPIN		*
*		COMPUTACION DISTRIBUIDA			*
*****************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

/* ---- Definiciones Externas ---- */
int errexit(const char *format, ...);
int connectTCP(const char *host, const char *service);

#define LINELEN         128
#define CTRL_BUF_SIZE   512
#define DATA_BUF_SIZE   4096

/* Estructura para pasar datos a los hilos */
typedef struct {
    char host[64];
    char service[16];
    char filename[128];
    char user[64];
    char pass[64];
    int  useActiveMode; /* 0=PASV, 1=PORT*/
    int  isResume;      /*0=Normal, 1=Usar REST*/
} ThreadArgs;

/* Variable global para el modo actual */
int g_activeMode = 0; 

void mostrarMenu(void);
int  ftp_leer_respuesta(int s, char *buf, size_t len);
int  ftp_login(int s, const char *user, const char *pass);
void enviarComandoSimple(int s, const char *comando);

int  comandoPASV(int s);                 
int  comandoPORT(int s_control);         
void comandoLIST(int s);
void comandoMKD(int s);
void comandoDELE(int s);

void *hiloEnvio(void *arg);
void *hiloDescarga(void *arg);

/* ---- MAIN ---- */
int main(int argc, char *argv[]) {
    const char *arg_host = "localhost";
    const char *arg_service = "21";
    char buf[LINELEN + 1];
    char temp_input[LINELEN];
    
    /* Variables de sesion persistentes */
    char currentHost[64];
    char currentUser[64];
    char currentPass[64];
    
    int s, n, opcion;

    /* Manejo de argumentos CLI */
    switch (argc) {
        case 1: break;
        case 2: arg_host = argv[1]; break;
        case 3: arg_host = argv[1]; arg_service = argv[2]; break;
        default: errexit("Uso: %s [host [puerto]]\n", argv[0]);
    }
    strcpy(currentHost, arg_host);

    /* Conexion Inicial (Main Thread) */
    s = connectTCP(currentHost, arg_service);
    if (s < 0) errexit("Error conectando a %s:%s\n", currentHost, arg_service);

    printf("\n=== CONECTADO A %s ===\n", currentHost);

    n = ftp_leer_respuesta(s, buf, sizeof(buf));
    if (n <= 0) errexit("Error de lectura\n");
    printf("Server: %s", buf);

    /* ---- LOGIN (Una sola vez) ---- */
    printf("\n--- Credenciales FTP ---\n");
    printf("Usuario: ");
    if (!fgets(temp_input, LINELEN, stdin)) errexit("Error entrada\n");
    temp_input[strcspn(temp_input, "\n")] = 0;
    strcpy(currentUser, temp_input);

    printf("Contrasenia: ");
    if (!fgets(temp_input, LINELEN, stdin)) errexit("Error entrada\n");
    temp_input[strcspn(temp_input, "\n")] = 0;
    strcpy(currentPass, temp_input);

    if (ftp_login(s, currentUser, currentPass) < 0) {
        errexit("Login rechazado\n");
    }
    printf(">>> Autenticacion Correcta \n");

    /* ---- Bucle Principal ---- */
    while (1) {
        mostrarMenu();
        if (scanf("%d", &opcion) != 1) {
            int c; while ((c = getchar()) != '\n' && c != EOF) {} 
            continue;
        }
        getchar(); /* Consumir '\n' */

        switch (opcion) {
            case 1: /* PWD */
                enviarComandoSimple(s, "PWD");
                break;

            case 2: /* LIST */
                comandoLIST(s);
                break;

            case 3: /* MKD */
                comandoMKD(s);
                break;

            case 4: /* DELE */
                comandoDELE(s);
                break;

            case 5: /* CAMBIAR MODO */
                g_activeMode = !g_activeMode;
                printf("\n>>> MODO ACTUAL: %s <<<\n", g_activeMode ? "ACTIVO (PORT)" : "PASIVO (PASV)");
                break;

            case 6: { /* STOR Concurrente (BATCH INPUT) */
                char inputLine[1024];
                printf("Ingrese archivos a subir (separados por espacio): ");
                if (!fgets(inputLine, sizeof(inputLine), stdin)) break;
                
                // Tokenizacion para archivos
                char *token = strtok(inputLine, " \n\r\t");
                int count = 0;

                while (token != NULL) {
                    ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
                    strcpy(args->filename, token);
                    strcpy(args->host, currentHost);
                    strcpy(args->service, arg_service);
                    strcpy(args->user, currentUser);
                    strcpy(args->pass, currentPass);
                    args->useActiveMode = g_activeMode;
                    args->isResume = 0;

                    pthread_t tid;
                    if (pthread_create(&tid, NULL, hiloEnvio, args) != 0) {
                        perror("Error hilo"); free(args);
                    } else {
                        printf("--> [%d] Subiendo '%s'...\n", ++count, token);
                    }
                    token = strtok(NULL, " \n\r\t"); // Siguiente archivo
                }
                if (count == 0) printf("No se ingresaron archivos.\n");
                break;
            }

            case 7: { /* RETR Concurrente (BATCH INPUT) */
                char inputLine[1024];
                printf("Ingrese archivos a descargar (separados por espacio): ");
                if (!fgets(inputLine, sizeof(inputLine), stdin)) break;
                
                char *token = strtok(inputLine, " \n\r\t");
                int count = 0;

                while (token != NULL) {
                    ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
                    strcpy(args->filename, token);
                    strcpy(args->host, currentHost);
                    strcpy(args->service, arg_service);
                    strcpy(args->user, currentUser);
                    strcpy(args->pass, currentPass);
                    args->useActiveMode = g_activeMode;
                    args->isResume = 0;

                    pthread_t tid;
                    if (pthread_create(&tid, NULL, hiloDescarga, args) != 0) {
                        perror("Error hilo"); free(args);
                    } else {
                        printf("--> [%d] Descargando '%s'...\n", ++count, token);
                    }
                    token = strtok(NULL, " \n\r\t");
                }
                if (count == 0) printf("No se ingresaron archivos.\n");
                break;
            }

            case 8: { /* REST + RETR (Reanudar 1 archivo) */
                ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
                printf("Archivo a REANUDAR (debe existir localmente): ");
                if (!fgets(args->filename, sizeof(args->filename), stdin)) { free(args); break; }
                args->filename[strcspn(args->filename, "\n")] = 0;

                strcpy(args->host, currentHost);
                strcpy(args->service, arg_service);
                strcpy(args->user, currentUser);
                strcpy(args->pass, currentPass);
                args->useActiveMode = g_activeMode;
                args->isResume = 1; /* Activar bandera REST */

                pthread_t tid;
                if (pthread_create(&tid, NULL, hiloDescarga, args) != 0) {
                    perror("Error hilo"); free(args);
                } else {
                    printf("--> Reanudacion iniciada en segundo plano.\n");
                }
                break;
            }

            case 9: /* QUIT */
                enviarComandoSimple(s, "QUIT");
                close(s);
                printf("Sesion finalizada.\n");
                return 0;

            default:
                printf("Opcion invalida.\n");
        }
    }
    return 0;
}

/* ---- Utilidades ---- */

void mostrarMenu(void) {
    printf("\n============ CLIENTE FTP ============\n");
    printf("Modo: [%s]\n", g_activeMode ? "PORT (Activo)" : "PASV (Pasivo)");
    printf("1. PWD (Directorio actual)\n");
    printf("2. LIST (Ver archivos)\n");
    printf("3. MKD (Crear carpeta)\n");
    printf("4. DELE (Borrar archivo)\n");
    printf("5. Cambiar Modo (PORT/PASV)\n");
    printf("6. STOR MULTIPLE (Subir en lote)\n");
    printf("7. RETR MULTIPLE (Descargar en lote)\n");
    printf("8. REST (Reanudar descarga)\n");
    printf("9. Salir\n");
    printf("Opcion > ");
}

int ftp_leer_respuesta(int s, char *buf, size_t len) {
    int n = read(s, buf, (int)len - 1);
    if (n > 0) buf[n] = '\0';
    else buf[0] = '\0';
    return n;
}

int ftp_login(int s, const char *user, const char *pass) {
    char buf[CTRL_BUF_SIZE];
    int n;

    snprintf(buf, sizeof(buf), "USER %s\r\n", user);
    write(s, buf, strlen(buf));
    n = ftp_leer_respuesta(s, buf, sizeof(buf));
    if (n <= 0 || strncmp(buf, "331", 3) != 0) return -1;

    snprintf(buf, sizeof(buf), "PASS %s\r\n", pass);
    write(s, buf, strlen(buf));
    n = ftp_leer_respuesta(s, buf, sizeof(buf));
    if (n <= 0 || strncmp(buf, "230", 3) != 0) return -1;
    
    return 0;
}

void enviarComandoSimple(int s, const char *comando) {
    char buf[CTRL_BUF_SIZE];
    snprintf(buf, sizeof(buf), "%s\r\n", comando);
    write(s, buf, strlen(buf));
    ftp_leer_respuesta(s, buf, sizeof(buf));
    printf("Server: %s", buf);
}

void comandoMKD(int s) {
    char buf[CTRL_BUF_SIZE];
    char path[LINELEN];
    printf("Nombre carpeta: ");
    if (fgets(path, sizeof(path), stdin)) {
        path[strcspn(path, "\n")] = 0;
        snprintf(buf, sizeof(buf), "MKD %s\r\n", path);
        write(s, buf, strlen(buf));
        ftp_leer_respuesta(s, buf, sizeof(buf));
        printf("Server: %s", buf);
    }
}

void comandoDELE(int s) {
    char buf[CTRL_BUF_SIZE];
    char path[LINELEN];
    printf("Archivo a borrar: ");
    if (fgets(path, sizeof(path), stdin)) {
        path[strcspn(path, "\n")] = 0;
        snprintf(buf, sizeof(buf), "DELE %s\r\n", path);
        write(s, buf, strlen(buf));
        ftp_leer_respuesta(s, buf, sizeof(buf));
        printf("Server: %s", buf);
    }
}

/* ---- Modos de Conexion ---- */
int comandoPASV(int s) {
    char buf[CTRL_BUF_SIZE];
    int h1, h2, h3, h4, p1, p2;
    char ip_str[64], port_str[16];
    
    write(s, "PASV\r\n", 6);
    if (ftp_leer_respuesta(s, buf, sizeof(buf)) <= 0) return -1;
    
    char *start = strchr(buf, '(');
    if (!start) return -1;
    sscanf(start + 1, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);

    sprintf(ip_str, "%d.%d.%d.%d", h1, h2, h3, h4);
    sprintf(port_str, "%d", p1 * 256 + p2);

    return connectTCP(ip_str, port_str);
}

int comandoPORT(int s_control) {
    int s_listen;
    struct sockaddr_in sin, my_addr;
    socklen_t len = sizeof(my_addr);
    unsigned char *ip, *p;
    char buf[LINELEN];
    
    s_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen < 0) return -1;
    
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = 0; 
    
    if (bind(s_listen, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind"); return -1;
    }
    getsockname(s_control, (struct sockaddr *)&my_addr, &len);
    len = sizeof(sin);
    getsockname(s_listen, (struct sockaddr *)&sin, &len);
    
    listen(s_listen, 1);
    
    ip = (unsigned char *)&my_addr.sin_addr;
    p  = (unsigned char *)&sin.sin_port; 
    
    snprintf(buf, sizeof(buf), "PORT %d,%d,%d,%d,%d,%d\r\n",
             ip[0], ip[1], ip[2], ip[3], p[0], p[1]); 
    write(s_control, buf, strlen(buf));
    
    ftp_leer_respuesta(s_control, buf, sizeof(buf));
    if (strncmp(buf, "200", 3) != 0) {
        close(s_listen); return -1;
    }
    return s_listen; 
}

void comandoLIST(int s) {
    char buf[DATA_BUF_SIZE];
    int n, dataSock = -1, listenSock = -1;
    
    if (g_activeMode) {
        listenSock = comandoPORT(s);
        if (listenSock < 0) return;
        write(s, "LIST\r\n", 6);
        ftp_leer_respuesta(s, buf, sizeof(buf)); 
        dataSock = accept(listenSock, NULL, NULL);
        close(listenSock);
    } else {
        dataSock = comandoPASV(s);
        if (dataSock < 0) return;
        write(s, "LIST\r\n", 6);
        ftp_leer_respuesta(s, buf, sizeof(buf)); 
    }

    if (dataSock >= 0) {
        printf("\n--- LISTADO ---\n");
        while ((n = read(dataSock, buf, sizeof(buf)-1)) > 0) {
            buf[n] = 0;
            printf("%s", buf);
        }
        printf("---------------\n");
        close(dataSock);
        ftp_leer_respuesta(s, buf, sizeof(buf));
    }
}

/* ---- Hilo Descarga (RETR + Resume) ---- */
void *hiloDescarga(void *arg) {
    pthread_detach(pthread_self());
    ThreadArgs *data = (ThreadArgs *)arg;
    char buf[DATA_BUF_SIZE];
    char resp[CTRL_BUF_SIZE];
    int s, dataSock = -1, listenSock = -1;
    int n;
    FILE *f;
    long resumeOffset = 0;

    s = connectTCP(data->host, data->service);
    if (s < 0) { free(data); pthread_exit(NULL); }
    
    ftp_leer_respuesta(s, resp, sizeof(resp));
    if (ftp_login(s, data->user, data->pass) < 0) {
        close(s); free(data); pthread_exit(NULL);
    }

    /* Logica REST */
    if (data->isResume) {
        struct stat st;
        if (stat(data->filename, &st) == 0) {
            resumeOffset = st.st_size;
            printf("[Hilo %s] Resumiendo desde %ld bytes.\n", data->filename, resumeOffset);
        }
    }

    /* Negociacion */
    if (data->useActiveMode) {
        listenSock = comandoPORT(s);
        if (listenSock < 0) { close(s); free(data); pthread_exit(NULL); }
    } else {
        dataSock = comandoPASV(s);
        if (dataSock < 0) { close(s); free(data); pthread_exit(NULL); }
    }

    if (resumeOffset > 0) {
        snprintf(buf, sizeof(buf), "REST %ld\r\n", resumeOffset);
        write(s, buf, strlen(buf));
        ftp_leer_respuesta(s, resp, sizeof(resp));
    }

    snprintf(buf, sizeof(buf), "RETR %s\r\n", data->filename);
    write(s, buf, strlen(buf));
    ftp_leer_respuesta(s, resp, sizeof(resp));

    if (data->useActiveMode) {
        dataSock = accept(listenSock, NULL, NULL);
        close(listenSock);
    }

    if (dataSock >= 0) {
        /* 'ab' para append si es resume, 'wb' para overwrite */
        f = fopen(data->filename, (resumeOffset > 0) ? "ab" : "wb");
        if (f) {
            while ((n = read(dataSock, buf, sizeof(buf))) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
            printf("[Hilo %s] Transferencia OK.\n", data->filename);
        }
        close(dataSock);
        ftp_leer_respuesta(s, resp, sizeof(resp));
    }

    close(s);
    free(data);
    pthread_exit(NULL);
}

/* ---- Hilo Subida (STOR) ---- */
void *hiloEnvio(void *arg) {
    pthread_detach(pthread_self());
    ThreadArgs *data = (ThreadArgs *)arg;
    char buf[DATA_BUF_SIZE];
    char resp[CTRL_BUF_SIZE];
    int s, dataSock = -1, listenSock = -1;
    int n;
    FILE *f;

    f = fopen(data->filename, "rb");
    if (!f) { free(data); pthread_exit(NULL); }

    s = connectTCP(data->host, data->service);
    if (s < 0) { fclose(f); free(data); pthread_exit(NULL); }

    ftp_leer_respuesta(s, resp, sizeof(resp));
    ftp_login(s, data->user, data->pass);

    if (data->useActiveMode) {
        listenSock = comandoPORT(s);
        if (listenSock < 0) { fclose(f); close(s); free(data); pthread_exit(NULL); }
        
        snprintf(buf, sizeof(buf), "STOR %s\r\n", data->filename);
        write(s, buf, strlen(buf));
        ftp_leer_respuesta(s, resp, sizeof(resp));
        
        dataSock = accept(listenSock, NULL, NULL);
        close(listenSock);
    } else {
        dataSock = comandoPASV(s);
        if (dataSock < 0) { fclose(f); close(s); free(data); pthread_exit(NULL); }
        
        snprintf(buf, sizeof(buf), "STOR %s\r\n", data->filename);
        write(s, buf, strlen(buf));
        ftp_leer_respuesta(s, resp, sizeof(resp));
    }

    if (dataSock >= 0) {
        printf("[Hilo %s] Subiendo...\n", data->filename);
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            write(dataSock, buf, n);
        }
        close(dataSock);
        ftp_leer_respuesta(s, resp, sizeof(resp));
        printf("[Hilo %s] Fin subida.\n", data->filename);
    }

    fclose(f);
    close(s);
    free(data);
    pthread_exit(NULL);
}
