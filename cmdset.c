#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define MAX_PRESETS 100
#define MAX_NAME_LEN 50
#define MAX_COMMAND_LEN 500
#define PRESET_FILE ".cmdset_presets"
#define JSON_LINE_BUFFER 1024

typedef struct {
    char name[MAX_NAME_LEN];
    char command[MAX_COMMAND_LEN];
    int active;
    time_t created_at;
    time_t last_used;
    int use_count;
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
Preset* find_preset(PresetManager *manager, const char *name);
void json_escape_string(const char *input, char *output);
int save_presets_json(PresetManager *manager);
int load_presets_json(PresetManager *manager);

int main(int argc, char *argv[]) {
    PresetManager manager;
    manager.count = 0;
    
    // Load existing presets first
    load_presets_json(&manager);
    
    int result = parse_arguments(argc, argv, &manager);
    if (result == 0) save_presets_json(&manager); // Save presets if any changes were made
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
    manager->presets[manager->count].created_at = time(NULL);
    manager->presets[manager->count].last_used = 0;
    manager->presets[manager->count].use_count = 0;
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
    preset->last_used = time(NULL);
    preset->use_count++;
    printf("Executing: %s\n", preset->command);
    printf("----------------------------------------\n");
    int result = system(preset->command);
    if (result != 0) {
        printf("----------------------------------------\n");
        printf("Command exited with status %d\n", result);
    }
    return result;
}

void json_escape_string(const char *input, char *output) {
    int j = 0;
    for (int i = 0; input[i] != '\0' && j < MAX_COMMAND_LEN * 2 - 1; i++) {
        switch (input[i]) {
            case '"':  output[j++] = '\\'; output[j++] = '"'; break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n'; break;
            case '\r': output[j++] = '\\'; output[j++] = 'r'; break;
            case '\t': output[j++] = '\\'; output[j++] = 't'; break;
            default:   output[j++] = input[i]; break;
        }
    }
    output[j] = '\0';
}

int save_presets_json(PresetManager *manager) {
    FILE *file = fopen(PRESET_FILE, "w");
    if (file == NULL) {
        printf("Error: Could not save presets to file: %s\n", strerror(errno));
        return 1;
    }
    fprintf(file, "{\n");
    fprintf(file, "  \"version\": \"2.0\",\n");
    fprintf(file, "  \"presets\": [\n");
    int first = 1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active) {
            if (!first) fprintf(file, ",\n");
            first = 0;
            char escaped_name[MAX_NAME_LEN * 2];
            char escaped_command[MAX_COMMAND_LEN * 2];
            json_escape_string(manager->presets[i].name, escaped_name);
            json_escape_string(manager->presets[i].command, escaped_command);
            fprintf(file, "    {\n");
            fprintf(file, "      \"name\": \"%s\",\n", escaped_name);
            fprintf(file, "      \"command\": \"%s\",\n", escaped_command);
            fprintf(file, "      \"created_at\": %ld,\n", manager->presets[i].created_at);
            fprintf(file, "      \"last_used\": %ld,\n", manager->presets[i].last_used);
            fprintf(file, "      \"use_count\": %d\n", manager->presets[i].use_count);
            fprintf(file, "    }");
        }
    }
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    fclose(file);
    return 0;
}

int load_presets_json(PresetManager *manager) {
    FILE *file = fopen(PRESET_FILE, "r");
    if (file == NULL) return 1; // File doesn't exist
    
    // Read entire file into memory
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
    
    // Simple JSON parsing - find all presets
    manager->count = 0;
    char *pos = content;
    
    while (*pos && manager->count < MAX_PRESETS) {
        // Find next preset object by looking for "name": pattern
        char *preset_start = strstr(pos, "\"name\":");
        if (preset_start == NULL) break;
        
        // Initialize preset
        manager->presets[manager->count].active = 1;
        manager->presets[manager->count].created_at = time(NULL);
        manager->presets[manager->count].last_used = 0;
        manager->presets[manager->count].use_count = 0;
        
        // Parse name - find the value after "name":
        char *name_start = strstr(preset_start, "\"name\":");
        if (name_start) {
            // Skip to the colon and find the value
            char *colon = strchr(name_start, ':');
            if (colon) {
                colon++; // Skip the colon
                while (*colon == ' ' || *colon == '\t') colon++; // Skip whitespace
                if (*colon == '"') {
                    colon++; // Skip the opening quote
                    char *name_end = strchr(colon, '"');
                    if (name_end) {
                        *name_end = '\0';
                        strncpy(manager->presets[manager->count].name, colon, MAX_NAME_LEN - 1);
                        manager->presets[manager->count].name[MAX_NAME_LEN - 1] = '\0';
                        *name_end = '"'; // Restore
                    }
                }
            }
        }
        
        // Parse command - find the value after "command":
        char *cmd_start = strstr(preset_start, "\"command\":");
        if (cmd_start) {
            // Skip to the colon and find the value
            char *colon = strchr(cmd_start, ':');
            if (colon) {
                colon++; // Skip the colon
                while (*colon == ' ' || *colon == '\t') colon++; // Skip whitespace
                if (*colon == '"') {
                    colon++; // Skip the opening quote
                    char *cmd_end = strchr(colon, '"');
                    if (cmd_end) {
                        *cmd_end = '\0';
                        strncpy(manager->presets[manager->count].command, colon, MAX_COMMAND_LEN - 1);
                        manager->presets[manager->count].command[MAX_COMMAND_LEN - 1] = '\0';
                        *cmd_end = '"'; // Restore
                    }
                }
            }
        }
        
        // Parse timestamps and use count
        char *created_start = strstr(preset_start, "\"created_at\":");
        if (created_start) {
            char *value_start = strchr(created_start, ':');
            if (value_start) {
                value_start++;
                while (*value_start == ' ') value_start++;
                manager->presets[manager->count].created_at = atol(value_start);
            }
        }
        
        char *last_used_start = strstr(preset_start, "\"last_used\":");
        if (last_used_start) {
            char *value_start = strchr(last_used_start, ':');
            if (value_start) {
                value_start++;
                while (*value_start == ' ') value_start++;
                manager->presets[manager->count].last_used = atol(value_start);
            }
        }
        
        char *use_count_start = strstr(preset_start, "\"use_count\":");
        if (use_count_start) {
            char *value_start = strchr(use_count_start, ':');
            if (value_start) {
                value_start++;
                while (*value_start == ' ') value_start++;
                manager->presets[manager->count].use_count = atoi(value_start);
            }
        }
        
        manager->count++;
        pos = preset_start + 1;
    }
    
    free(content);
    return 0;
}

Preset* find_preset(PresetManager *manager, const char *name) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->presets[i].active && strcmp(manager->presets[i].name, name) == 0) {
            return &manager->presets[i];
        }
    }
    return NULL;
}
