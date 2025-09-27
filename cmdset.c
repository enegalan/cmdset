#include "cmdset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <termios.h>
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

static char session_password[256] = {0};
static time_t session_start_time = 0;
static int session_active = 0;
static char session_file[512];
static char current_preset_name[256] = {0};
static char last_error_message[256] = {0};

static int derive_key(const char *password, const unsigned char *salt, unsigned char *key);
static int get_master_password(char *password, int max_len);
static int get_session_password(char *password, int max_len, const char *preset_name);
static int is_session_valid(void);
static void clear_session(void);
static int encrypt_command_internal(const char *plaintext, char *encrypted, const char *preset_name);
static int decrypt_command_internal(const char *encrypted, char *plaintext, const char *preset_name);

#define CMDSET_SUCCESS 0
#define CMDSET_ERROR_MEMORY -1
#define CMDSET_ERROR_FILE -2
#define CMDSET_ERROR_NOT_FOUND -3
#define CMDSET_ERROR_EXISTS -4
#define CMDSET_ERROR_INVALID -5
#define CMDSET_ERROR_ENCRYPTION -6
#define CMDSET_ERROR_JSON -7

static const char* error_messages[] = {
    "Success",
    "Memory allocation error",
    "File operation error",
    "Preset not found",
    "Preset already exists",
    "Invalid parameters",
    "Encryption error",
    "JSON parsing error"
};

const char* cmdset_get_error_message(int error_code) {
    int index = -error_code;
    if (index >= 0 && index < (int)(sizeof(error_messages) / sizeof(error_messages[0]))) return error_messages[index];
    return "Unknown error";
}

int cmdset_init(cmdset_manager_t *manager) {
    if (manager == NULL) {
        strcpy(last_error_message, "Manager is NULL");
        return CMDSET_ERROR_INVALID;
    }
    memset(manager, 0, sizeof(cmdset_manager_t));
    return cmdset_load_presets(manager);
}

int cmdset_add_preset(cmdset_manager_t *manager, const char *name, const char *command, int encrypt) {
    if (manager == NULL || name == NULL || command == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    if (manager->count >= MAX_PRESETS) {
        strcpy(last_error_message, "Maximum number of presets reached");
        return CMDSET_ERROR_MEMORY;
    }
    if (strlen(name) >= MAX_NAME_LEN) {
        strcpy(last_error_message, "Preset name too long");
        return CMDSET_ERROR_INVALID;
    }
    if (strlen(command) >= MAX_COMMAND_LEN) {
        strcpy(last_error_message, "Command too long");
        return CMDSET_ERROR_INVALID;
    }
    cmdset_preset_t *existing = NULL;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            existing = &manager->presets[i];
            break;
        }
    }
    if (existing != NULL) {
        strcpy(last_error_message, "Preset already exists");
        return CMDSET_ERROR_EXISTS;
    }
    cmdset_preset_t *preset = &manager->presets[manager->count];
    strcpy(preset->name, name);
    preset->active = 1;
    preset->encrypt = encrypt;
    preset->created_at = time(NULL);
    preset->last_used = 0;
    preset->use_count = 0;
    if (encrypt) {
        char encrypted_command[ENCRYPTED_COMMAND_LEN];
        if (encrypt_command_internal(command, encrypted_command, name) != 0) {
            strcpy(last_error_message, "Failed to encrypt command");
            return CMDSET_ERROR_ENCRYPTION;
        }
        strcpy(preset->command, encrypted_command);
    } else strcpy(preset->command, command);
    manager->count++;
    return CMDSET_SUCCESS;
}

int cmdset_remove_preset(cmdset_manager_t *manager, const char *name) {
    if (manager == NULL || name == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            manager->presets[i].active = 0;
            return CMDSET_SUCCESS;
        }
    }
    strcpy(last_error_message, "Preset not found");
    return CMDSET_ERROR_NOT_FOUND;
}

