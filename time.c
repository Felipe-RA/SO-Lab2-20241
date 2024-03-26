#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <comando>\n", argv[0]);
        return 1;
    }

    struct timeval start, end;
    pid_t pid;
    int status;

    // Obtener el tiempo de inicio
    gettimeofday(&start, NULL);

    pid = fork();
    if (pid == -1) {
        // Error al crear el proceso hijo
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Proceso hijo: ejecuta el comando
        execvp(argv[1], &argv[1]);
        // Si execvp retorna, entonces hubo un error
        perror("execvp");
        exit(1);
    } else {
        // Proceso padre: espera a que el hijo termine
        waitpid(pid, &status, 0);

        // Obtener el tiempo de finalización
        gettimeofday(&end, NULL);

        // Calcular y mostrar el tiempo transcurrido
        double elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        printf("Elapsed time: %.5f seconds\n", elapsedTime);
    }

    return 0;
}
