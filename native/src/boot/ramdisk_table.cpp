#include <cstring>
#include <algorithm>
#include <limits.h>
#include <sys/stat.h>

#include <base.hpp>

#include "bootimg.hpp"
#include "magiskboot.hpp"
#include "ramdisk_table.hpp"

#define PADDING 15

using namespace std;

void ramdisk_table::rm(const char *name) {
    auto it = entries.begin();
    bool not_found = true;
    while (it != entries.end()) {
        auto entry = it->get();
        if (!strncmp((char *)entry->ramdisk_name, name, VENDOR_RAMDISK_NAME_SIZE)) {
            entries.erase(it);
            not_found = false;
            break;
        }
        ++it;
    }
    if (not_found)
        fprintf(stderr, "No table entry with name %s\n", name);
}

void ramdisk_table::add(const char *name, ramdisk_type type, const uint32_t *id, int copy_id) {
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
    memcpy(&entry->board_id, &entries.front()->board_id, 4 * VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE);

    struct stat ramdisk_stat;
    xstat(file_name, &ramdisk_stat);
    entry->ramdisk_size = ramdisk_stat.st_size;
    entries.push_back(std::move(entry));
}

void ramdisk_table::dump(const char *file) {
    int fd = creat(file, 0644);
    for (const auto &entry : entries) {
        xwrite(fd, entry.get(), sizeof(vendor_ramdisk_table_entry_v4));
    }
    close(fd);
}

void ramdisk_table::load(const char *file) {
    if (access(file, R_OK) == 0) {
        auto m = mmap_data(file);
        for (int i = 0; i < m.sz; i += sizeof(vendor_ramdisk_table_entry_v4)) {
            auto entry = make_unique<struct vendor_ramdisk_table_entry_v4>();
            if (m.sz >= i + sizeof(vendor_ramdisk_table_entry_v4))
                memcpy(entry.get(), m.buf + i, sizeof(vendor_ramdisk_table_entry_v4));
            else
                return;
            entries.push_back(std::move(entry));
        }
        return;
    }
    fprintf(stderr, "Can't find ramdisk table file.\n");
    exit(1);
}

void ramdisk_table::print() {
    for (int i = 0; i < entries.size(); ++i) {
        auto &entry = entries.at(i);
        fprintf(stderr, "%-*s [%d]\n", PADDING, "ENTRY", i);
        fprintf(stderr, "%-*s [%.*s]\n", PADDING, "NAME", VENDOR_RAMDISK_NAME_SIZE, entry->ramdisk_name);
        fprintf(stderr, "%-*s [%s]\n", PADDING, "TYPE", type2name[entry->ramdisk_type]);
        fprintf(stderr, "%-*s [%d]\n", PADDING, "RAMDISK_OFFSET", entry->ramdisk_offset);
        fprintf(stderr, "%-*s [%d]\n", PADDING, "RAMDISK_SIZE", entry->ramdisk_size);
        fprintf(stderr, "%-*s [0x%X", PADDING, "BOARD_ID", entry->board_id[0]);
        for (int j = 1; j < VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE - 1; ++j) {
            fprintf(stderr, " 0x%X", entry->board_id[j]);
        }
        fprintf(stderr, " 0x%X]\n\n", entry->board_id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE - 1]);
    }
}

bool ramdisk_table::name_exist(const char *name) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        auto entry = it->get();
        if (!strncmp((char *)entry->ramdisk_name, name, VENDOR_RAMDISK_NAME_SIZE)) {
            return true;
        }
    }
    return false;
}

int ramdisk_table_commands(int argc, char *argv[]) {
    char *in_table = argv[0];
    ++argv;
    --argc;
    ramdisk_table table;

    table.load(in_table);
    table.print();

    int cmdc;
    char *cmdv[10];

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

        if (cmdc >= 2 && cmdv[0] == "rm"sv) {
            if (cmdv[1] == "--name"sv) {
                if (cmdc == 2)
                    table.rm("\0");
                else
                    table.rm(cmdv[2]);
            } else {
                return 1;
            }
            table.dump(in_table);
        } else if (cmdv[0] == "add"sv) {
            ramdisk_type type;
            char *name;
            uint32_t id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE];
            memset(id, 0, 4 * VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE);
            int copy_id = -1;
            for (int i = 1; i < argc - 1; i += 2) {
                if (cmdv[i] == "--type"sv && i + 1 < argc - 1) {
                    if (argv[i + 1] == "none"sv)
                        type = VENDOR_RAMDISK_TYPE_NONE;
                    else if (argv[i + 1] == "dlkm"sv)
                        type = VENDOR_RAMDISK_TYPE_DLKM;
                    else if (argv[i + 1] == "platform"sv)
                        type = VENDOR_RAMDISK_TYPE_PLATFORM;
                    else if (argv[i + 1] == "recovery"sv)
                        type = VENDOR_RAMDISK_TYPE_RECOVERY;
                    else
                        return 1;
                    continue;
                } else if (cmdv[i] == "--name"sv && i + 1 < argc - 1) {
                    if (strlen(cmdv[i + 1]) > 32) {
                        fprintf(stderr, "Name is to long. Maximal length is 32 characters.\n");
                        return 1;
                    }
                    if (table.name_exist(cmdv[i + 1])) {
                        fprintf(stderr,
                                "An entry with name %.*s already exist.\n",
                                VENDOR_RAMDISK_NAME_SIZE,
                                cmdv[i + 1]);
                        exit(1);
                    }
                    name = argv[i + 1];
                    continue;
                } else if (cmdv[i] == "--copy-id"sv && i + 1 < argc - 1) {
                    if (cmdv[i + 1] == "copy"sv)
                        continue;

                }
            }
            table.add(name, type, id, copy_id);
        }
    }

    return 0;
}

const char *Type2Name::operator[](uint32_t type) {
    switch (type) {
        case VENDOR_RAMDISK_TYPE_NONE:
            return "none";
        case VENDOR_RAMDISK_TYPE_PLATFORM:
            return "platform";
        case VENDOR_RAMDISK_TYPE_DLKM:
            return "dlkm";
        case VENDOR_RAMDISK_TYPE_RECOVERY:
            return "recovery";
        default:
            return "\0";
    }
}