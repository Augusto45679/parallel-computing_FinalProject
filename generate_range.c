#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

/**
 * @brief Baraja un arreglo de enteros usando el algoritmo Fisher-Yates.
 * @param array El arreglo a barajar.
 * @param n El número de elementos en el arreglo.
 */
void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            int temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }
}

/**
 * @brief Muestra las instrucciones de uso del programa y termina la ejecución.
 * @param prog_name El nombre del ejecutable (argv[0]).
 */
void print_usage_and_exit(const char *prog_name) {
    fprintf(stderr, "Uso: %s <cantidad_N> <archivo_salida> <valor_min> <valor_max> [semilla]\n\n", prog_name);
    fprintf(stderr, "Argumentos:\n");
    fprintf(stderr, "  <cantidad_N>      Número de enteros ÚNICOS a generar.\n");
    fprintf(stderr, "  <archivo_salida>  Nombre del archivo donde se guardarán los números.\n");
    fprintf(stderr, "  <valor_min>       Valor mínimo (inclusivo) para los números generados.\n");
    fprintf(stderr, "  <valor_max>       Valor máximo (exclusivo) para los números generados.\n");
    fprintf(stderr, "  [semilla]         (Opcional) Semilla para el generador de números aleatorios para reproducibilidad.\n\n");
    fprintf(stderr, "Ejemplo:\n");
    fprintf(stderr, "  %s 1000 datos.txt 10000 20000\n", prog_name);
    fprintf(stderr, "  (Genera 1000 números únicos entre 10000 y 19999 en 'datos.txt')\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // --- 1. Validación de Argumentos ---
    if (argc < 5 || argc > 6) {
        print_usage_and_exit(argv[0]);
    }

    long N_long = atol(argv[1]);
    if (N_long <= 0 || N_long > INT_MAX) {
        fprintf(stderr, "Error: La cantidad de números <cantidad_N> debe ser un entero positivo.\n");
        return EXIT_FAILURE;
    }
    int N = (int)N_long;

    const char *output_filename = argv[2];

    long min_val_long = atol(argv[3]);
    long max_val_long = atol(argv[4]);

    if (min_val_long >= max_val_long) {
        fprintf(stderr, "Error: <valor_min> (%ld) debe ser estrictamente menor que <valor_max> (%ld).\n", min_val_long, max_val_long);
        return EXIT_FAILURE;
    }

    long range_size_long = max_val_long - min_val_long;
    if (range_size_long < N) {
        fprintf(stderr, "Error: No se pueden generar %d números únicos en un rango de solo %ld valores.\n", N, range_size_long);
        fprintf(stderr, "Asegúrate de que (valor_max - valor_min) sea >= cantidad_N.\n");
        return EXIT_FAILURE;
    }
    if (range_size_long > INT_MAX || min_val_long < INT_MIN || max_val_long > INT_MAX) {
        fprintf(stderr, "Error: El rango de valores o su tamaño exceden los límites de un entero de 32 bits.\n");
        return EXIT_FAILURE;
    }
    int min_val = (int)min_val_long;
    int range_size = (int)range_size_long;

    unsigned int seed;
    if (argc == 6) {
        seed = (unsigned int)atoi(argv[5]);
        printf("Usando semilla proporcionada: %u\n", seed);
    } else {
        seed = (unsigned int)time(NULL);
        printf("Usando semilla basada en el tiempo actual: %u\n", seed);
    }
    srand(seed);

    // --- 2. Apertura del Archivo de Salida ---
    FILE *file = fopen(output_filename, "w");
    if (!file) {
        perror("Error abriendo el archivo de salida");
        return EXIT_FAILURE;
    }

    printf("Generando %d números únicos en el rango [%d, %ld) y guardando en '%s'...\n", N, min_val, max_val_long, output_filename);

    // --- 3. Generación de Números Únicos (Fisher-Yates Shuffle) ---
    int *all_numbers = (int *)malloc(range_size * sizeof(int));
    if (!all_numbers) {
        perror("Error de asignación de memoria para el barajado");
        fclose(file);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < range_size; i++) {
        all_numbers[i] = min_val + i;
    }

    shuffle(all_numbers, range_size);

    // --- 4. Escritura de Datos ---
    fprintf(file, "%d\n", N);

    for (int i = 0; i < N; i++) {
        if (fprintf(file, "%d\n", all_numbers[i]) < 0) {
            perror("Error escribiendo un número en el archivo");
            free(all_numbers);
            fclose(file);
            return EXIT_FAILURE;
        }
    }

    free(all_numbers);
    fclose(file);

    printf("¡Archivo '%s' generado exitosamente!\n", output_filename);

    return EXIT_SUCCESS;
}