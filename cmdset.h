#ifndef CMDSET_H
#define CMDSET_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[50];
    char command[500];
    int active;
    int encrypt;
    long created_at;
    long last_used;
    int use_count;
} cmdset_preset_t;

typedef struct {
    cmdset_preset_t presets[100];
    int count;
} cmdset_manager_t;

int cmdset_init(cmdset_manager_t *manager);
int cmdset_add_preset(cmdset_manager_t *manager, const char *name, const char *command, int encrypt);
int cmdset_remove_preset(cmdset_manager_t *manager, const char *name);
int cmdset_execute_preset(cmdset_manager_t *manager, const char *name, const char *additional_args);
int cmdset_list_presets(cmdset_manager_t *manager, char *output, int max_len);
int cmdset_find_preset(cmdset_manager_t *manager, const char *name, cmdset_preset_t *preset);
int cmdset_save_presets(cmdset_manager_t *manager);
int cmdset_load_presets(cmdset_manager_t *manager);
int cmdset_export_presets(cmdset_manager_t *manager, const char *filename);
int cmdset_import_presets(cmdset_manager_t *manager, const char *filename);
int cmdset_encrypt_command(const char *plaintext, char *encrypted);
int cmdset_decrypt_command(const char *encrypted, char *plaintext);
void cmdset_cleanup(cmdset_manager_t *manager);

const char* cmdset_get_error_message(int error_code);
int cmdset_get_preset_count(cmdset_manager_t *manager);
int cmdset_get_preset_by_index(cmdset_manager_t *manager, int index, cmdset_preset_t *preset);

#ifdef __cplusplus
}
#endif

#endif // CMDSET_H
