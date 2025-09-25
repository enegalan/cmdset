#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <json-c/json.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

#define MAX_PRESETS 100
#define MAX_NAME_LEN 50
#define MAX_COMMAND_LEN 500
#define PRESET_FILE ".cmdset_presets"
#define JSON_LINE_BUFFER 1024
#define ENCRYPTED_COMMAND_LEN (MAX_COMMAND_LEN * 2)
#define SALT_LEN 16
#define IV_LEN 16
#define KEY_LEN 32
#define SESSION_TIMEOUT 300

typedef struct {
    char name[MAX_NAME_LEN];
    char command[MAX_COMMAND_LEN];
    int active;
    int encrypt;
    time_t created_at;
    time_t last_used;
    int use_count;
} Preset;

typedef struct {
    Preset presets[MAX_PRESETS];
    int count;
} PresetManager;

static char session_password[256] = {0};
static time_t session_start_time = 0;
static int session_active = 0;

void print_usage(const char *program_name);
int parse_arguments(int argc, char *argv[], PresetManager *manager);
int add_preset(PresetManager *manager, const char *name, const char *command, int encrypt);
int remove_preset(PresetManager *manager, const char *name);
int list_presets(PresetManager *manager);
int execute_preset(PresetManager *manager, const char *name, const char *additional_args);
Preset* find_preset(PresetManager *manager, const char *name);
int save_presets_json(PresetManager *manager);
int load_presets_json(PresetManager *manager);
int encrypt_command(const char *plaintext, char *encrypted);
int decrypt_command(const char *encrypted, char *plaintext);
int get_master_password(char *password, int max_len);
int derive_key(const char *password, const unsigned char *salt, unsigned char *key);
int get_session_password(char *password, int max_len);
int is_session_valid(void);
void clear_session(void);
void show_session_status(void);

int main(int argc, char *argv[]) {
    PresetManager manager;
    manager.count = 0;
    load_presets_json(&manager);
    int result = parse_arguments(argc, argv, &manager);
    if (result == 0) save_presets_json(&manager); // Save presets if any changes were made
    return result;
}

void print_usage(const char *program_name) {
    printf("Usage: %s [options...]\n", program_name);
    printf(" %s add <name> <command>       Add a new preset\n", program_name);
    printf(" %s a <name> <command>          Add a new preset (short)\n", program_name);
    printf(" %s add --encrypt <name> <command>     Add an encrypted preset\n", program_name);
    printf(" %s a -e <name> <command>        Add an encrypted preset (short)\n", program_name);
    printf(" %s remove <name>              Remove a preset\n", program_name);
    printf(" %s rm <name>                   Remove a preset (short)\n", program_name);
    printf(" %s list                       List all presets\n", program_name);
    printf(" %s ls                         List all presets (short)\n", program_name);
    printf(" %s exec <name> [args...]      Execute a preset with optional arguments\n", program_name);
    printf(" %s e <name> [args...]          Execute a preset with optional arguments (short)\n", program_name);
    printf(" %s run <name> [args...]        Execute a preset with optional arguments (short)\n", program_name);
    printf(" %s help                       Show this help message\n", program_name);
    printf(" %s h                          Show this help message (short)\n", program_name);
    printf(" %s clear-session              Clear cached password session\n", program_name);
    printf(" %s cs                         Clear cached password session (short)\n", program_name);
    printf(" %s status                     Show session status\n", program_name);
    printf(" %s s                          Show session status (short)\n", program_name);
}

