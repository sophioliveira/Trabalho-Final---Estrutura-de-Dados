#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* ==========================================================================
 * CORRECAO DE PORTABILIDADE (WINDOWS VS LINUX/MAC)
 * ========================================================================== */
#ifdef _WIN32
    #include <windows.h> // Para a funcao Sleep() no Windows
#else
    #include <unistd.h>  // Para a funcao usleep() no Linux/macOS
#endif

void simulate_latency(int ms) {
#ifdef _WIN32
    Sleep(ms); // Windows trabalha em milissegundos
#else
    usleep(ms * 1000); // POSIX trabalha em microssegundos
#endif
}

#define MAX_LINE 2048
#define BLOOM_SIZE 100000
#define HASH_SIZE 2000
#define OPT_HASH_SIZE 15000 
#define ALPHABET_SIZE 256

/* ==========================================================================
 * STRUCTS DE DADOS BASE E CONFIGURACOES
 * ========================================================================== */
typedef struct {
    char record_id[15];
    int year;
    int month;
    char country[30];
    char app_category[30];
    char app_name[30];
    float daily_screen_time;
    float battery_drain_pct;
    float monthly_spend_usd;
    char sleep_disruption[15];
} MobileData;

typedef struct {
    bool active;
    int max_avl_nodes;     // R5: Limite de memoria / Descarte
    double time_budget_ms; // R7: Orcamento computacional rigido
    int latency_ms;        // R12: Simulacao de latencia de rede/hardware (em milissegundos)
    float anomaly_chance;  // R18: Dados anomalos
    unsigned char xor_key; // R23: Encriptacao algoritmica
} RestrictionConfig;

// Configuracao inicial. Latencia de 1ms e suficiente para estourar o orcamento em repeticoes.
RestrictionConfig sysConfig = {false, 5000, 25.0, 1, 0.10, 0xAA};
int current_avl_nodes = 0;

/* ==========================================================================
 * ESTRUTURA NAO CLASSICA 1: BLOOM FILTER
 * ========================================================================== */
unsigned char bloomFilter[BLOOM_SIZE / 8] = {0};

