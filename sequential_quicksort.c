#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Función de comparación para qsort
int compare_integers(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}
// Función para verificar si un número es primo (idéntica a la de parallel_quicksortV2.c)
bool is_prime(int n) {
    int i; // Declaración movida al principio del bloque
    if (n <= 1) return false;
    for (i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int i; // Declaración movida al principio del bloque

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_de_entrada>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error abriendo el archivo");
        return 1;
    }

    int N;
    
    // Leer N desde el archivo
    fscanf(file, "%d", &N);

    int *array = (int *)malloc(N * sizeof(int));
    if (!array) {
        perror("Error de asignación de memoria");
        fclose(file);
        return 1;
    }

    // Leer los datos desde el archivo
    for (i = 0; i < N; i++) {
        fscanf(file, "%d", &array[i]);
    }
    fclose(file);

    printf("Arreglo original (N=%d) leído desde %s.\n", N, argv[1]);

    // Iniciar el temporizador con MPI para consistencia
    double start_time, end_time;
    start_time = MPI_Wtime();

    // Realizar el ordenamiento Quick Sort secuencial
    qsort(array, N, sizeof(int), compare_integers);

    // Contar números primos
    int prime_count = 0;
    for (i = 0; i < N; i++) {
        if (is_prime(array[i])) {
            prime_count++;
        }
    }

    // Detener el temporizador después de todo el trabajo
    end_time = MPI_Wtime();
    double time_used = end_time - start_time;

    printf("\n--- Resultados Secuenciales ---\n");
    printf("Arreglo ordenado correctamente.\n"); // No imprimimos el arreglo completo por defecto para grandes N
    printf("Total de números primos encontrados: %d\n", prime_count);
    printf("Tiempo de ejecución total: %f segundos\n", time_used);

    free(array);
    MPI_Finalize();

    return 0;
}