int parse_arguments(int argc, char *argv[], PresetManager *manager) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "clear-session") == 0 || strcmp(argv[1], "cs") == 0) {
        clear_session();
        return 0;
    }
    if (strcmp(argv[1], "status") == 0 || strcmp(argv[1], "s") == 0) {
        show_session_status();
        return 0;
    }
    if (strcmp(argv[1], "add") == 0 || strcmp(argv[1], "a") == 0) {
        if (argc < 4) {
            printf("Error: add command requires name and command arguments\n");
            printf("Usage: %s add <name> <command>\n", argv[0]);
            return 1;
        }
        int encrypt = 0;
        int name_index = 2;
        int command_index = 3;
        if (argc >= 5 && (strcmp(argv[2], "-e") == 0 || strcmp(argv[2], "--encrypt") == 0)) {
            encrypt = 1;
            name_index = 3;
            command_index = 4;
        }
        if (encrypt && argc < 5) {
            printf("Error: encrypted add command requires name and command arguments\n");
            printf("Usage: %s add --encrypt <name> <command>\n", argv[0]);
            printf("Or: %s add -e <name> <command>\n", argv[0]);
            return 1;
        }
        return add_preset(manager, argv[name_index], argv[command_index], encrypt);
    }
    if (strcmp(argv[1], "remove") == 0 || strcmp(argv[1], "rm") == 0) {
        if (argc < 3) {
            printf("Error: remove command requires name argument\n");
            printf("Usage: %s remove <name>\n", argv[0]);
            return 1;
        }
        return remove_preset(manager, argv[2]);
    }
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) return list_presets(manager);
    if (strcmp(argv[1], "exec") == 0 || strcmp(argv[1], "e") == 0 || strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            printf("Error: exec command requires name argument\n");
            printf("Usage: %s exec <name> [additional_args...]\n", argv[0]);
            return 1;
        }
        char *additional_args = NULL;
        if (argc > 3) {
            int total_len = 0;
            for (int i = 3; i < argc; i++) {
                total_len += strlen(argv[i]) + 1;
            }
            additional_args = malloc(total_len);
            if (additional_args == NULL) {
                printf("Error: Memory allocation failed\n");
                return 1;
            }
            additional_args[0] = '\0';
            for (int i = 3; i < argc; i++) {
                if (i > 3) strcat(additional_args, " ");
                strcat(additional_args, argv[i]);
            }
        }
        int result = execute_preset(manager, argv[2], additional_args);
        if (additional_args) free(additional_args);
        return result;
    }
    printf("Error: Unknown command '%s'\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}

int add_preset(PresetManager *manager, const char *name, const char *command, int encrypt) {
    if (manager->count >= MAX_PRESETS) {
        printf("Error: Maximum number of presets reached (%d)\n", MAX_PRESETS);
        return 1;
    }
    if (strlen(name) >= MAX_NAME_LEN) {
        printf("Error: Preset name too long (max %d characters)\n", MAX_NAME_LEN - 1);
        return 1;
    }
    if (strlen(command) >= MAX_COMMAND_LEN) {
        printf("Error: Command too long (max %d characters)\n", MAX_COMMAND_LEN - 1);
        return 1;
    }
    Preset *existing = find_preset(manager, name);
    if (existing != NULL) {
        printf("Error: Preset '%s' already exists\n", name);
        return 1;
    }
    strcpy(manager->presets[manager->count].name, name);
    manager->presets[manager->count].encrypt = encrypt;
    manager->presets[manager->count].active = 1;
    manager->presets[manager->count].created_at = time(NULL);
    manager->presets[manager->count].last_used = 0;
    manager->presets[manager->count].use_count = 0;
    if (encrypt) {
        char encrypted_command[ENCRYPTED_COMMAND_LEN];
        if (encrypt_command(command, encrypted_command) != 0) {
            printf("Error: Failed to encrypt command\n");
            return 1;
        }
        strcpy(manager->presets[manager->count].command, encrypted_command);
        printf("Encrypted preset '%s' added successfully\n", name);
    } else {
        strcpy(manager->presets[manager->count].command, command);
        printf("Preset '%s' added successfully\n", name);
    }
    manager->count++;
    return 0;
}

int remove_preset(PresetManager *manager, const char *name) {
    Preset *preset = find_preset(manager, name);
    if (preset == NULL) {
        printf("Error: Preset '%s' not found\n", name);
        return 1;
    }
    preset->active = 0;
    printf("Preset '%s' removed successfully\n", name);
    return 0;
}

int list_presets(PresetManager *manager) {
    int active_count = 0;
    printf("\nPresets:\n");
    printf("--------\n");
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            if (manager->presets[i].encrypt) {
                printf("%d. %s: [ENCRYPTED] (command hidden)\n", active_count + 1, 
                    manager->presets[i].name);
            } else {
                printf("%d. %s: %s\n", active_count + 1, 
                    manager->presets[i].name, 
                    manager->presets[i].command);
            }
            active_count++;
        }
    }
    if (active_count == 0) printf("No presets found\n");
    else printf("\nTotal: %d preset(s)\n", active_count);
    return 0;
}