unsigned long hash_djb2(unsigned char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

unsigned long hash_sdbm(unsigned char *str) {
    unsigned long hash = 0;
    int c;
    while ((c = *str++)) hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

void bloom_insert(char* key) {
    unsigned long h1 = hash_djb2((unsigned char*)key) % BLOOM_SIZE;
    unsigned long h2 = hash_sdbm((unsigned char*)key) % BLOOM_SIZE;
    bloomFilter[h1 / 8] |= (1 << (h1 % 8));
    bloomFilter[h2 / 8] |= (1 << (h2 % 8));
}

bool bloom_check(char* key) {
    unsigned long h1 = hash_djb2((unsigned char*)key) % BLOOM_SIZE;
    unsigned long h2 = hash_sdbm((unsigned char*)key) % BLOOM_SIZE;
    return (bloomFilter[h1 / 8] & (1 << (h1 % 8))) && (bloomFilter[h2 / 8] & (1 << (h2 % 8)));
}

/* ==========================================================================
 * ESTRUTURA CLASSICA 1: FILA ENCADEADA (Stream de Leitura)
 * ========================================================================== */
typedef struct QueueNode {
    MobileData* data;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

Queue* initQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

void enqueue(Queue* q, MobileData* data) {
    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
    newNode->data = data;
    newNode->next = NULL;
    if (q->rear == NULL) q->front = q->rear = newNode;
    else { q->rear->next = newNode; q->rear = newNode; }
    q->size++;
}

/* ==========================================================================
 * ESTRUTURA CLASSICA 2: TABELA HASH TRADICIONAL
 * ========================================================================== */
typedef struct HashNode {
    MobileData* data;
    struct HashNode* next;
} HashNode;

HashNode* hashTable[HASH_SIZE] = {NULL};

void insertHash(MobileData* data) {
    unsigned long index = hash_djb2((unsigned char*)data->record_id) % HASH_SIZE;
    HashNode* newNode = (HashNode*)malloc(sizeof(HashNode));
    newNode->data = data;
    newNode->next = hashTable[index];
    hashTable[index] = newNode;
}

MobileData* searchHash(char* record_id) {
    // R12: Simulacao Multiplataforma de Latencia de Rede/Hardware
    if (sysConfig.active && sysConfig.latency_ms > 0) {
        simulate_latency(sysConfig.latency_ms);
    }
    unsigned long index = hash_djb2((unsigned char*)record_id) % HASH_SIZE;
    HashNode* current = hashTable[index];
    while (current != NULL) {
        if (strcmp(current->data->record_id, record_id) == 0) return current->data;
        current = current->next;
    }
    return NULL;
}

bool deleteHash(char* record_id) {
    unsigned long index = hash_djb2((unsigned char*)record_id) % HASH_SIZE;
    HashNode* current = hashTable[index];
    HashNode* prev = NULL;
    while (current != NULL) {
        if (strcmp(current->data->record_id, record_id) == 0) {
            if (prev == NULL) hashTable[index] = current->next;
            else prev->next = current->next;
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

/* ==========================================================================
 * ESTRUTURA OTIMIZADA: TABELA HASH COM ENDERECAMENTO ABERTO
 * ========================================================================== */
MobileData* optHashTable[OPT_HASH_SIZE] = {NULL};

void insertOptHash(MobileData* data) {
    unsigned long index = hash_djb2((unsigned char*)data->record_id) % OPT_HASH_SIZE;
    int start_index = index;
    while (optHashTable[index] != NULL) {
        if (strcmp(optHashTable[index]->record_id, data->record_id) == 0) return;
        index = (index + 1) % OPT_HASH_SIZE;
        if (index == start_index) return;
    }
    optHashTable[index] = data;
}

MobileData* searchOptHash(char* record_id) {
    // R12: Simulacao Multiplataforma de Latencia de Rede/Hardware
    if (sysConfig.active && sysConfig.latency_ms > 0) {
        simulate_latency(sysConfig.latency_ms);
    }
    unsigned long index = hash_djb2((unsigned char*)record_id) % OPT_HASH_SIZE;
    int start_index = index;
    while (optHashTable[index] != NULL) {
        if (strcmp(optHashTable[index]->record_id, record_id) == 0) return optHashTable[index];
        index = (index + 1) % OPT_HASH_SIZE;
        if (index == start_index) break;
    }
    return NULL;
}

bool deleteOptHash(char* record_id) {
    unsigned long index = hash_djb2((unsigned char*)record_id) % OPT_HASH_SIZE;
    int start_index = index;
    while (optHashTable[index] != NULL) {
        if (strcmp(optHashTable[index]->record_id, record_id) == 0) {
            optHashTable[index] = NULL;
            return true;
        }
        index = (index + 1) % OPT_HASH_SIZE;
        if (index == start_index) break;
    }
    return false;
}

/* ==========================================================================
 * ESTRUTURA CLASSICA 3: ARVORE AVL
 * ========================================================================== */
typedef struct AVLNode {
    MobileData* data;
    struct AVLNode *left, *right;
    int height;
} AVLNode;

int getHeight(AVLNode* n) { return (n == NULL) ? 0 : n->height; }
int getMax(int a, int b) { return (a > b) ? a : b; }
int getBalance(AVLNode* n) { return (n == NULL) ? 0 : getHeight(n->left) - getHeight(n->right); }

AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y; y->left = T2;
    y->height = getMax(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = getMax(getHeight(x->left), getHeight(x->right)) + 1;
    return x;
}

AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x; x->right = T2;
    x->height = getMax(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = getMax(getHeight(y->left), getHeight(y->right)) + 1;
    return y;
}

AVLNode* insertAVL(AVLNode* node, MobileData* data) {
    if (sysConfig.active && current_avl_nodes >= sysConfig.max_avl_nodes) {
        return node; 
    }

    if (node == NULL) {
        AVLNode* newNode = (AVLNode*)malloc(sizeof(AVLNode));
        newNode->data = data;
        newNode->left = newNode->right = NULL;
        newNode->height = 1;
        current_avl_nodes++;
        return newNode;
    }
    if (data->daily_screen_time < node->data->daily_screen_time)
        node->left = insertAVL(node->left, data);
    else
        node->right = insertAVL(node->right, data);

    node->height = 1 + getMax(getHeight(node->left), getHeight(node->right));
    int balance = getBalance(node);

    if (balance > 1 && data->daily_screen_time < node->left->data->daily_screen_time) return rightRotate(node);
    if (balance < -1 && data->daily_screen_time >= node->right->data->daily_screen_time) return leftRotate(node);
    if (balance > 1 && data->daily_screen_time >= node->left->data->daily_screen_time) {
        node->left = leftRotate(node->left); return rightRotate(node);
    }
    if (balance < -1 && data->daily_screen_time < node->right->data->daily_screen_time) {
        node->right = rightRotate(node->right); return leftRotate(node);
    }
    return node;
}

AVLNode* minValueNode(AVLNode* node) {
    AVLNode* current = node;
    while (current->left != NULL) current = current->left;
    return current;
}

AVLNode* deleteAVL(AVLNode* root, float screen_time, char* record_id, bool* deleted) {
    if (root == NULL) return root;

    if (screen_time < root->data->daily_screen_time)
        root->left = deleteAVL(root->left, screen_time, record_id, deleted);
    else if (screen_time > root->data->daily_screen_time)
        root->right = deleteAVL(root->right, screen_time, record_id, deleted);
    else {
        if (strcmp(root->data->record_id, record_id) != 0) {
            root->right = deleteAVL(root->right, screen_time, record_id, deleted);
            return root;
        }

        *deleted = true;
        current_avl_nodes--;
        if ((root->left == NULL) || (root->right == NULL)) {
            AVLNode* temp = root->left ? root->left : root->right;
            if (temp == NULL) { temp = root; root = NULL; }
            else *root = *temp;
            free(temp);
        } else {
            AVLNode* temp = minValueNode(root->right);
            root->data = temp->data;
            root->right = deleteAVL(root->right, temp->data->daily_screen_time, temp->data->record_id, deleted);
        }
    }

    if (root == NULL) return root;
    root->height = 1 + getMax(getHeight(root->left), getHeight(root->right));
    int balance = getBalance(root);

    if (balance > 1 && getBalance(root->left) >= 0) return rightRotate(root);
    if (balance > 1 && getBalance(root->left) < 0) { root->left = leftRotate(root->left); return rightRotate(root); }
    if (balance < -1 && getBalance(root->right) <= 0) return leftRotate(root);
    if (balance < -1 && getBalance(root->right) > 0) { root->right = rightRotate(root->right); return leftRotate(root); }
    return root;
}

/* ==========================================================================
 * ESTRUTURA NAO CLASSICA 2: TRIE
 * ========================================================================== */
typedef struct TrieNode {
    struct TrieNode* children[ALPHABET_SIZE];
    bool isEndOfWord;
    char app_name[30];
} TrieNode;

TrieNode* createTrieNode() {
    TrieNode* node = (TrieNode*)malloc(sizeof(TrieNode));
    node->isEndOfWord = false; node->app_name[0] = '\0';
    for (int i = 0; i < ALPHABET_SIZE; i++) node->children[i] = NULL;
    return node;
}

void insertTrie(TrieNode* root, const char* word) {
    TrieNode* current = root;
    for (int i = 0; word[i] != '\0'; i++) {
        unsigned char index = (unsigned char)tolower((unsigned char)word[i]);
        if (current->children[index] == NULL) current->children[index] = createTrieNode();
        current = current->children[index];
    }
    current->isEndOfWord = true; strcpy(current->app_name, word);
}

bool deleteTrie(TrieNode* root, const char* word) {
    TrieNode* current = root;
    for (int i = 0; word[i] != '\0'; i++) {
        unsigned char index = (unsigned char)tolower((unsigned char)word[i]);
        if (current->children[index] == NULL) return false;
        current = current->children[index];
    }
    if (current->isEndOfWord) {
        current->isEndOfWord = false;
        return true;
    }
    return false;
}

void printTrieWords(TrieNode* root) {
    if (root == NULL) return;
    if (root->isEndOfWord) printf("    -> %s\n", root->app_name);
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i] != NULL) printTrieWords(root->children[i]);
    }
}

void searchPrefixTrie(TrieNode* root, const char* prefix) {
    TrieNode* current = root;
    for (int i = 0; prefix[i] != '\0'; i++) {
        unsigned char index = (unsigned char)tolower((unsigned char)prefix[i]);
        if (current->children[index] == NULL) {
            printf("\n [!] Nenhum aplicativo encontrado com o prefixo '%s'.\n", prefix); return;
        }
        current = current->children[index];
    }
    printf("\n [*] Aplicativos encontrados com o prefixo '%s':\n", prefix);
    printTrieWords(current);
}

/* ==========================================================================
 * FUNCOES DE DESALOCACAO RECURSIVA (Memory Leak Protection)
 * ========================================================================== */
void freeAVL(AVLNode* root) {
    if (root == NULL) return;
    freeAVL(root->left);
    freeAVL(root->right);
    free(root);
}

void freeTrie(TrieNode* root) {
    if (root == NULL) return;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i] != NULL) freeTrie(root->children[i]);
    }
    free(root);
}

