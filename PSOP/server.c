#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#include "threadpool.h"

#define PORT 8080
#define BUFFER_SIZE 1048576
#define THREAD_POOL_SIZE 8
#define QUEUE_SIZE 50

typedef struct 
{
    char titlu[100];
    char autor[100];
    int an;
    char pdf_file[100];
    char image[100];
} carte;

carte carti[100];
int numar_carti = 0;

const char *get_file_extension(const char *file_name) 
{
    const char *dot = strrchr(file_name, '.');

    if (dot == NULL || dot == file_name) 
    {
        // daca nu exista extensie, returneaza un sir gol
        return "";
    } 
    else 
    {
        // daca exista o extensie, returneaza extensia 
        return dot + 1;
    }
}

//tipurile MIME sunt importante pentru a specifica tipul de continut atunci cand sunt trimise fisiere prin HTTP 
const char *get_mime_type(const char *file_ext) 
{
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) 
    {
        return "text/html";
    }

    if (strcasecmp(file_ext, "txt") == 0) 
    {
        return "text/plain";
    }

    if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0)
    {
         return "image/jpeg";
    }

    if (strcasecmp(file_ext, "png") == 0)
    {
        return "image/png";
    }

    if (strcasecmp(file_ext, "pdf") == 0) 
    {
        return "application/pdf";
    }

    return "application/octet-stream";
}

void load_books()
{
    FILE *file = fopen("public/carti.txt", "r");

    if (!file) 
    {
        perror("Nu s-a putut deschide fisierul carti.txt");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%[^,], %[^,], %d, %[^,], %[^\n]\n",
                  carti[numar_carti].titlu,
                  carti[numar_carti].autor,
                  &carti[numar_carti].an,
                  carti[numar_carti].pdf_file,
                  carti[numar_carti].image) == 5) 
    {
        printf("Carte incarcata: %s - %s (%d)\n", carti[numar_carti].titlu, carti[numar_carti].autor, carti[numar_carti].an);
        numar_carti++;   
    }

    fclose(file);
}

int is_valid_extension_for_download(const char *file_ext) 
{
    if (strcmp(file_ext, "png") == 0 || strcmp(file_ext, "jpg") == 0 || strcmp(file_ext, "pdf") == 0 || strcmp(file_ext, "jpeg") == 0) 
    {
        return 1;
    }
    return 0;
}

void build_http_response(const char *file_name, const char *file_ext, char *response, size_t *response_len, int client_fd) 
{
    const char *mime_type = get_mime_type(file_ext);

    // daca fisierul este valid pentru descarcare (imagine sau PDF), va include si antetul Content-Disposition pentru a sugera descarcarea fisierului
     if (is_valid_extension_for_download(file_ext)==1)
     {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                 mime_type, file_name);
        printf("Raspuns HTTP: %s\n", response);
    } 
    else 
    {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n\r\n",
                 mime_type);
        printf("Raspuns HTTP: %s\n", response);
    }

    int file_fd = open(file_name, O_RDONLY);
    // daca fisierul nu este gasit, va trimite un raspuns de eroare 404
    if (file_fd == -1) 
    {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        send(client_fd, response, *response_len, 0);
        return;
    }

    *response_len = strlen(response);
    // client_fd-> descriptorul de fisier pentru conexiunea cu clientul
    send(client_fd, response, *response_len, 0);// trimite raspunsul HTTP (adica antetele) catre client

    ssize_t bytes_read; //numarul de octeti cititi din fisierul solicitat 

    //se citeste fisierul in bucati de dimensiunea BUFFER_SIZE; fiecare bucată citită este trimisă clientului
    while ((bytes_read = read(file_fd, response, BUFFER_SIZE)) > 0) 
    {
        send(client_fd, response, bytes_read, 0);
    }

    close(file_fd);
}