int cmdset_execute_preset(cmdset_manager_t *manager, const char *name, const char *additional_args) {
    if (manager == NULL || name == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    cmdset_preset_t *preset = NULL;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            preset = &manager->presets[i];
            break;
        }
    }
    if (preset == NULL) {
        strcpy(last_error_message, "Preset not found");
        return CMDSET_ERROR_NOT_FOUND;
    }
    preset->last_used = time(NULL);
    preset->use_count++;
    char command_to_execute[MAX_COMMAND_LEN];
    if (preset->encrypt) {
        if (decrypt_command_internal(preset->command, command_to_execute, name) != 0) {
            strcpy(last_error_message, "Incorrect password or decryption failed");
            return CMDSET_ERROR_ENCRYPTION;
        }
    } else strcpy(command_to_execute, preset->command);
    if (additional_args != NULL && strlen(additional_args) > 0) {
        strcat(command_to_execute, " ");
        strcat(command_to_execute, additional_args);
    }
    int result = system(command_to_execute);
    if (preset->encrypt) memset(command_to_execute, 0, MAX_COMMAND_LEN);
    return result;
}

int cmdset_list_presets(cmdset_manager_t *manager, char *output, int max_len) {
    if (manager == NULL || output == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    int active_count = 0;
    int offset = 0;
    offset += snprintf(output + offset, max_len - offset, "Presets:\n");
    offset += snprintf(output + offset, max_len - offset, "--------\n");
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            if (manager->presets[i].encrypt) {
                offset += snprintf(output + offset, max_len - offset, 
                    "%d. %s: [ENCRYPTED] (command hidden)\n", 
                    active_count + 1, manager->presets[i].name);
            } else {
                offset += snprintf(output + offset, max_len - offset, 
                    "%d. %s: %s\n", 
                    active_count + 1, manager->presets[i].name, 
                    manager->presets[i].command);
            }
            active_count++;
        }
    }
    if (active_count == 0) offset += snprintf(output + offset, max_len - offset, "No presets found\n");
    else offset += snprintf(output + offset, max_len - offset, "\nTotal: %d preset(s)\n", active_count);
    return CMDSET_SUCCESS;
}

int cmdset_find_preset(cmdset_manager_t *manager, const char *name, cmdset_preset_t *preset) {
    if (manager == NULL || name == NULL || preset == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            *preset = manager->presets[i];
            return CMDSET_SUCCESS;
        }
    }
    strcpy(last_error_message, "Preset not found");
    return CMDSET_ERROR_NOT_FOUND;
}