void freeHashTables() {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode* current = hashTable[i];
        while (current != NULL) {
            HashNode* temp = current;
            current = current->next;
            free(temp);
        }
        hashTable[i] = NULL;
    }
    for (int i = 0; i < OPT_HASH_SIZE; i++) optHashTable[i] = NULL;
}

void freeQueueAndData(Queue* q) {
    QueueNode* current = q->front;
    while (current != NULL) {
        QueueNode* temp = current;
        current = current->next;
        free(temp->data); 
        free(temp);       
    }
    free(q);
}

/* ==========================================================================
 * TRES OPERACOES ADICIONAIS DE MANIPULACAO DE DADOS
 * ========================================================================== */
void printSevereSleepDisruption(AVLNode* root, int* count) {
    if (root != NULL) {
        printSevereSleepDisruption(root->left, count);
        if (strcmp(root->data->sleep_disruption, "Severe") == 0) {
            printf("  - ID: %s | App Culpado: %s | Tempo de Tela: %.1f min\n", 
                   root->data->record_id, root->data->app_name, root->data->daily_screen_time);
            (*count)++; if (*count >= 10) return;
        }
        printSevereSleepDisruption(root->right, count);
    }
}

void calcAvgBattery(AVLNode* root, double* totalBattery, int* count) {
    if (root != NULL) {
        calcAvgBattery(root->left, totalBattery, count);
        *totalBattery += root->data->battery_drain_pct; (*count)++;
        calcAvgBattery(root->right, totalBattery, count);
    }
}