void handle_books_page(int client_fd) 
{
    char response[BUFFER_SIZE * 2] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
    strcat(response,
           "<!DOCTYPE html><html lang='ro'><head><meta charset='UTF-8'><title>Listă de cărți</title>"
           "<style>"
           "body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }"
           "h1 { text-align: center; color: #333; }"
           ".book-list { list-style-type: none; padding: 0; }"
           ".book-item { background-color: #fff; padding: 15px; margin-bottom: 10px; border-radius: 5px; box-shadow: 0 0 5px rgba(0,0,0,0.1); display: flex; align-items: center; justify-content: space-between; }"
           ".book-info { flex: 1; }"
           ".button { color: white; background-color: #28a745; padding: 10px; border: none; border-radius: 5px; text-decoration: none; margin-right: 5px; }"
           ".button:hover { background-color: #218838; }"
           ".button-group { display: flex; gap: 10px; }"
           "</style>"
           "</head><body><h1>Listă de cărți</h1><ul class='book-list'>");

    for (int i = 0; i < numar_carti; i++) 
    {
        char book_entry[BUFFER_SIZE];
        snprintf(book_entry, sizeof(book_entry),
                 "<li class='book-item'>"
                 "<div class='book-info'><strong>%s</strong> - %s (%d)</div>"
                 "<div class='button-group'>"
                 "<a href='/download/%s' class='button'>Descarcă coperta</a>"
                 "<a href='/download/%s' class='button'>Descarcă PDF</a>"
                 "</div>"
                 "</li>",
                 carti[i].titlu, carti[i].autor, carti[i].an,
                 carti[i].image, carti[i].pdf_file);
        strncat(response, book_entry, sizeof(response) - strlen(response) - 1);
    }

    strcat(response, "</ul></body></html>");
    send(client_fd, response, strlen(response), 0);
}

void replace_plus_with_space(char *str) 
{
    while (*str) 
    {
        if (*str == '+') 
        {
            *str = ' ';
        }
        str++;
    }
}

int extract_form_data(const char *body, char *nume, char *prenume) 
{
    char *nume_start = strstr(body, "nume=");
    char *prenume_start = strstr(body, "prenume=");
    
    if (nume_start && prenume_start) 
    {
        nume_start += 5;  // trecem peste "nume="
        prenume_start += 8;  // trecem peste "prenume="
        
        // extragem valorile pentru nume si prenume
        char *nume_end = strchr(nume_start, '&');
        char *prenume_end = strchr(prenume_start, '&');

        if (nume_end) 
        {
            strncpy(nume, nume_start, nume_end - nume_start);
            nume[nume_end - nume_start] = '\0';
        } 
        else 
        {
            strcpy(nume, nume_start);
        }

        if (prenume_end) 
        {
            strncpy(prenume, prenume_start, prenume_end - prenume_start);
            prenume[prenume_end - prenume_start] = '\0';
        } 
        else 
        {
            strcpy(prenume, prenume_start);
        }
        
        return 1;  
    }

    return 0;  // nu s-au gasit datele
}

char* run_script_feedback(const char *nume, const char *prenume) 
{
    static char response[BUFFER_SIZE];
    int pipefd[2];

    // cream un pipe pentru comunicarea intre procesul parinte si procesul copil
    if (pipe(pipefd) == -1) 
    {
        strcpy(response, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nEroare la crearea pipe-ului.");
        return response;
    }

    pid_t pid = fork();
    if (pid == 0) 
    {
        close(pipefd[0]); // inchidem capatul de citire al pipe-ului in procesul copil

        // redirectionam stdout si stderr catre pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]); // inchidem capatul de scriere al pipe-ului dupa redirectionare

        const char *script_path = "public/script.sh";
        char *args[] = { (char *)script_path, (char *)nume, (char *)prenume, NULL };

        execvp(args[0], args); //executam scriptul
    } 
    else if (pid > 0) 
    {
        close(pipefd[1]); // inchidem capătul de scriere al pipe-ului in procesul parinte

        wait(NULL); // asteptam terminarea procesului copil

        strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n");

        // citim iesirea scriptului din pipe si o adaugam in raspuns
        char output_line[256];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], output_line, sizeof(output_line) - 1)) > 0) 
        {
            output_line[bytes_read] = '\0'; 
            strcat(response, output_line);
        }

        close(pipefd[0]); // inchidem capatul de citire al pipe-ului
    } 
    else 
    {
        strcpy(response, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nEroare la crearea procesului.");
    }

    return response;
}