int execute_preset(PresetManager *manager, const char *name, const char *additional_args) {
    Preset *preset = find_preset(manager, name);
    if (preset == NULL) {
        printf("Error: Preset '%s' not found\n", name);
        return 1;
    }
    preset->last_used = time(NULL);
    preset->use_count++;
    char command_to_execute[MAX_COMMAND_LEN];
    if (preset->encrypt) {
        if (decrypt_command(preset->command, command_to_execute) != 0) {
            printf("Error: Failed to decrypt command\n");
            return 1;
        }
        printf("Executing encrypted command: [HIDDEN]");
    } else {
        strcpy(command_to_execute, preset->command);
        printf("Executing: %s", command_to_execute);
    }
    if (additional_args != NULL && strlen(additional_args) > 0) {
        if (strstr(command_to_execute, " -e") != NULL) {
            char *dash_e_pos = strstr(command_to_execute, " -e");
            if (dash_e_pos != NULL) {
                char temp_command[MAX_COMMAND_LEN];
                int base_len = dash_e_pos - command_to_execute;
                strncpy(temp_command, command_to_execute, base_len);
                temp_command[base_len] = '\0';
                strcat(temp_command, " -e \"");
                strcat(temp_command, additional_args);
                strcat(temp_command, "\"");
                strcpy(command_to_execute, temp_command);
            }
        } else {
            strcat(command_to_execute, " ");
            strcat(command_to_execute, additional_args);
        }
        if (!preset->encrypt) printf(" %s", additional_args);
    }
    printf("\n");
    printf("----------------------------------------\n");
    int result = system(command_to_execute);
    if (result != 0) {
        printf("Command exited with status %d\n", result);
    }
    if (preset->encrypt) memset(command_to_execute, 0, MAX_COMMAND_LEN); // Clear sensitive data from memory
    return result;
}

int save_presets_json(PresetManager *manager) {
    json_object *root = json_object_new_object();
    if (root == NULL) {
        printf("Error: Could not create JSON object\n");
        return 1;
    }
    json_object *version = json_object_new_string("2.0");
    json_object_object_add(root, "version", version);
    json_object *presets_array = json_object_new_array();
    if (presets_array == NULL) {
        json_object_put(root);
        printf("Error: Could not create presets array\n");
        return 1;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            json_object *preset = json_object_new_object();
            if (preset == NULL) {
                json_object_put(root);
                return 1;
            }
            json_object_object_add(preset, "name", json_object_new_string(manager->presets[i].name));
            json_object_object_add(preset, "command", json_object_new_string(manager->presets[i].command));
            json_object_object_add(preset, "encrypt", json_object_new_boolean(manager->presets[i].encrypt));
            json_object_object_add(preset, "created_at", json_object_new_int64(manager->presets[i].created_at));
            json_object_object_add(preset, "last_used", json_object_new_int64(manager->presets[i].last_used));
            json_object_object_add(preset, "use_count", json_object_new_int(manager->presets[i].use_count));
            json_object_array_add(presets_array, preset);
        }
    }
    json_object_object_add(root, "presets", presets_array);
    const char *json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    if (json_string == NULL) {
        json_object_put(root);
        printf("Error: Could not generate JSON string\n");
        return 1;
    }
    FILE *file = fopen(PRESET_FILE, "w");
    if (file == NULL) {
        json_object_put(root);
        printf("Error: Could not save presets to file: %s\n", strerror(errno));
        return 1;
    }
    fprintf(file, "%s", json_string);
    fclose(file);
    json_object_put(root);
    return 0;
}