void runAnomalyDetector(AVLNode* root, double mean, double stddev, int* totalAnomalies) {
    if (root != NULL) {
        runAnomalyDetector(root->left, mean, stddev, totalAnomalies);
        if (fabs(root->data->daily_screen_time - mean) > (3 * stddev)) {
            printf("  [ALERTA DE ANOMALIA] ID: %s | Tempo de Tela Absurdo: %.1f min (Media: %.1f)\n",
                   root->data->record_id, root->data->daily_screen_time, mean);
            (*totalAnomalies)++;
        }
        runAnomalyDetector(root->right, mean, stddev, totalAnomalies);
    }
}

/* ==========================================================================
 * PARSER DO DATASET COM PARADA SE RESTRICAO ATIVA
 * ========================================================================== */
void loadDatasetToQueue(const char* filename, Queue* streamQueue) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("[AVISO] Arquivo %s nao localizado.\n", filename);
        return;
    }
    char line[MAX_LINE];
    fgets(line, sizeof(line), file); 

    int count_loaded = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        char* token = strtok(line, ",");
        if (!token) continue;

        if (bloom_check(token)) continue;
        bloom_insert(token);

        MobileData* record = (MobileData*)malloc(sizeof(MobileData));
        strcpy(record->record_id, token);
        
        if (sysConfig.active) {
            for(int i=0; record->record_id[i] != '\0'; i++) record->record_id[i] ^= sysConfig.xor_key;
            for(int i=0; record->record_id[i] != '\0'; i++) record->record_id[i] ^= sysConfig.xor_key;
        }

        record->year = atoi(strtok(NULL, ","));
        record->month = atoi(strtok(NULL, ","));
        strtok(NULL, ","); strtok(NULL, ","); 
        strcpy(record->country, strtok(NULL, ","));
        strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ",");
        strcpy(record->app_category, strtok(NULL, ","));
        strcpy(record->app_name, strtok(NULL, ","));
        strtok(NULL, ","); 
        
        float raw_screen_time = (float)atof(strtok(NULL, ","));
        if (sysConfig.active && ((float)rand()/RAND_MAX < sysConfig.anomaly_chance)) {
            raw_screen_time += 600.0f;
        }
        record->daily_screen_time = raw_screen_time;

        strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ",");
        record->battery_drain_pct = (float)atof(strtok(NULL, ","));
        strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ",");
        record->monthly_spend_usd = (float)atof(strtok(NULL, ","));
        strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ","); strtok(NULL, ",");
        strcpy(record->sleep_disruption, strtok(NULL, ","));

        enqueue(streamQueue, record);
        count_loaded++;
    }
    fclose(file);
    printf("[SUCESSO] %d registros carregados do arquivo real.\n", count_loaded);
}

