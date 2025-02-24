#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// Includiamo TinyUSB se necessario (o usa stdio_usb_connected() per Pico2)
#include "tusb.h"

// External functions (for logging and prediction)
extern void setLogFunction(void (*log_function)(uint8_t *string));
extern void predict(float* input, uint32_t* input_shape, uint32_t shape_len, float** result);

static void log_fn(uint8_t *string) {
    printf("%s\n", string);
}

#define MAX_SEQ_LENGTH     15
#define MAX_INPUT_LENGTH   256

// Simple dictionary mapping words to indices
typedef struct {
    const char *word;
    uint32_t idx;
} TokenMapping;

TokenMapping dictionary[] = {
    {"a", 1},
    {"acquisto", 2},
    {"adoro", 3},
    {"affidabile", 4},
    {"aggiornamento", 5},
    {"alla", 6},
    {"alle", 7},
    {"annoiati", 8},
    {"app", 9},
    {"apprezzato", 10},
    {"aspettavo", 11},
    {"assaggiato", 12},
    {"avvincente", 13},
    {"bambini", 14},
    {"banale", 15},
    {"bellissimo", 16},
    {"ben", 17},
    {"blocca", 18},
    {"bruttissimo", 19},
    {"buono", 20},
    {"buttati", 21},
    {"cancellata", 22},
    {"capolavoro", 23},
    {"che", 24},
    {"chiare", 25},
    {"ci", 26},
    {"cibo", 27},
    {"cinematografia", 28},
    {"cinematografico", 29},
    {"città", 30},
    {"classica", 31},
    {"clienti", 32},
    {"coinvolgente", 33},
    {"commovente", 34},
    {"completamente", 35},
    {"complimenti", 36},
    {"concerto", 37},
    {"confusa", 38},
    {"confuse", 39},
    {"consiglio", 40},
    {"contento", 41},
    {"cuffie", 42},
    {"cui", 43},
    {"da", 44},
    {"dal", 45},
    {"dallinizio", 46},
    {"davvero", 47},
    {"dei", 48},
    {"del", 49},
    {"delizioso", 50},
    {"della", 51},
    {"dellattore", 52},
    {"delle", 53},
    {"deludente", 54},
    {"delusione", 55},
    {"deluso", 56},
    {"dettagliate", 57},
    {"di", 58},
    {"dimenticare", 59},
    {"disastro", 60},
    {"disgustoso", 61},
    {"disponibile", 62},
    {"disponibili", 63},
    {"dispositivo", 64},
    {"divertiti", 65},
    {"e", 66},
    {"eccellente", 67},
    {"eccezionale", 68},
    {"ed", 69},
    {"emozionante", 70},
    {"emozionato", 71},
    {"era", 72},
    {"erano", 73},
    {"esperienza", 74},
    {"estremamente", 75},
    {"evitare", 76},
    {"fantastica", 77},
    {"fantastico", 78},
    {"fastidiosa", 79},
    {"fatto", 80},
    {"felice", 81},
    {"film", 82},
    {"fine", 83},
    {"finirei", 84},
    {"fino", 85},
    {"fotografia", 86},
    {"fragile", 87},
    {"freddo", 88},
    {"gelato", 89},
    {"gentile", 90},
    {"giro", 91},
    {"ha", 92},
    {"ho", 93},
    {"hotel", 94},
    {"i", 95},
    {"il", 96},
    {"immangiabile", 97},
    {"impeccabile", 98},
    {"in", 99},
    {"inaspettata", 100},
    {"incredibile", 101},
    {"incredibilmente", 102},
    {"incubo", 103},
    {"indimenticabile", 104},
    {"insoddisfatto", 105},
    {"inutile", 106},
    {"inutili", 107},
    {"ispirato", 108},
    {"la", 109},
    {"lacrime", 110},
    {"laptop", 111},
    {"lavoro", 112},
    {"le", 113},
    {"lenta", 114},
    {"lento", 115},
    {"lettura", 116},
    {"lho", 117},
    {"libro", 118},
    {"lo", 119},
    {"lora", 120},
    {"ma", 121},
    {"magica", 122},
    {"mai", 123},
    {"male", 124},
    {"materiali", 125},
    {"me", 126},
    {"meglio", 127},
    {"meravigliosa", 128},
    {"mi", 129},
    {"miglior", 130},
    {"mio", 131},
    {"moltissimo", 132},
    {"molto", 133},
    {"momento", 134},
    {"musica", 135},
    {"negativa", 136},
    {"nessuno", 137},
    {"noiosa", 138},
    {"noioso", 139},
    {"non", 140},
    {"nuovo", 141},
    {"odiato", 142},
    {"odio", 143},
    {"odore", 144},
    {"oggi", 145},
    {"ogni", 146},
    {"online", 147},
    {"organizzata", 148},
    {"orribile", 149},
    {"ottima", 150},
    {"ottimo", 151},
    {"peggior", 152},
    {"peggiorato", 153},
    {"per", 154},
    {"perfetta", 155},
    {"perfetto", 156},
    {"performance", 157},
    {"personale", 158},
    {"pessima", 159},
    {"pessimo", 160},
    {"piace", 161},
    {"più", 162},
    {"poco", 163},
    {"pop", 164},
    {"positiva", 165},
    {"posti", 166},
    {"posto", 167},
    {"presentato", 168},
    {"preso", 169},
    {"prevedibile", 170},
    {"prodotto", 171},
    {"professionale", 172},
    {"profondamente", 173},
    {"profumo", 174},
    {"qualità", 175},
    {"questa", 176},
    {"questo", 177},
    {"regalo", 178},
    {"rispondono", 179},
    {"ristorante", 180},
    {"scadente", 181},
    {"scelta", 182},
    {"scortese", 183},
    {"scritto", 184},
    {"sempre", 185},
    {"senso", 186},
    {"sentito", 187},
    {"serata", 188},
    {"seria", 189},
    {"serie", 190},
    {"servizio", 191},
    {"sgarbato", 192},
    {"sgradevole", 193},
    {"shopping", 194},
    {"si", 195},
    {"sia", 196},
    {"sicuramente", 197},
    {"singolo", 198},
    {"smartphone", 199},
    {"soddisfatto", 200},
    {"soldi", 201},
    {"sono", 202},
    {"sorpresa", 203},
    {"sorpreso", 204},
    {"speciale", 205},
    {"spettacolare", 206},
    {"spettacolo", 207},
    {"spiegazioni", 208},
    {"sprecati", 209},
    {"squisito", 210},
    {"staff", 211},
    {"stata", 212},
    {"stato", 213},
    {"stupendi", 214},
    {"subito", 215},
    {"suono", 216},
    {"tantissimo", 217},
    {"tecnologia", 218},
    {"terribile", 219},
    {"toccato", 220},
    {"tornarci", 221},
    {"tornerò", 222},
    {"totale", 223},
    {"trama", 224},
    {"triste", 225},
    {"tutti", 226},
    {"tutto", 227},
    {"tv", 228},
    {"un", 229},
    {"una", 230},
    {"unavventura", 231},
    {"unazienda", 232},
    {"unica", 233},
    {"unottima", 234},
    {"uso", 235},
    {"utile", 236},
    {"utilissima", 237},
    {"vedo", 238},
    {"veloce", 239},
    {"vera", 240},
    {"viaggio", 241},
    {"è", 242},
};