int cmdset_save_presets(cmdset_manager_t *manager) {
    if (manager == NULL) {
        strcpy(last_error_message, "Manager is NULL");
        return CMDSET_ERROR_INVALID;
    }
    json_object *root = json_object_new_object();
    if (root == NULL) {
        strcpy(last_error_message, "Could not create JSON object");
        return CMDSET_ERROR_JSON;
    }
    json_object *version = json_object_new_string("2.0");
    json_object_object_add(root, "version", version);
    json_object *presets_array = json_object_new_array();
    if (presets_array == NULL) {
        json_object_put(root);
        strcpy(last_error_message, "Could not create presets array");
        return CMDSET_ERROR_JSON;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            json_object *preset = json_object_new_object();
            if (preset == NULL) {
                json_object_put(root);
                strcpy(last_error_message, "Could not create preset object");
                return CMDSET_ERROR_JSON;
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
        strcpy(last_error_message, "Could not generate JSON string");
        return CMDSET_ERROR_JSON;
    }
    FILE *file = fopen(PRESET_FILE, "w");
    if (file == NULL) {
        json_object_put(root);
        snprintf(last_error_message, sizeof(last_error_message), "Could not save presets to file: %s", strerror(errno));
        return CMDSET_ERROR_FILE;
    }
    fprintf(file, "%s", json_string);
    fclose(file);
    json_object_put(root);
    return CMDSET_SUCCESS;
}

int cmdset_load_presets(cmdset_manager_t *manager) {
    if (manager == NULL) {
        strcpy(last_error_message, "Manager is NULL");
        return CMDSET_ERROR_INVALID;
    }
    FILE *file = fopen(PRESET_FILE, "r");
    if (file == NULL) {
        manager->count = 0;
        return CMDSET_SUCCESS;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(file);
        strcpy(last_error_message, "Memory allocation failed");
        return CMDSET_ERROR_MEMORY;
    }
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    json_object *root = json_tokener_parse(content);
    free(content);
    if (root == NULL) {
        strcpy(last_error_message, "Could not parse JSON file");
        return CMDSET_ERROR_JSON;
    }
    manager->count = 0;
    json_object *presets_array;
    if (json_object_object_get_ex(root, "presets", &presets_array) && 
        json_object_is_type(presets_array, json_type_array)) {
        int array_size = json_object_array_length(presets_array);
        for (int i = 0; i < array_size && manager->count < MAX_PRESETS; i++) {
            json_object *preset = json_object_array_get_idx(presets_array, i);
            if (preset != NULL) {
                manager->presets[manager->count].active = 1;
                json_object *name_item;
                if (json_object_object_get_ex(preset, "name", &name_item) && 
                    json_object_is_type(name_item, json_type_string)) {
                    const char *name_str = json_object_get_string(name_item);
                    strncpy(manager->presets[manager->count].name, name_str, MAX_NAME_LEN - 1);
                    manager->presets[manager->count].name[MAX_NAME_LEN - 1] = '\0';
                }
                json_object *command_item;
                if (json_object_object_get_ex(preset, "command", &command_item) && 
                    json_object_is_type(command_item, json_type_string)) {
                    const char *command_str = json_object_get_string(command_item);
                    strncpy(manager->presets[manager->count].command, command_str, MAX_COMMAND_LEN - 1);
                    manager->presets[manager->count].command[MAX_COMMAND_LEN - 1] = '\0';
                }
                json_object *encrypt_item;
                if (json_object_object_get_ex(preset, "encrypt", &encrypt_item)) {
                    manager->presets[manager->count].encrypt = json_object_get_boolean(encrypt_item) ? 1 : 0;
                } else manager->presets[manager->count].encrypt = 0;
                json_object *created_item;
                if (json_object_object_get_ex(preset, "created_at", &created_item) && 
                    json_object_is_type(created_item, json_type_int)) {
                    manager->presets[manager->count].created_at = (time_t)json_object_get_int64(created_item);
                } else manager->presets[manager->count].created_at = time(NULL);
                json_object *last_used_item;
                if (json_object_object_get_ex(preset, "last_used", &last_used_item) && 
                    json_object_is_type(last_used_item, json_type_int)) {
                    manager->presets[manager->count].last_used = (time_t)json_object_get_int64(last_used_item);
                } else manager->presets[manager->count].last_used = 0;
                json_object *use_count_item;
                if (json_object_object_get_ex(preset, "use_count", &use_count_item) && 
                    json_object_is_type(use_count_item, json_type_int)) {
                    manager->presets[manager->count].use_count = json_object_get_int(use_count_item);
                } else manager->presets[manager->count].use_count = 0;
                manager->count++;
            }
        }
    }
    json_object_put(root);
    return CMDSET_SUCCESS;
}

int cmdset_export_presets(cmdset_manager_t *manager, const char *filename) {
    if (manager == NULL || filename == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    json_object *root = json_object_new_object();
    if (root == NULL) {
        strcpy(last_error_message, "Could not create JSON object");
        return CMDSET_ERROR_JSON;
    }
    json_object *version = json_object_new_string("2.0");
    json_object_object_add(root, "version", version);
    json_object *exported_at = json_object_new_int64(time(NULL));
    json_object_object_add(root, "exported_at", exported_at);
    json_object *presets_array = json_object_new_array();
    if (presets_array == NULL) {
        json_object_put(root);
        strcpy(last_error_message, "Could not create presets array");
        return CMDSET_ERROR_JSON;
    }
    int exported_count = 0;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            json_object *preset = json_object_new_object();
            if (preset == NULL) {
                json_object_put(root);
                strcpy(last_error_message, "Could not create preset object");
                return CMDSET_ERROR_JSON;
            }
            json_object_object_add(preset, "name", json_object_new_string(manager->presets[i].name));
            json_object_object_add(preset, "command", json_object_new_string(manager->presets[i].command));
            json_object_object_add(preset, "encrypt", json_object_new_boolean(manager->presets[i].encrypt));
            json_object_object_add(preset, "created_at", json_object_new_int64(manager->presets[i].created_at));
            json_object_object_add(preset, "last_used", json_object_new_int64(manager->presets[i].last_used));
            json_object_object_add(preset, "use_count", json_object_new_int(manager->presets[i].use_count));
            json_object_array_add(presets_array, preset);
            exported_count++;
        }
    }
    json_object_object_add(root, "presets", presets_array);
    json_object *count = json_object_new_int(exported_count);
    json_object_object_add(root, "count", count);
    const char *json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    if (json_string == NULL) {
        json_object_put(root);
        strcpy(last_error_message, "Could not generate JSON string");
        return CMDSET_ERROR_JSON;
    }
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        json_object_put(root);
        snprintf(last_error_message, sizeof(last_error_message), "Could not create export file '%s': %s", filename, strerror(errno));
        return CMDSET_ERROR_FILE;
    }
    fprintf(file, "%s", json_string);
    fclose(file);
    json_object_put(root);
    return CMDSET_SUCCESS;
}