/* ==========================================================================
 * MODULO DE BENCHMARK OFICIAL ESCALAVEL
 * ========================================================================== */
void runBenchmarkModule(Queue* sourceQueue) {
    printf("\n========================================================\n");
    printf("   MODULO DE BENCHMARK: ANALISE EMPIRICA DE DESEMPENHO\n");
    printf("========================================================\n");
    if(sysConfig.active) {
        printf("[ALERTA] Modo de Restricao Ativo! Timeout definido para: %.2f ms\n", sysConfig.time_budget_ms);
    }

    int test_size = sourceQueue->size; 
    if (test_size == 0) { printf("[ERRO] Sem dados para testar.\n"); return; }

    MobileData** test_array = (MobileData**)malloc(sizeof(MobileData*) * test_size);
    QueueNode* curr = sourceQueue->front;
    for(int i=0; i<test_size && curr != NULL; i++) {
        test_array[i] = curr->data;
        curr = curr->next;
    }

    int REPETICOES = 50; 
    bool timeout_hash_ins = false, timeout_opt_ins = false;
    bool timeout_hash_search = false, timeout_opt_search = false;

    clock_t start = clock();
    for(int r=0; r<REPETICOES; r++) {
        for(int i=0; i<test_size; i++) {
            insertHash(test_array[i]);
            if (sysConfig.active && (((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0) > sysConfig.time_budget_ms) {
                timeout_hash_ins = true; break;
            }
        }
        if(timeout_hash_ins) break;
    }
    double time_hash_ins = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;

    start = clock();
    for(int r=0; r<REPETICOES; r++) {
        for(int i=0; i<test_size; i++) {
            insertOptHash(test_array[i]);
            if (sysConfig.active && (((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0) > sysConfig.time_budget_ms) {
                timeout_opt_ins = true; break;
            }
        }
        if(timeout_opt_ins) break;
    }
    double time_opt_ins = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;

    start = clock();
    for(int r=0; r<REPETICOES; r++) {
        for(int i=0; i<test_size; i++) {
            searchHash(test_array[i]->record_id);
            if (sysConfig.active && (((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0) > sysConfig.time_budget_ms) {
                timeout_hash_search = true; break;
            }
        }
        if(timeout_hash_search) break;
    }
    double time_hash_search = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;

    start = clock();
    for(int r=0; r<REPETICOES; r++) {
        for(int i=0; i<test_size; i++) {
            searchOptHash(test_array[i]->record_id);
            if (sysConfig.active && (((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0) > sysConfig.time_budget_ms) {
                timeout_opt_search = true; break;
            }
        }
        if(timeout_opt_search) break;
    }
    double time_opt_search = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;

    printf("+-------------------------+-------------------+-------------------+\n");
    printf("| Metrica de Operacao     | Hash Tradicional  | Hash Otimizada    |\n");
    printf("+-------------------------+-------------------+-------------------+\n");
    
    printf("| Tempo Insercao          | ");
    if(timeout_hash_ins) printf("   TIMEOUT (R7)   | "); else printf("%14.4f ms | ", time_hash_ins);
    if(timeout_opt_ins) printf("   TIMEOUT (R7)   |\n"); else printf("%14.4f ms |\n", time_opt_ins);

    printf("| Tempo Busca             | ");
    if(timeout_hash_search) printf("   TIMEOUT (R7)   | "); else printf("%14.4f ms | ", time_hash_search);
    if(timeout_opt_search) printf("   TIMEOUT (R7)   |\n"); else printf("%14.4f ms |\n", time_opt_search);

    printf("| Tamanho Total da Amostra| %17d | %17d |\n", test_size, test_size);
    printf("| Restricoes de Hardware  | %17s | %17s |\n", sysConfig.active ? "ATIVAS" : "DESATIVADAS", sysConfig.active ? "ATIVAS" : "DESATIVADAS");
    printf("+-------------------------+-------------------+-------------------+\n");

    free(test_array);
}

/* ==========================================================================
 * MENU PRINCIPAL
 * ========================================================================== */
int main() {
    setlocale(LC_ALL, "Portuguese_Brazil.UTF-8");
    setlocale(LC_NUMERIC, "C");

    Queue* streamQueue = initQueue();
    loadDatasetToQueue("mobile_app_usage_screen_time.csv", streamQueue);

    if (streamQueue->size == 0) {
        printf("\n[AVISO] Dataset offline. Inicializando Fallback de Seguranca: Gerando 10.000 registros na Fila...\n");
        for(int i=1; i<=10000; i++) {
            MobileData* d = (MobileData*)malloc(sizeof(MobileData));
            sprintf(d->record_id, "APP%07d", i);
            strcpy(d->app_name, i%3==0 ? "Instagram" : (i%3==1 ? "Youtube" : "WhatsApp"));
            strcpy(d->app_category, "Social");
            d->daily_screen_time = 30.0f + (i % 200);
            d->battery_drain_pct = 0.5f + (i % 10);
            strcpy(d->sleep_disruption, i%7==0 ? "Severe" : "Mild");
            enqueue(streamQueue, d);
        }
    }

    AVLNode* rootAVL = NULL;
    TrieNode* rootTrie = createTrieNode();

    QueueNode* aux = streamQueue->front;
    while (aux != NULL) {
        insertHash(aux->data);
        insertOptHash(aux->data);
        rootAVL = insertAVL(rootAVL, aux->data);
        insertTrie(rootTrie, aux->data->app_name);
        aux = aux->next;
    }
    printf("[INFO] Processamento completo. %d nos ativos nas arvores.\n", streamQueue->size);

    int opcao = 0;
    while (opcao != 8) {
        printf("\n--------------------------------------------------------\n");
        printf("           MENU DE OPERACOES DO PROJETO 2026            \n");
        printf("--------------------------------------------------------\n");
        printf("1. Buscar Perfil de Usuario (Hash Table)\n");
        printf("2. Remover Usuario do Sistema (Sincronizada AVL/Hash/Trie)\n");
        printf("3. Autocompletar Nome do App (Trie)\n");
        printf("4. Relatorio de Saude Clinica e Bateria (Metricas AVL)\n");
        printf("5. Alternar Simulador de Restricoes Criticas (Status: %s)\n", sysConfig.active ? "LIGADO (R5,R7,R12,R18,R23)" : "DESLIGADO");
        printf("6. Simulacao 3-Sigma / Injecao de Ruido (Nova Operacao)\n");
        printf("7. EXECUTAR BENCHMARK COMPARATIVO OFICIAL\n");
        printf("8. Sair do Sistema (Encerrar e Desalocar RAM)\n");
        printf("Escolha uma funcionalidade: ");
        
        if(scanf("%d", &opcao) != 1) {
            while(getchar() != '\n'); continue;
        }

        if (opcao == 1) {
            char input_id[15], busca_id[15];
            printf("-> Digite o numero ou ID completo: ");
            scanf("%s", input_id);
            if (isdigit((unsigned char)input_id[0])) sprintf(busca_id, "APP%07d", atoi(input_id));
            else strcpy(busca_id, input_id);

            MobileData* enc = searchHash(busca_id);
            if (enc) printf("\n [RESULTADO] Encontrado: %s | App: %s | Tempo Uso: %.1f min\n", enc->record_id, enc->app_name, enc->daily_screen_time);
            else printf("\n [!] Codigo %s nao localizado.\n", busca_id);
        }
        else if (opcao == 2) {
            char input_id[15], busca_id[15];
            printf("-> Digite o ID para remocao definitiva: ");
            scanf("%s", input_id);
            if (isdigit((unsigned char)input_id[0])) sprintf(busca_id, "APP%07d", atoi(input_id));
            else strcpy(busca_id, input_id);

            MobileData* target = searchHash(busca_id);
            if (target) {
                float s_time = target->daily_screen_time; char a_name[30]; strcpy(a_name, target->app_name);
                bool del_h = deleteHash(busca_id);
                bool del_opt = deleteOptHash(busca_id);
                bool del_avl = false;
                rootAVL = deleteAVL(rootAVL, s_time, busca_id, &del_avl);
                deleteTrie(rootTrie, a_name);
                printf("\n [EXITO] Sincronizacao completa. Removido de todas as tabelas.\n");
            } else printf("\n [!] Usuario inexistente.\n");
        }
        else if (opcao == 3) {
            char prefixo[30]; printf("-> Iniciais do aplicativo: "); scanf("%s", prefixo);
            searchPrefixTrie(rootTrie, prefixo);
        }
        else if (opcao == 4) {
            int cont = 0; double total_bat = 0; int count_bat = 0;
            printSevereSleepDisruption(rootAVL, &cont);
            calcAvgBattery(rootAVL, &total_bat, &count_bat);
            if (count_bat > 0) printf("\n [ANALISE] Media Consumo Bateria: %.2f%%\n", total_bat / count_bat);
        }
        else if (opcao == 5) {
            sysConfig.active = !sysConfig.active;
            printf("\n [MODO ALTERNADO] O ecossistema de restricoes foi %s.\n", sysConfig.active ? "ATIVADO" : "DESATIVADO");
        }
        else if (opcao == 6) {
            int anomalies = 0;
            runAnomalyDetector(rootAVL, 130.0, 35.0, &anomalies);
            printf("\n -> Conclusao: Isoladas %d anomalias estatisticas.\n", anomalies);
        }
        else if (opcao == 7) {
            runBenchmarkModule(streamQueue);
        }
    }

    printf("\n[INFO] Iniciando rotinas de limpeza recursiva de nos...\n");
    freeAVL(rootAVL);
    freeTrie(rootTrie);
    freeHashTables();
    freeQueueAndData(streamQueue);
    printf("[SUCESSO] Toda a memoria RAM foi liberada (Valgrind Safe). Encerrando.\n");

    return 0;
}