const uint32_t dictionary_size = sizeof(dictionary) / sizeof(dictionary[0]);

// Lookup a token in the dictionary; returns 0 if not found.
uint32_t lookup_token(const char *token) {
    for (uint32_t i = 0; i < dictionary_size; i++) {
        if (strcmp(token, dictionary[i].word) == 0) {
            return dictionary[i].idx;
        }
    }
    return 0; // Unknown tokens -> index 0 (padding)
}

int main() {
    stdio_init_all();
    
    // Wait for USB connection using stdio_usb_connected() (works reliably on Pico2).
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    printf("\nSentiment Analysis Demo\n");
    fflush(stdout);
    
    // Set up logging
    setLogFunction(log_fn);
    
#ifdef PICO_DEFAULT_LED_PIN
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
#endif

    // Buffer for serial input
    char input_str[MAX_INPUT_LENGTH] = {0};

    // Main loop: continuously ask for new phrases.
    while (1) {
        printf("\nInserisci una frase: ");
        fflush(stdout);
        
        // Clear input buffer
        memset(input_str, 0, sizeof(input_str));
        if (!fgets(input_str, sizeof(input_str), stdin)) {
            printf("Errore o EOF durante fgets.\n");
            break;
        }
        
        // Remove trailing newline if present.
        size_t len = strlen(input_str);
        if (len > 0 && input_str[len - 1] == '\n') {
            input_str[len - 1] = '\0';
        }
        
        // Tokenize input and build token indices.
        float input_data[MAX_SEQ_LENGTH] = {0};
        uint32_t token_count = 0;
        
        char *token = strtok(input_str, " ");
        while (token != NULL && token_count < MAX_SEQ_LENGTH) {
            uint32_t idx = lookup_token(token);
            input_data[token_count++] = (float) idx;
            token = strtok(NULL, " ");
        }
        
        // Print tokenized indices.
        printf("\nTokenized input (indices): ");
        for (uint32_t i = 0; i < MAX_SEQ_LENGTH; i++) {
            printf("%u ", (uint32_t) input_data[i]);
        }
        printf("\n");
        fflush(stdout);
        
        // Set up input shape [1, MAX_SEQ_LENGTH]
        uint32_t input_shape[] = {1, MAX_SEQ_LENGTH};
        float* result;
        
        printf("Starting prediction...\n");
        fflush(stdout);
#ifdef PICO_DEFAULT_LED_PIN
        gpio_put(LED_PIN, 1);
#endif
        absolute_time_t start_time = get_absolute_time();
        predict(input_data, input_shape, 2, &result);
        absolute_time_t end_time = get_absolute_time();
#ifdef PICO_DEFAULT_LED_PIN
        gpio_put(LED_PIN, 0);
#endif
        
        int64_t diff_us = absolute_time_diff_us(start_time, end_time);
        printf("Prediction took %lld microseconds\n", diff_us);
        printf("Sentiment prediction score: %.6f\n", result[0]);
        if (result[0] >= 0.5f) {
            printf("-> Sentiment positivo\n");
        } else {
            printf("-> Sentiment negativo\n");
        }
        fflush(stdout);
    }
    
    return 0;
}