int cmdset_import_presets(cmdset_manager_t *manager, const char *filename) {
    if (manager == NULL || filename == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        snprintf(last_error_message, sizeof(last_error_message), "Could not open import file '%s': %s", filename, strerror(errno));
        return CMDSET_ERROR_FILE;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(file);
        strcpy(last_error_message, "Memory allocation failed");
        return CMDSET_ERROR_MEMORY;
    }
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    json_object *root = json_tokener_parse(content);
    free(content);
    if (root == NULL) {
        strcpy(last_error_message, "Could not parse JSON file");
        return CMDSET_ERROR_JSON;
    }
    json_object *presets_array;
    if (!json_object_object_get_ex(root, "presets", &presets_array) || 
        !json_object_is_type(presets_array, json_type_array)) {
        json_object_put(root);
        strcpy(last_error_message, "Invalid preset file format - missing presets array");
        return CMDSET_ERROR_JSON;
    }
    int array_size = json_object_array_length(presets_array);
    for (int i = 0; i < array_size && manager->count < MAX_PRESETS; i++) {
        json_object *preset = json_object_array_get_idx(presets_array, i);
        if (preset != NULL) {
            json_object *name_item;
            if (!json_object_object_get_ex(preset, "name", &name_item) || 
                !json_object_is_type(name_item, json_type_string)) {
                continue;
            }
            const char *name_str = json_object_get_string(name_item);
            int exists = 0;
            for (int j = 0; j < manager->count; j++) {
                if (manager->presets[j].active && strcmp(manager->presets[j].name, name_str) == 0) {
                    exists = 1;
                    break;
                }
            }
            if (exists) continue;
            manager->presets[manager->count].active = 1;
            strncpy(manager->presets[manager->count].name, name_str, MAX_NAME_LEN - 1);
            manager->presets[manager->count].name[MAX_NAME_LEN - 1] = '\0';
            json_object *command_item;
            if (json_object_object_get_ex(preset, "command", &command_item) && 
                json_object_is_type(command_item, json_type_string)) {
                const char *command_str = json_object_get_string(command_item);
                strncpy(manager->presets[manager->count].command, command_str, MAX_COMMAND_LEN - 1);
                manager->presets[manager->count].command[MAX_COMMAND_LEN - 1] = '\0';
            } else continue;
            json_object *encrypt_item;
            if (json_object_object_get_ex(preset, "encrypt", &encrypt_item)) manager->presets[manager->count].encrypt = json_object_get_boolean(encrypt_item) ? 1 : 0;
            else manager->presets[manager->count].encrypt = 0;
            json_object *created_item;
            if (json_object_object_get_ex(preset, "created_at", &created_item) && 
                json_object_is_type(created_item, json_type_int)) {
                manager->presets[manager->count].created_at = (time_t)json_object_get_int64(created_item);
            } else manager->presets[manager->count].created_at = time(NULL);
            json_object *last_used_item;
            if (json_object_object_get_ex(preset, "last_used", &last_used_item) && 
                json_object_is_type(last_used_item, json_type_int)) {
                manager->presets[manager->count].last_used = (time_t)json_object_get_int64(last_used_item);
            } else manager->presets[manager->count].last_used = 0;
            json_object *use_count_item;
            if (json_object_object_get_ex(preset, "use_count", &use_count_item) && 
                json_object_is_type(use_count_item, json_type_int)) {
                manager->presets[manager->count].use_count = json_object_get_int(use_count_item);
            } else manager->presets[manager->count].use_count = 0;
            manager->count++;
        }
    }
    json_object_put(root);
    return CMDSET_SUCCESS;
}

