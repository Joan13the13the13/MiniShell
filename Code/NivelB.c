/*
nivelA.c - Adelaida Delgado (adaptación de nivel3.c)
Cada nivel incluye la funcionalidad de la anterior.
nivel A: Muestra el prompt, captura la línea de comandos, 
la trocea en tokens y chequea si se trata de comandos internos 
(cd, export, source, jobs, fg, bg o exit). Si son externos los ejecuta con execvp()
*/

#define _POSIX_C_SOURCE 200112L

#define DEBUGN1 1 //parse_args()
#define DEBUGN3 1 //execute_line()

#define PROMPT_PERSONAL 1 // si no vale 1 el prompt será solo el carácter de PROMPT

#define RESET_FORMATO "\033[0m"
#define NEGRO_T "\x1b[30m"
#define NEGRO_F "\x1b[40m"
#define GRIS "\x1b[94m"
#define ROJO_T "\x1b[31m"
#define VERDE_T "\x1b[32m"
#define AMARILLO_T "\x1b[33m"
#define AZUL_T "\x1b[34m"
#define MAGENTA_T "\x1b[35m"
#define CYAN_T "\x1b[36m"
#define BLANCO_T "\x1b[97m"
#define NEGRITA "\x1b[1m"

#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define N_JOBS 24 // cantidad de trabajos permitidos

char const PROMPT = '$';

#include <errno.h>  //errno
#include <stdio.h>  //printf(), fflush(), fgets(), stdout, stdin, stderr, fprintf()
#include <stdlib.h> //setenv(), getenv()
#include <string.h> //strcmp(), strtok(), strerror()
#include <unistd.h> //NULL, getcwd(), chdir()
#include <sys/types.h> //pid_t
#include <sys/wait.h>  //wait()
#include <signal.h> //signal(), SIG_DFL, SIG_IGN, SIGINT, SIGCHLD
#include <fcntl.h> //O_WRONLY, O_CREAT, O_TRUNC
#include <sys/stat.h> //S_IRUSR, S_IWUSR

int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs(char **args);
int internal_bg(char **args);
int internal_fg(char **args);

char *read_line(char *line);
int parse_args(char **args, char *line);
int execute_line(char *line);

//métodos añadidos por nosotros
void initJL();
void reaper();
void ctrlc();
void ctrlz();


static char mi_shell[COMMAND_LINE_SIZE]; //variable global para guardar el nombre del minishell

//static pid_t foreground_pid = 0;
struct info_process {
	pid_t pid;
	char status;
	char cmd[COMMAND_LINE_SIZE];
};
static struct info_process jobs_list[N_JOBS]; //Tabla de procesos. La posición 0 será para el foreground

void imprimir_prompt();

int check_internal(char **args) {
    if (!strcmp(args[0], "cd"))
        return internal_cd(args);
    if (!strcmp(args[0], "export"))
        return internal_export(args);
    if (!strcmp(args[0], "source"))
        return internal_source(args);
    if (!strcmp(args[0], "jobs"))
        return internal_jobs(args);
    if (!strcmp(args[0], "bg"))
        return internal_bg(args);
    if (!strcmp(args[0], "fg"))
        return internal_fg(args);
    if (!strcmp(args[0], "exit"))
        exit(0);
    return 0; // no es un comando interno
}

int internal_cd(char **args) {
    printf("[internal_cd()→ comando interno no implementado]\n");
    return 1;
} 

int internal_export(char **args) {
    printf("[internal_export()→ comando interno no implementado]\n");
    return 1;
}

int internal_source(char **args) {
    printf("[internal_source()→ comando interno no implementado]\n");
    return 1;
}

int internal_jobs(char **args) {
    #if DEBUGN1 
        printf("[internal_jobs()→ Esta función mostrará el PID de los procesos que no estén en foreground]\n");
    #endif
    return 1;
}

int internal_fg(char **args) {
    #if DEBUGN1 
        printf("[internal_fg()→ Esta función enviará un trabajo detenido al foreground reactivando su ejecución, o uno del background al foreground ]\n");
    #endif
    return 1;
}

int internal_bg(char **args) {
    #if DEBUGN1 
        printf("[internal_bg()→ Esta función reactivará un proceso detenido para que siga ejecutándose pero en segundo plano]\n");
    #endif
    return 1;
}

void imprimir_prompt() {
#if PROMPT_PERSONAL == 1
    printf(NEGRITA ROJO_T "%s" BLANCO_T ":", getenv("USER"));
    printf(AZUL_T "MINISHELL" BLANCO_T "%c " RESET_FORMATO, PROMPT);
#else
    printf("%c ", PROMPT);

#endif
    fflush(stdout);
    return;
}


char *read_line(char *line) {
  
    imprimir_prompt();
    char *ptr=fgets(line, COMMAND_LINE_SIZE, stdin); // leer linea
    if (ptr) {
        // ELiminamos el salto de línea (ASCII 10) sustituyéndolo por el \0
        char *pos = strchr(line, 10);
        if (pos != NULL){
            *pos = '\0';
        } 
	}  else {   //ptr==NULL por error o eof
        printf("\r");
        if (feof(stdin)) { //se ha pulsado Ctrl+D
            fprintf(stderr,"Bye bye\n");
            exit(0);
        }   
    }
    return ptr;
}

