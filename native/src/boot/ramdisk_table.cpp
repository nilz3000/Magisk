#include "ramdisk_table.hpp"
#include "magiskboot.hpp"

#include <cstring>
#include <algorithm>
#include <base.hpp>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

void ramdisk_table::rm(const char *name) {
    remove_if(entries.begin(), entries.end(), [&] (unique_ptr<struct vendor_ramdisk_table_entry_v4> e) {
        return !strncmp(e->ramdisk_name, name, VENDOR_RAMDISK_NAME_SIZE);
    });
}

void ramdisk_table::add(const char *name, ramdisk_type type) {
    char file_name[PATH_MAX] = {};
    ssprintf(file_name, sizeof(file_name), VENDOR_RAMDISK_FILE, VENDOR_RAMDISK_NAME_SIZE, name);

    auto entry = make_unique<struct vendor_ramdisk_table_entry_v4>();
    memset(entry.get(), 0, sizeof(vendor_ramdisk_table_entry_v4));
    entry->ramdisk_type = type;

    // probably safest to sum up existing sizes
    for (auto const &e : entries) {
        entry->ramdisk_offset += e->ramdisk_size;
    }
    memcpy(&entry->ramdisk_name, name, strlen(name));
    memcpy(entry->board_id, entries.front()->board_id, 4 * VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE);

    struct stat ramdisk_stat;
    xstat(file_name, &ramdisk_stat);
    entry->ramdisk_size =
    memcpy()
}

int ramdisk_table_commands(int argc, char *argv[]) {
    char *in_table = argv[0];
    ++argv;
    --argc;
    ramdisk_table table;
    if (access(in_table, R_OK) == 0) {
        table.load_table(in_table);
    } else {
        return 1;
    }

    int cmdc;
    char *cmdv[4];

    for (int i = 0; i < argc; ++i) {
        // Reset
        cmdc = 0;
        memset(cmdv, 0, sizeof(cmdv));

        // Split the commands
        char *tok = strtok(argv[i], " ");
        while (tok && cmdc < std::size(cmdv)) {
            if (cmdc == 0 && tok[0] == '#')
                break;
            cmdv[cmdc++] = tok;
            tok = strtok(nullptr, " ");
        }

        if (cmdc == 0)
            continue;

        if (cmdc == 2 && cmdv[0] == "rm"sv) {
            table.rm(cmdv[1]);
        } else if (cmdc == 3 && cmdv[0] == "add"sv) {
            char *type_name = cmdv[2];
            ramdisk_type type;
            if (type_name == "none"sv) {
                type = VENDOR_RAMDISK_TYPE_NONE;
            } else if (type_name == "dlkm"sv) {
                type = VENDOR_RAMDISK_TYPE_DLKM;
            } else if (type_name == "platform"sv) {
                type = VENDOR_RAMDISK_TYPE_PLATFORM;
            } else if (type_name == "recovery"sv) {
                type = VENDOR_RAMDISK_TYPE_RECOVERY;
            } else {
                fprintf(stderr, "Please specify a ramdisk type. Available options are 'none', 'dlkm', 'platform', 'recovery'.\n");
                return 1;
            }

            char *name = cmdv[1];
            if (strlen(name) > 32) {
                fprintf(stderr, "Name is to long. Maximal length is 32 characters.\n");
                return 1;
            }
            if (any_of(entries.begin(), entries.end(), [&] (unique_ptr<struct vendor_ramdisk_table_entry_v4> e) {
                return !strncmp(e->ramdisk_name, name, VENDOR_RAMDISK_NAME_SIZE);})) {
                fprintf(stderr, "An entry with name %.*s already exist.\n", VENDOR_RAMDISK_NAME_SIZE, name);
                return 1;
            }

            char file_name[PATH_MAX] = {};
            ssprintf(file_name, sizeof(file_name), VENDOR_RAMDISK_FILE, VENDOR_RAMDISK_NAME_SIZE, name);
            if (access(file_name, R_OK)) {
                fprintf(stderr, "%.*s doesn't exist.\n", PATH_MAX, name);
                return 1;
            }
            table.add(name, type);
        } else {
            return 1;
        }
    }

    cpio.dump(incpio);
    return 0;
}