#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>

// --- Implementación de una Tabla Hash simple para enteros ---

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct HashTable {
    int size;
    Node** table;
} HashTable;

/** @brief Crea una tabla hash de un tamaño dado. */
HashTable* create_hash_table(int size) {
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    if (!ht) return NULL;
    ht->size = size;
    ht->table = (Node**)calloc(size, sizeof(Node*)); // calloc inicializa a NULL
    if (!ht->table) {
        free(ht);
        return NULL;
    }
    return ht;
}

/** @brief Función de hash simple. */
unsigned int hash_function(int key, int size) {
    return (unsigned int)key % size;
}

/** @brief Inserta una clave en la tabla hash. No verifica duplicados. */
void hash_insert(HashTable* ht, int key) {
    unsigned int index = hash_function(key, ht->size);
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) return; // Fallo silencioso en caso de error de memoria
    new_node->key = key;
    new_node->next = ht->table[index];
    ht->table[index] = new_node;
}

/** @brief Busca una clave en la tabla hash. Devuelve true si la encuentra. */
bool hash_search(HashTable* ht, int key) {
    unsigned int index = hash_function(key, ht->size);
    Node* current = ht->table[index];
    while (current) {
        if (current->key == key) {
            return true;
        }
        current = current->next;
    }
    return false;
}

/** @brief Libera toda la memoria usada por la tabla hash. */
void free_hash_table(HashTable* ht) {
    if (!ht) return;
    for (int i = 0; i < ht->size; i++) {
        Node* current = ht->table[i];
        while (current) {
            Node* temp = current;
            current = current->next;
            free(temp);
        }
    }
    free(ht->table);
    free(ht);
}

/** @brief Genera un número aleatorio de 64 bits para un rango más amplio. */
long long rand_64() {
    return ((long long)rand() << 32) | rand();
}

void print_usage_and_exit(const char *prog_name) {
    fprintf(stderr, "Uso: %s <cantidad_N> <archivo_salida> [semilla]\n\n", prog_name);
    fprintf(stderr, "Genera <cantidad_N> números enteros ÚNICOS en el rango completo [INT_MIN, INT_MAX].\n\n");
    fprintf(stderr, "Argumentos:\n");
    fprintf(stderr, "  <cantidad_N>      Número de enteros a generar (ej. 1000000).\n");
    fprintf(stderr, "  <archivo_salida>  Nombre del archivo de salida.\n");
    fprintf(stderr, "  [semilla]         (Opcional) Semilla para reproducibilidad.\n\n");
    fprintf(stderr, "Ejemplo para generar 1 millón de números:\n");
    fprintf(stderr, "  %s 1000000 datos_1M.txt\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        print_usage_and_exit(argv[0]);
    }

    long N_long = atol(argv[1]);
    if (N_long <= 0 || N_long > 100000000) { // Límite práctico
        fprintf(stderr, "Error: La cantidad de números debe ser un entero positivo (límite práctico 100M).\n");
        return EXIT_FAILURE;
    }
    int N = (int)N_long;

    const char *output_filename = argv[2];

    unsigned int seed;
    if (argc == 4) {
        seed = (unsigned int)atoi(argv[3]);
        printf("Usando semilla proporcionada: %u\n", seed);
    } else {
        seed = (unsigned int)time(NULL);
        printf("Usando semilla basada en el tiempo actual: %u\n", seed);
    }
    srand(seed);

    // --- Tabla Hash para verificar unicidad ---
    // Un buen tamaño es ~2x la cantidad de elementos para minimizar colisiones.
    int ht_size = N * 2;
    HashTable* used_numbers = create_hash_table(ht_size);
    if (!used_numbers) {
        perror("Error creando la tabla hash");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(output_filename, "w");
    if (!file) {
        perror("Error abriendo el archivo de salida");
        free_hash_table(used_numbers);
        return EXIT_FAILURE;
    }

    printf("Generando %d números únicos en el rango [%d, %d] y guardando en '%s'...\n", N, INT_MIN, INT_MAX, output_filename);

    // Escribir N en la primera línea
    fprintf(file, "%d\n", N);

    long long min_val = INT_MIN;
    long long max_val = INT_MAX;
    unsigned long long range = (unsigned long long)max_val - min_val + 1;

    for (int i = 0; i < N; ) {
        long long rand_val = min_val + (rand_64() % range);
        int num = (int)rand_val;

        if (!hash_search(used_numbers, num)) {
            hash_insert(used_numbers, num);
            if (fprintf(file, "%d ", num) < 0) { // Cambiado \n por un espacio
                perror("Error escribiendo en el archivo");
                break; // Salir del bucle en caso de error
            }
            i++; // Solo incrementar si el número fue único y se escribió
        }
    }

    fprintf(file, "\n"); // Añadimos un salto de línea final por buena práctica
    free_hash_table(used_numbers);
    fclose(file);

    printf("¡Archivo '%s' generado exitosamente!\n", output_filename);

    return EXIT_SUCCESS;
}