int parse_args(char **args, char *line) {
    int i = 0;

    args[i] = strtok(line, " \t\n\r");
    #if DEBUGN1 
        fprintf(stderr, GRIS "[parse_args()→ token %i: %s]\n" RESET_FORMATO, i, args[i]);
    #endif
    while (args[i] && args[i][0] != '#') { // args[i]!= NULL && *args[i]!='#'
        i++;
        args[i] = strtok(NULL, " \t\n\r");
        #if DEBUGN1 
            fprintf(stderr, GRIS "[parse_args()→ token %i: %s]\n" RESET_FORMATO, i, args[i]);
        #endif
    }
    if (args[i]) {
        args[i] = NULL; // por si el último token es el símbolo comentario
        #if DEBUGN1 
            fprintf(stderr, GRIS "[parse_args()→ token %i corregido: %s]\n" RESET_FORMATO, i, args[i]);
        #endif
    }
    return i;
}

int execute_line(char *line) {
    char *args[ARGS_SIZE];
    pid_t pid, status;
    char command_line[COMMAND_LINE_SIZE];

    //copiamos la línea de comandos sin '\n' para guardarlo en el array de structs de los procesos
    memset(command_line, '\0', sizeof(command_line)); 
    strcpy(command_line, line); //antes de llamar a parse_args() que modifica line

    if (parse_args(args, line) > 0) {
        if (check_internal(args)) {
            return 1;
        }
        pid=fork();//creamos un proceso hijo

        if(pid==0){//hijo

            //NIVEL B
            signal(SIGCHLD,SIG_DFL);
            signal(SIGINT, SIG_IGN);

            fprintf(stderr, "\n[execute_line() --> PID hijo %d (%s) ",getpid(),args[0]);
            execvp(args[0],args);
            fprintf(stderr, "El comando no se ha ejecutado correctamente");
            exit(-1);

        }else if (pid>0) {//padre
            //le recordamos al padre que si se produce ctr+c, realice esa rutina
            //signal(SIGINT, ctrlc());

            fprintf(stderr, "[execute_line() --> PID Padre %d (%s) ",getpid(),mi_shell);
            printf("Soy el proceso padre, actualizare jobs_list[0]");
            printf("  Mi pid es: %d\n",getpid());
            
            //Actualizamos la información del proceso padre
            
            //actualizamos el pid con el pid del proceso hijo
            jobs_list[0].pid = pid;
            //actualizamos el status a 'E'
            jobs_list[0].status = 'F';
            //actualizamos el cmd con el proceso hijo
            strcpy(jobs_list[0].cmd, args[0]);

            //esperamos a la confirmación del cambio de estado del hijo

            //NIVEL B
            //wait(&status); //lo substituimos por signal(SIGCHLD,reaper());
            while(jobs_list[0].pid > 0){ //mientras haya un hijo ejecutandose en primer plano (foreground)
                pause();
            }

            //Reseteamos los valores de Job_list[0]
            initJL();
            
        }
        
        #if DEBUGN3
            fprintf(stderr, GRIS "[execute_line()→ PID padre: %d]\n" RESET_FORMATO, getpid());
        #endif
    }
    return 0;
}


//método para resetear los valores de Job_list[0]
void initJL(){
    //Ponemos el pid a 0
    jobs_list[0].pid = 0;
    //Ponemos el status a N
    //strcpy(jobs_list[0].status,'N');
    jobs_list[0].status='N';
    //Ponemos todo \0 en el cmd de jobs list
    memset(jobs_list[0].cmd,'\0',sizeof(jobs_list[0].cmd));
}

//método reaper
void reaper(){

    signal(SIGCHLD,reaper);
    pid_t ended;
    int status;

    while ((ended=waitpid(-1, &status, WNOHANG)) > 0) {
        jobs_list[0].pid=0;
        jobs_list[0].status='F';
      }

}

//método ctrlc
void ctrlc(){
    //signal(SIGINT,ctrlc);

    
    if((jobs_list[0].pid > 0)&&(jobs_list[0].cmd!=mi_shell)){
         //mientras haya un hijo ejecutandose en primer plano (foreground) y no es un minishell
         printf( "\n[ctrlc()→ Soy el proceso con PID %d (%s), el proceso en foreground es %d (%s)] ",getpid(),mi_shell,jobs_list[0].pid,jobs_list[0].cmd);
        kill(jobs_list[0].pid,SIGTERM);
    }else{
        printf( "\n[ctrlc() --> Soy el proceso con PID %d (%s)] ",getpid(),mi_shell);
        printf("\nSeñal 15 no enviada por %d (%s) debido a que no hay proceso en foreground]", getpid(),mi_shell);
    }

    //errores
}

//método ctrlz NIVEL C
void ctrlz(){

}



int main(int argc, char *argv[]) {
    //dejamos claro al padre que si se producen estas señales 
    //tenemos que hacer estas acciones
    signal(SIGCHLD,reaper);
    signal(SIGINT, ctrlc);
    signal(SIGTSTP, ctrlz);

    char line[COMMAND_LINE_SIZE];
    memset(line, 0, COMMAND_LINE_SIZE);

    //Introducios dentro de mi_shell el nombre del programa
    strcpy(mi_shell,argv[0]);
    

    //Inicializamos jobs_list[0]
    initJL();

    while (1) {
        if (read_line(line)) { // !=NULL
            execute_line(line);
        }
    }
    return 0;
}





