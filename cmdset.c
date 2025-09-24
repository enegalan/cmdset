#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_PRESETS 100
#define MAX_NAME_LEN 50
#define MAX_COMMAND_LEN 500
#define PRESET_FILE ".cmdset_presets"
#define SEPARATOR "|||CMDSEP|||"
#define MAX_LINE_LEN (MAX_NAME_LEN + MAX_COMMAND_LEN + 20)

typedef struct {
    char name[MAX_NAME_LEN];
    char command[MAX_COMMAND_LEN];
    int active;
} Preset;

typedef struct {
    Preset presets[MAX_PRESETS];
    int count;
} PresetManager;

void print_usage(const char *program_name);
int parse_arguments(int argc, char *argv[], PresetManager *manager);
int add_preset(PresetManager *manager, const char *name, const char *command);
int remove_preset(PresetManager *manager, const char *name);
int list_presets(PresetManager *manager);
int execute_preset(PresetManager *manager, const char *name);
int save_presets(PresetManager *manager);
int load_presets(PresetManager *manager);
Preset* find_preset(PresetManager *manager, const char *name);
void encode_string(const char *input, char *output);
void decode_string(const char *input, char *output);

int main(int argc, char *argv[]) {
    PresetManager manager;
    manager.count = 0;
    load_presets(&manager);
    int result = parse_arguments(argc, argv, &manager);
    if (result == 0) save_presets(&manager); // Save presets if any changes were made
    return result;
}

void print_usage(const char *program_name) {
    printf("Usage: %s [options...]\n", program_name);
    printf(" %s add <name> <command>       Add a new preset\n", program_name);
    printf(" %s a <name> <command>          Add a new preset (short)\n", program_name);
    printf(" %s remove <name>              Remove a preset\n", program_name);
    printf(" %s rm <name>                   Remove a preset (short)\n", program_name);
    printf(" %s list                       List all presets\n", program_name);
    printf(" %s ls                         List all presets (short)\n", program_name);
    printf(" %s exec <name>                Execute a preset\n", program_name);
    printf(" %s e <name>                   Execute a preset (short)\n", program_name);
    printf(" %s run <name>                 Execute a preset (short)\n", program_name);
    printf(" %s help                       Show this help message\n", program_name);
    printf(" %s h                          Show this help message (short)\n", program_name);
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
    if (strcmp(argv[1], "add") == 0 || strcmp(argv[1], "a") == 0) {
        if (argc < 4) {
            printf("Error: add command requires name and command arguments\n");
            printf("Usage: %s add <name> <command>\n", argv[0]);
            return 1;
        }
        return add_preset(manager, argv[2], argv[3]);
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
            printf("Usage: %s exec <name>\n", argv[0]);
            return 1;
        }
        return execute_preset(manager, argv[2]);
    }
    printf("Error: Unknown command '%s'\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}

int add_preset(PresetManager *manager, const char *name, const char *command) {
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
    strcpy(manager->presets[manager->count].command, command);
    manager->presets[manager->count].active = 1;
    manager->count++;
    printf("Preset '%s' added successfully\n", name);
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
            printf("%d. %s: %s\n", active_count + 1, 
                manager->presets[i].name, 
                manager->presets[i].command);
            active_count++;
        }
    }
    if (active_count == 0) printf("No presets found\n");
    else printf("\nTotal: %d preset(s)\n", active_count);
    return 0;
}

int execute_preset(PresetManager *manager, const char *name) {
    Preset *preset = find_preset(manager, name);
    if (preset == NULL) {
        printf("Error: Preset '%s' not found\n", name);
        return 1;
    }
    printf("Executing: %s\n", preset->command);
    printf("----------------------------------------\n");
    int result = system(preset->command);
    if (result != 0) {
        printf("----------------------------------------\n");
        printf("Command exited with status %d\n", result);
    }
    return result;
}

int save_presets(PresetManager *manager) {
    FILE *file = fopen(PRESET_FILE, "w");
    if (file == NULL) {
        printf("Error: Could not save presets to file: %s\n", strerror(errno));
        return 1;
    }
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            char encoded_command[MAX_COMMAND_LEN * 2];
            encode_string(manager->presets[i].command, encoded_command);
            fprintf(file, "%s%s%s\n",
                manager->presets[i].name,
                SEPARATOR,
                encoded_command);
        }
    }
    fclose(file);
    return 0;
}

int load_presets(PresetManager *manager) {
    FILE *file = fopen(PRESET_FILE, "r");
    if (file == NULL) return 0; // File doesn't exist, start with empty presets
    char line[MAX_LINE_LEN];
    manager->count = 0;
    while (fgets(line, sizeof(line), file) && manager->count < MAX_PRESETS) {
        line[strcspn(line, "\n")] = 0;
        char *separator = strstr(line, SEPARATOR); // Find separator
        if (separator == NULL) continue; // Skip malformed lines
        *separator = '\0';
        char *name = line;
        char *encoded_command = separator + strlen(SEPARATOR);
        if (strlen(name) < MAX_NAME_LEN && strlen(encoded_command) < MAX_COMMAND_LEN * 2) {
            char decoded_command[MAX_COMMAND_LEN];
            decode_string(encoded_command, decoded_command);
            strcpy(manager->presets[manager->count].name, name);
            strcpy(manager->presets[manager->count].command, decoded_command);
            manager->presets[manager->count].active = 1;
            manager->count++;
        }
    }
    fclose(file);
    return 0;
}

void encode_string(const char *input, char *output) {
    int j = 0;
    for (int i = 0; input[i] != '\0' && j < MAX_COMMAND_LEN - 1; i++) {
        switch (input[i]) {
            case '\n':
                output[j++] = '\\';
                output[j++] = 'n';
                break;
            case '\r':
                output[j++] = '\\';
                output[j++] = 'r';
                break;
            case '\t':
                output[j++] = '\\';
                output[j++] = 't';
                break;
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            case '|':
                output[j++] = '\\';
                output[j++] = 'p';
                break;
            case '&':
                output[j++] = '\\';
                output[j++] = 'a';
                break;
            case ';':
                output[j++] = '\\';
                output[j++] = 's';
                break;
            default:
                output[j++] = input[i];
                break;
        }
    }
    output[j] = '\0';
}

void decode_string(const char *input, char *output) {
    int j = 0;
    for (int i = 0; input[i] != '\0' && j < MAX_COMMAND_LEN - 1; i++) {
        if (input[i] == '\\' && input[i + 1] != '\0') {
            switch (input[i + 1]) {
                case 'n':
                    output[j++] = '\n';
                    i++;
                    break;
                case 'r':
                    output[j++] = '\r';
                    i++;
                    break;
                case 't':
                    output[j++] = '\t';
                    i++;
                    break;
                case '\\':
                    output[j++] = '\\';
                    i++;
                    break;
                case 'p':
                    output[j++] = '|';
                    i++;
                    break;
                case 'a':
                    output[j++] = '&';
                    i++;
                    break;
                case 's':
                    output[j++] = ';';
                    i++;
                    break;
                default:
                    output[j++] = input[i];
                    break;
            }
        } else output[j++] = input[i];
    }
    output[j] = '\0';
}

Preset* find_preset(PresetManager *manager, const char *name) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            return &manager->presets[i];
        }
    }
    return NULL;
}