int load_presets_json(PresetManager *manager) {
    FILE *file = fopen(PRESET_FILE, "r");
    if (file == NULL) return 1; // File doesn't exist
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(file);
        return 1;
    }
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    json_object *root = json_tokener_parse(content);
    free(content);
    if (root == NULL) {
        printf("Error: Could not parse JSON file\n");
        return 1;
    }
    manager->count = 0;
    json_object *presets_array;
    if (json_object_object_get_ex(root, "presets", &presets_array) && json_object_is_type(presets_array, json_type_array)) {
        int array_size = json_object_array_length(presets_array);
        for (int i = 0; i < array_size && manager->count < MAX_PRESETS; i++) {
            json_object *preset = json_object_array_get_idx(presets_array, i);
            if (preset != NULL) {
                manager->presets[manager->count].active = 1;
                json_object *name_item;
                if (json_object_object_get_ex(preset, "name", &name_item) && json_object_is_type(name_item, json_type_string)) {
                    const char *name_str = json_object_get_string(name_item);
                    strncpy(manager->presets[manager->count].name, name_str, MAX_NAME_LEN - 1);
                    manager->presets[manager->count].name[MAX_NAME_LEN - 1] = '\0';
                }
                json_object *command_item;
                if (json_object_object_get_ex(preset, "command", &command_item) && json_object_is_type(command_item, json_type_string)) {
                    const char *command_str = json_object_get_string(command_item);
                    strncpy(manager->presets[manager->count].command, command_str, MAX_COMMAND_LEN - 1);
                    manager->presets[manager->count].command[MAX_COMMAND_LEN - 1] = '\0';
                }
                json_object *encrypt_item;
                if (json_object_object_get_ex(preset, "encrypt", &encrypt_item)) manager->presets[manager->count].encrypt = json_object_get_boolean(encrypt_item) ? 1 : 0;
                else manager->presets[manager->count].encrypt = 0;
                json_object *created_item;
                if (json_object_object_get_ex(preset, "created_at", &created_item) && json_object_is_type(created_item, json_type_int)) manager->presets[manager->count].created_at = (time_t)json_object_get_int64(created_item);
                else manager->presets[manager->count].created_at = time(NULL);
                json_object *last_used_item;
                if (json_object_object_get_ex(preset, "last_used", &last_used_item) && json_object_is_type(last_used_item, json_type_int)) manager->presets[manager->count].last_used = (time_t)json_object_get_int64(last_used_item);
                else manager->presets[manager->count].last_used = 0;
                json_object *use_count_item;
                if (json_object_object_get_ex(preset, "use_count", &use_count_item) && json_object_is_type(use_count_item, json_type_int)) manager->presets[manager->count].use_count = json_object_get_int(use_count_item);
                else manager->presets[manager->count].use_count = 0;
                manager->count++;
            }
        }
    }
    
    return 0;
}

Preset* find_preset(PresetManager *manager, const char *name) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) return &manager->presets[i];
    }
    return NULL;
}

int get_master_password(char *password, int max_len) {
    printf("Enter master password for encryption: ");
    fflush(stdout);
    #ifdef _WIN32
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
    #else
        system("stty -echo");
    #endif
    if (fgets(password, max_len, stdin) == NULL) {
        #ifdef _WIN32
            SetConsoleMode(hStdin, mode);
        #else
            system("stty echo");
        #endif
        return 1;
    }
    #ifdef _WIN32
        SetConsoleMode(hStdin, mode);
    #else
        system("stty echo");
    #endif
    printf("\n");
    password[strcspn(password, "\n")] = '\0';
    return 0;
}

int get_session_password(char *password, int max_len) {
    if (is_session_valid()) {
        strncpy(password, session_password, max_len - 1);
        password[max_len - 1] = '\0';
        return 0;
    }
    if (get_master_password(password, max_len) != 0) return 1;
    strncpy(session_password, password, sizeof(session_password) - 1);
    session_password[sizeof(session_password) - 1] = '\0';
    session_start_time = time(NULL);
    session_active = 1;
    printf("Password cached for %d minutes. Use 'cmdset clear-session' to clear.\n", SESSION_TIMEOUT / 60);
    return 0;
}

int is_session_valid(void) {
    if (!session_active) return 0;
    time_t current_time = time(NULL);
    if (current_time - session_start_time > SESSION_TIMEOUT) {
        clear_session();
        return 0;
    }
    return 1;
}

void clear_session(void) {
    memset(session_password, 0, sizeof(session_password));
    session_start_time = 0;
    session_active = 0;
    printf("Session cleared.\n");
}

void show_session_status(void) {
    if (is_session_valid()) {
        time_t remaining = SESSION_TIMEOUT - (time(NULL) - session_start_time);
        printf("Session active: %ld minutes remaining\n", remaining / 60);
    } else printf("No active session\n");
}

int derive_key(const char *password, const unsigned char *salt, unsigned char *key) {
    if (PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_LEN, 10000, EVP_sha256(), KEY_LEN, key) != 1) {
        return 1;
    }
    return 0;
}