int cmdset_encrypt_command(const char *plaintext, char *encrypted) {
    return encrypt_command_internal(plaintext, encrypted, NULL);
}

int cmdset_decrypt_command(const char *encrypted, char *plaintext) {
    return decrypt_command_internal(encrypted, plaintext, NULL);
}

void cmdset_cleanup(cmdset_manager_t *manager) {
    if (manager != NULL) memset(manager, 0, sizeof(cmdset_manager_t));
    clear_session();
}

int cmdset_get_preset_count(cmdset_manager_t *manager) {
    if (manager == NULL) return 0;
    int count = 0;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) count++;
    }
    return count;
}

int cmdset_get_preset_by_index(cmdset_manager_t *manager, int index, cmdset_preset_t *preset) {
    if (manager == NULL || preset == NULL) {
        strcpy(last_error_message, "Invalid parameters");
        return CMDSET_ERROR_INVALID;
    }
    int current_index = 0;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            if (current_index == index) {
                *preset = manager->presets[i];
                return CMDSET_SUCCESS;
            }
            current_index++;
        }
    }
    strcpy(last_error_message, "Index out of range");
    return CMDSET_ERROR_NOT_FOUND;
}

static int derive_key(const char *password, const unsigned char *salt, unsigned char *key) {
    int result = PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_LEN, 10000, EVP_sha256(), KEY_LEN, key);
    if (result != 1) return 1;
    return 0;
}

static int get_master_password(char *password, int max_len) {
    printf("Enter master password for encryption: ");
    fflush(stdout);
    #ifdef _WIN32
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
    #else
        if (isatty(STDIN_FILENO)) system("stty -echo 2>/dev/null");
    #endif
    if (fgets(password, max_len, stdin) == NULL) {
        #ifdef _WIN32
            SetConsoleMode(hStdin, mode);
        #else
            if (isatty(STDIN_FILENO)) system("stty echo 2>/dev/null");
        #endif
        return 1;
    }
    #ifdef _WIN32
        SetConsoleMode(hStdin, mode);
    #else
        if (isatty(STDIN_FILENO)) system("stty echo 2>/dev/null");
    #endif
        printf("\n");
        password[strcspn(password, "\n")] = '\0';
        return 0;
}