void handle_feedback_submission(const char *body, int client_fd) 
{
    char nume[100] = "", prenume[100] = "";

    printf("Datele formularului: %s\n", body);

    if (!extract_form_data(body, nume, prenume)) 
    {
        char error_response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nEroare la procesarea cererii.";
        send(client_fd, error_response, strlen(error_response), 0);
        return;
    }

    // inlocuim '+' cu spatii in nume si prenume
    replace_plus_with_space(nume);
    replace_plus_with_space(prenume);

    // rulam scriptul si obtinem raspunsul
    char *script_response = run_script_feedback(nume, prenume);

    send(client_fd, script_response, strlen(script_response), 0);
}

/*
char *url_decode(const char *src) 
{
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; ++i) 
    {
        if (src[i] == '%' && i + 2 < src_len) 
        {
            char hex[3] = { src[i + 1], src[i + 2], '\0' };
            decoded[decoded_len++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } 
        else 
        {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';

    return decoded;
}
*/

void handle_client(void *arg) 
{
    int client_fd = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) 
    {
        printf("Cerere primita:\n%s\n", buffer);
        if (strncmp(buffer, "GET / ", 6) == 0) 
        {
            build_http_response("public/index.html", "html", buffer, (size_t *)&bytes_received, client_fd);
        } 
        else if (strstr(buffer, "GET /books")) 
        {
            handle_books_page(client_fd);
        } 
        else if (strstr(buffer, "GET /download/")) 
        {
            char *file_name = strstr(buffer, "GET /download/") + strlen("GET /download/");
            char *end_of_file = strstr(file_name, " ");
            *end_of_file = '\0';

            char file_path[BUFFER_SIZE];
            snprintf(file_path, sizeof(file_path), "public/%s", file_name);

            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_path));

            build_http_response(file_path, file_ext, buffer, (size_t *)&bytes_received, client_fd);
        } 
        else if (strstr(buffer, "GET /feedback"))
        {
            build_http_response("public/index2.html", "html", buffer, (size_t *)&bytes_received, client_fd);
        }
        else if (strstr(buffer, "POST /feedback"))
        {
            // printf("%s", buffer);
            // intr-o cerere HTTP exista un separator intre antete si corp
            // urmatorul strstr returneaza un pointer catre inceputul acestei structuri
            char *body = strstr(buffer, "\r\n\r\n"); 
            if (body) 
            {
                body += 4; // sarim peste separator, cei 4 octeti
                handle_feedback_submission(body, client_fd);
            }
        }
        else 
        {
            snprintf(buffer, BUFFER_SIZE, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nPagina nu a fost gasita.");
            send(client_fd, buffer, strlen(buffer), 0);
        }
    }

    close(client_fd);

    //return NULL;
}

void start_server() 
{
    int server_fd;
    struct sockaddr_in server_addr;

    // se creeaza un socket de tip TCP/IP (SOCK_STREAM) folosind familia de adrese IPv4 (AF_INET)
    //-> socket() returneaza un descriptor de fisier (un întreg) care reprezinta socketul creat
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; // folosim IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // serverul asculta pe toate interfetele disponibile ale serverului (nu doar pe una specifica)
    server_addr.sin_port = htons(PORT); // seteaza portul pe care serverul va asculta

    // legarea socketului de adresa si port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // serverul asculta conexiunile de intrare pe socketul creat (pana la 10 conexiuni simultane in asteptare)
    if (listen(server_fd, 10) < 0) 
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    tpool_t *tp;
    tp = tpool_create(4);


    printf("Server listening on port %d\n", PORT);

    // bucla principala pentru acceptarea conexiunilor 
    while (1) 
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) 
        {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        ///pthread_t thread;
        //pthread_create(&thread, NULL, handle_client, client_fd);
        //pthread_detach(thread); // thread-ul este detasat, adica nu va fi nevoie sa asteptam finalizarea acestuia in mod explicit (el se va termina independent de thread-ul principal)
        tpool_add_work(tp,handle_client,client_fd);

    }
    tpool_wait(tp); 
    tpool_destroy(tp);
    close(server_fd);
}

int main() {
    load_books();
    start_server();
    return 0;
}