int encrypt_command(const char *plaintext, char *encrypted) {
    char master_password[256];
    if (get_session_password(master_password, sizeof(master_password)) != 0) return 1;
    unsigned char salt[SALT_LEN];
    unsigned char iv[IV_LEN];
    if (RAND_bytes(salt, SALT_LEN) != 1 || RAND_bytes(iv, IV_LEN) != 1) {
        memset(master_password, 0, sizeof(master_password));
        return 1;
    }
    unsigned char key[KEY_LEN];
    if (derive_key(master_password, salt, key) != 0) {
        memset(master_password, 0, sizeof(master_password));
        return 1;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    int len;
    int ciphertext_len;
    unsigned char ciphertext[MAX_COMMAND_LEN + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, (unsigned char*)plaintext, strlen(plaintext)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    unsigned char combined[SALT_LEN + IV_LEN + MAX_COMMAND_LEN + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
    memcpy(combined, salt, SALT_LEN);
    memcpy(combined + SALT_LEN, iv, IV_LEN);
    memcpy(combined + SALT_LEN + IV_LEN, ciphertext, ciphertext_len);
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int combined_len = SALT_LEN + IV_LEN + ciphertext_len;
    int encoded_len = 0;
    for (int i = 0; i < combined_len; i += 3) {
        unsigned char b1 = combined[i];
        unsigned char b2 = (i + 1 < combined_len) ? combined[i + 1] : 0;
        unsigned char b3 = (i + 2 < combined_len) ? combined[i + 2] : 0;
        unsigned int combined_bytes = (b1 << 16) | (b2 << 8) | b3;
        encrypted[encoded_len++] = base64_chars[(combined_bytes >> 18) & 0x3F];
        encrypted[encoded_len++] = base64_chars[(combined_bytes >> 12) & 0x3F];
        encrypted[encoded_len++] = (i + 1 < combined_len) ? base64_chars[(combined_bytes >> 6) & 0x3F] : '=';
        encrypted[encoded_len++] = (i + 2 < combined_len) ? base64_chars[combined_bytes & 0x3F] : '=';
    }
    encrypted[encoded_len] = '\0';
    memset(master_password, 0, sizeof(master_password));
    memset(key, 0, KEY_LEN);
    memset(salt, 0, SALT_LEN);
    memset(iv, 0, IV_LEN);
    memset(ciphertext, 0, sizeof(ciphertext));
    memset(combined, 0, sizeof(combined));
    return 0;
}

int decrypt_command(const char *encrypted, char *plaintext) {
    char master_password[256];
    if (get_session_password(master_password, sizeof(master_password)) != 0) return 1;
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int encrypted_len = strlen(encrypted);
    int decoded_len = (encrypted_len * 3) / 4;
    unsigned char *decoded = malloc(decoded_len);
    if (decoded == NULL) {
        memset(master_password, 0, sizeof(master_password));
        return 1;
    }
    int decoded_index = 0;
    for (int i = 0; i < encrypted_len; i += 4) {
        unsigned char c1 = strchr(base64_chars, encrypted[i]) - base64_chars;
        unsigned char c2 = strchr(base64_chars, encrypted[i + 1]) - base64_chars;
        unsigned char c3 = (encrypted[i + 2] == '=') ? 0 : strchr(base64_chars, encrypted[i + 2]) - base64_chars;
        unsigned char c4 = (encrypted[i + 3] == '=') ? 0 : strchr(base64_chars, encrypted[i + 3]) - base64_chars;
        unsigned int combined = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
        decoded[decoded_index++] = (combined >> 16) & 0xFF;
        if (i + 2 < encrypted_len && encrypted[i + 2] != '=') decoded[decoded_index++] = (combined >> 8) & 0xFF;
        if (i + 3 < encrypted_len && encrypted[i + 3] != '=') decoded[decoded_index++] = combined & 0xFF;
    }
    unsigned char *salt = decoded;
    unsigned char *iv = decoded + SALT_LEN;
    unsigned char *ciphertext = decoded + SALT_LEN + IV_LEN;
    int ciphertext_len = decoded_len - SALT_LEN - IV_LEN;
    unsigned char key[KEY_LEN];
    if (derive_key(master_password, salt, key) != 0) {
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        return 1;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    int len;
    int plaintext_len;
    if (EVP_DecryptUpdate(ctx, (unsigned char*)plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    plaintext_len = len;
    if (EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    plaintext_len += len;
    plaintext[plaintext_len] = '\0';
    EVP_CIPHER_CTX_free(ctx);
    free(decoded);
    memset(master_password, 0, sizeof(master_password));
    memset(key, 0, KEY_LEN);
    return 0;
}