static int get_session_password(char *password, int max_len, const char *preset_name) {
    if (strlen(session_file) == 0) {
        const char *home = getenv("HOME");
        if (home != NULL) snprintf(session_file, sizeof(session_file), "%s/.cmdset_session", home);
        else strcpy(session_file, "/tmp/.cmdset_session");
    }
    if (is_session_valid() && strlen(current_preset_name) > 0 && 
        preset_name != NULL && strcmp(current_preset_name, preset_name) == 0) {
        strncpy(password, session_password, max_len - 1);
        password[max_len - 1] = '\0';
        return 0;
    }
    FILE *f = fopen(session_file, "r");
    if (f != NULL) {
        time_t file_time;
        if (fscanf(f, "%ld\n", &file_time) == 1) {
            time_t current_time = time(NULL);
            if (current_time - file_time <= SESSION_TIMEOUT) {
                if (fgets(session_password, sizeof(session_password), f) != NULL) {
                    session_password[strcspn(session_password, "\n")] = '\0';
                    if (fgets(current_preset_name, sizeof(current_preset_name), f) != NULL) {
                        current_preset_name[strcspn(current_preset_name, "\n")] = '\0';
                        if (preset_name != NULL && strcmp(current_preset_name, preset_name) == 0) {
                            session_start_time = file_time;
                            session_active = 1;
                            strncpy(password, session_password, max_len - 1);
                            password[max_len - 1] = '\0';
                            fclose(f);
                            return 0;
                        }
                    }
                }
            }
        }
        fclose(f);
    }
    if (get_master_password(password, max_len) != 0) return 1;
    return 0;
}

static int is_session_valid(void) {
    if (!session_active) return 0;
    time_t current_time = time(NULL);
    if (current_time - session_start_time > SESSION_TIMEOUT) {
        clear_session();
        return 0;
    }
    return 1;
}

static void clear_session(void) {
    memset(session_password, 0, sizeof(session_password));
    session_start_time = 0;
    session_active = 0;
}

static int encrypt_command_internal(const char *plaintext, char *encrypted, const char *preset_name) {
    char master_password[256];
    if (get_session_password(master_password, sizeof(master_password), preset_name) != 0) return 1;
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
    EVP_CIPHER_CTX_set_padding(ctx, 1);
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
    if (preset_name != NULL) {
        strncpy(current_preset_name, preset_name, sizeof(current_preset_name) - 1);
        current_preset_name[sizeof(current_preset_name) - 1] = '\0';
        strncpy(session_password, master_password, sizeof(session_password) - 1);
        session_password[sizeof(session_password) - 1] = '\0';
        session_start_time = time(NULL);
        session_active = 1;
        FILE *session_f = fopen(session_file, "w");
        if (session_f != NULL) {
            fprintf(session_f, "%ld\n%s\n%s\n", session_start_time, session_password, current_preset_name);
            fflush(session_f);
            fclose(session_f);
            chmod(session_file, 0600);
        }
        printf("Password cached for %d minutes. Use 'cmdset clear-session' to clear.\n", SESSION_TIMEOUT / 60);
    }
    memset(master_password, 0, sizeof(master_password));
    memset(key, 0, KEY_LEN);
    memset(salt, 0, SALT_LEN);
    memset(iv, 0, IV_LEN);
    memset(ciphertext, 0, sizeof(ciphertext));
    memset(combined, 0, sizeof(combined));
    return 0;
}

static int decrypt_command_internal(const char *encrypted, char *plaintext, const char *preset_name) {
    char master_password[256];
    if (get_session_password(master_password, sizeof(master_password), preset_name) != 0) return 1;
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
    EVP_CIPHER_CTX_set_padding(ctx, 1);
    int len;
    int plaintext_len;
    unsigned char temp_plaintext[MAX_COMMAND_LEN];
    if (EVP_DecryptUpdate(ctx, temp_plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        return 1;
    }
    plaintext_len = len;
    if (EVP_DecryptFinal_ex(ctx, temp_plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(decoded);
        memset(master_password, 0, sizeof(master_password));
        memset(key, 0, KEY_LEN);
        clear_session();
        system("rm -f ~/.cmdset_session");
        return 1;
    }
    plaintext_len += len;
    temp_plaintext[plaintext_len] = '\0';
    strcpy(plaintext, (char*)temp_plaintext);
    EVP_CIPHER_CTX_free(ctx);
    free(decoded);
    if (preset_name != NULL) {
        strncpy(current_preset_name, preset_name, sizeof(current_preset_name) - 1);
        current_preset_name[sizeof(current_preset_name) - 1] = '\0';
        strncpy(session_password, master_password, sizeof(session_password) - 1);
        session_password[sizeof(session_password) - 1] = '\0';
        session_start_time = time(NULL);
        session_active = 1;
        FILE *session_f = fopen(session_file, "w");
        if (session_f != NULL) {
            fprintf(session_f, "%ld\n%s\n%s\n", session_start_time, session_password, current_preset_name);
            fflush(session_f);
            fclose(session_f);
            chmod(session_file, 0600);
        }
        printf("Password cached for %d minutes. Use 'cmdset clear-session' to clear.\n", SESSION_TIMEOUT / 60);
    }
    memset(master_password, 0, sizeof(master_password));
    memset(key, 0, KEY_LEN);
    return 0;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [command] [options...]\n", program_name);
    printf(" %s add <name> <command>                Add a new preset\n", program_name);
    printf(" %s a <name> <command>                  Add a new preset (short)\n", program_name);
    printf(" %s add --encrypt <name> <command>      Add an encrypted preset\n", program_name);
    printf(" %s a -e <name> <command>               Add an encrypted preset (short)\n", program_name);
    printf(" %s remove <name>                       Remove a preset\n", program_name);
    printf(" %s rm <name>                           Remove a preset (short)\n", program_name);
    printf(" %s list                                List all presets\n", program_name);
    printf(" %s ls                                  List all presets (short)\n", program_name);
    printf(" %s exec <name> [args...]               Execute a preset with optional arguments\n", program_name);
    printf(" %s e <name> [args...]                  Execute a preset with optional arguments (short)\n", program_name);
    printf(" %s run <name> [args...]                Execute a preset with optional arguments (short)\n", program_name);
    printf(" %s help                                Show this help message\n", program_name);
    printf(" %s h                                   Show this help message (short)\n", program_name);
    printf(" %s clear-session                       Clear cached password session\n", program_name);
    printf(" %s cs                                  Clear cached password session (short)\n", program_name);
    printf(" %s status                              Show session status\n", program_name);
    printf(" %s s                                   Show session status (short)\n", program_name);
    printf(" %s export [filename]                   Export presets to JSON file\n", program_name);
    printf(" %s exp [filename]                      Export presets to JSON file (short)\n", program_name);
    printf(" %s import [filename]                   Import presets from JSON file\n", program_name);
    printf(" %s imp [filename]                      Import presets from JSON file (short)\n", program_name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    cmdset_manager_t manager;
    int result = cmdset_init(&manager);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to initialize CmdSet: %s\n", cmdset_get_error_message(result));
        return 1;
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "h") == 0) {
        print_usage(argv[0]);
        cmdset_cleanup(&manager);
        return 0;
    }
    if (strcmp(argv[1], "add") == 0 || strcmp(argv[1], "a") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: add command requires name and command\n");
            print_usage(argv[0]);
            cmdset_cleanup(&manager);
            return 1;
        }
        char* name;
        char* command;
        int encrypt = 0;
        if (argc >= 4 && strcmp(argv[2], "--encrypt") == 0) {
            encrypt = 1;
            name = argv[3];
            command = argv[4];
        }
        else if (argc >= 5 && strcmp(argv[2], "-e") == 0) {
            encrypt = 1;
            name = argv[3];
            command = argv[4];
        }
        else {
            name = argv[2];
            command = argv[3];
            if (argc > 4 && strcmp(argv[4], "-e") == 0) encrypt = 1;
        }
        result = cmdset_add_preset(&manager, name, command, encrypt);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to add preset: %s\n", cmdset_get_error_message(result));
            cmdset_cleanup(&manager);
            return 1;
        }
        result = cmdset_save_presets(&manager);
        if (result != 0) fprintf(stderr, "Warning: Failed to save presets: %s\n", cmdset_get_error_message(result));
        printf("Preset '%s' added successfully\n", name);
    }
    else if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) {
        int count = cmdset_get_preset_count(&manager);
        if (count == 0) printf("No presets found\n");
        else {
            printf("Found %d preset(s):\n", count);
            for (int i = 0; i < count; i++) {
                cmdset_preset_t preset;
                if (cmdset_get_preset_by_index(&manager, i, &preset) == 0) printf("  %s: %s%s\n", preset.name, preset.command, preset.encrypt ? " (encrypted)" : "");
            }
        }
    }
    else if (strcmp(argv[1], "exec") == 0 || strcmp(argv[1], "e") == 0 || strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: exec command requires preset name\n");
            print_usage(argv[0]);
            cmdset_cleanup(&manager);
            return 1;
        }
        char* name = argv[2];
        char* additional_args = NULL;
        if (argc > 3) {
            int total_len = 0;
            for (int i = 3; i < argc; i++) {
                total_len += strlen(argv[i]) + 1;
            }
            additional_args = malloc(total_len);
            if (additional_args) {
                strcpy(additional_args, argv[3]);
                for (int i = 4; i < argc; i++) {
                    strcat(additional_args, " ");
                    strcat(additional_args, argv[i]);
                }
            }
        }
        result = cmdset_execute_preset(&manager, name, additional_args);
        if (result < 0) {
            fprintf(stderr, "Error: Failed to execute preset: %s\n", cmdset_get_error_message(result));
            if (additional_args) free(additional_args);
            cmdset_cleanup(&manager);
            return 1;
        }
        if (additional_args) free(additional_args);
        return result; // Return the exit code from the executed command
    }
    else if (strcmp(argv[1], "remove") == 0 || strcmp(argv[1], "rm") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: remove command requires preset name\n");
            print_usage(argv[0]);
            cmdset_cleanup(&manager);
            return 1;
        }
        char* name = argv[2];
        result = cmdset_remove_preset(&manager, name);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to remove preset: %s\n", cmdset_get_error_message(result));
            cmdset_cleanup(&manager);
            return 1;
        }
        result = cmdset_save_presets(&manager);
        if (result != 0) {
            fprintf(stderr, "Warning: Failed to save presets: %s\n", cmdset_get_error_message(result));
        }
        printf("Preset '%s' removed successfully\n", name);
    }
    else if (strcmp(argv[1], "clear-session") == 0 || strcmp(argv[1], "cs") == 0) {
        clear_session();
        system("rm -f ~/.cmdset_session");
        printf("Password session cleared\n");
    }
    else if (strcmp(argv[1], "status") == 0 || strcmp(argv[1], "s") == 0) {
        int count = cmdset_get_preset_count(&manager);
        printf("Session Status:\n");
        printf("  Active presets: %d\n", count);
        printf("  Manager initialized: Yes\n");
    }
    else if (strcmp(argv[1], "export") == 0 || strcmp(argv[1], "exp") == 0) {
        char* filename = "cmdset_export.json";
        if (argc > 2) filename = argv[2];
        result = cmdset_export_presets(&manager, filename);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to export presets: %s\n", cmdset_get_error_message(result));
            cmdset_cleanup(&manager);
            return 1;
        }
        printf("Presets exported to '%s'\n", filename);
    }
    else if (strcmp(argv[1], "import") == 0 || strcmp(argv[1], "imp") == 0) {
        char* filename = "cmdset_export.json";
        if (argc > 2) {
            filename = argv[2];
        }
        result = cmdset_import_presets(&manager, filename);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to import presets: %s\n", cmdset_get_error_message(result));
            cmdset_cleanup(&manager);
            return 1;
        }
        result = cmdset_save_presets(&manager);
        if (result != 0) fprintf(stderr, "Warning: Failed to save presets: %s\n", cmdset_get_error_message(result));
        printf("Presets imported from '%s'\n", filename);
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        print_usage(argv[0]);
        cmdset_cleanup(&manager);
        return 1;
    }
    cmdset_cleanup(&manager);
    return 0;
}
