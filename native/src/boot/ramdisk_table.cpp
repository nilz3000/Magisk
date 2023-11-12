#include <charconv>
#include <cstddef>
#include <cstdint>
#include <sys/stat.h>
#include <cstring>
#include <climits>
#include <string>
#include <vector>
#include <charconv>

#include <base.hpp>

#include "bootimg.hpp"
#include "magiskboot.hpp"
#include "ramdisk_table.hpp"

#define PADDING 15

using namespace std;

bool ramdisk_table::rm(const string &name) {
    auto it = entries.begin();
    while (it != entries.end()) {
        if (string_view(it->ramdisk_name) == name) {
            entries.erase(it);
            return true;
        }
        ++it;
    }
    fprintf(stderr, "No table entry with name %s\n", name.data());
    return false;
}

bool ramdisk_table::rm(uint32_t id) {
    if (id >= entries.size()) {
        fprintf(stderr, "No table entry with id %d\n", id);
        return false;
    }
    entries.erase(entries.begin() + id);
    return true;
}

void ramdisk_table::add(const string &name, int type, const uint32_t *id) {
    char file_name[PATH_MAX] = {};
    ssprintf(file_name, sizeof(file_name), "%s/%s.cpio", VND_RAMDISK_DIR, name.c_str());

    vendor_ramdisk_table_entry_v4 entry = {};
    entry.ramdisk_type = type;

    // probably safest to sum up existing sizes
    for (auto const &e : entries) {
        entry.ramdisk_offset += e.ramdisk_size;
    }
    if (sizeof(entry.ramdisk_name) >= name.size() + 1) {
        memcpy(&entry.ramdisk_name, name.c_str(), name.size() + 1);
    }
    memcpy(&entry.board_id, id, 4 * VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE);

    struct stat ramdisk_stat;
    xstat(file_name, &ramdisk_stat);
    entry.ramdisk_size = ramdisk_stat.st_size;
    entries.emplace_back(entry);
}

void ramdisk_table::dump(const string &file) {
    int fd = creat(file.c_str(), 0644);
    xwrite(fd, entries.data(), sizeof(vendor_ramdisk_table_entry_v4) * entries.size());
    close(fd);
}

void ramdisk_table::load(const string &file) {
    if (access(file.c_str(), R_OK) == 0) {
        auto m = mmap_data(file.c_str());
        for (int i = 0; i < m.sz(); i += sizeof(vendor_ramdisk_table_entry_v4)) {
            if (m.sz() >= i + sizeof(vendor_ramdisk_table_entry_v4)) {
                entries.emplace_back(*reinterpret_cast<vendor_ramdisk_table_entry_v4*>(m.buf() + i));
            }
        }
        return;
    }
    fprintf(stderr, "Can't find ramdisk table file.\n");
    exit(1);
}

void ramdisk_table::print() {
    int i = 0;
    for (const auto &entry : entries) {
        fprintf(stderr, "%-*s [%d]\n", PADDING, "ENTRY", i++);
        fprintf(stderr, "%-*s [%.*s]\n", PADDING, "NAME", VENDOR_RAMDISK_NAME_SIZE, entry.ramdisk_name);
        fprintf(stderr, "%-*s [%s]\n", PADDING, "TYPE", type2name[entry.ramdisk_type]);
        fprintf(stderr, "%-*s [%d]\n", PADDING, "RAMDISK_OFFSET", entry.ramdisk_offset);
        fprintf(stderr, "%-*s [%d]\n", PADDING, "RAMDISK_SIZE", entry.ramdisk_size);
        fprintf(stderr, "%-*s [0x%X", PADDING, "BOARD_ID", entry.board_id[0]);
        for (int j = 1; j < VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE - 1; ++j) {
            fprintf(stderr, " 0x%X", entry.board_id[j]);
        }
        fprintf(stderr, " 0x%X]\n\n", entry.board_id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE - 1]);
    }
}

bool ramdisk_table::name_exist(const string &name) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (string_view(it->ramdisk_name) == name) {
            return true;
        }
    }
    return false;
}

size_t ramdisk_table::get_table_length() {
    return entries.size();
}

vendor_ramdisk_table_entry_v4 &ramdisk_table::get_table_entry(unsigned int idx) {
    return entries.at(idx);
}

vector<vendor_ramdisk_table_entry_v4> &ramdisk_table::get_table() {
    return entries;
}

int ramdisk_table_commands(int argc, char *argv[]) {
    char *in_table = argv[0];
    ++argv;
    --argc;
    ramdisk_table table;

    table.load(in_table);

    vector<string> cmdv;

    for (int argidx = 0; argidx < argc; ++argidx) {
        // Reset
        string_view argv_view(argv[argidx]);

        // Split the commands
        auto split_pos = string_view::npos;
        do {
            split_pos = argv_view.find(' ');
            auto substring = argv_view.substr(0, split_pos);
            argv_view.remove_prefix(split_pos == argv_view.size() ? split_pos : split_pos + 1);
            if (substring.starts_with(' ')) {
                continue;
            }
            cmdv.emplace_back(substring);
        } while (split_pos != string_view::npos);

        if (cmdv.empty())
            continue;

        if (cmdv.size() >= 2 && cmdv[0] == "rm"sv) {
            if (cmdv[1] == "--name"sv) {
                string name("\0");
                if (cmdv.size() != 2) {
                    name = cmdv[2];
                }
                if (!table.rm(name))
                    return 1;
            } else if (cmdv[1] == "--entry"sv && cmdv.size() >= 3) {
                uint32_t entry_num;
                auto [ptr, ec] = from_chars(cmdv[2].c_str(), cmdv[2].c_str() + cmdv[2].size(), entry_num, 10);
                if (ec != std::errc() || ptr == cmdv[2].c_str() || entry_num >= table.get_table_length()) {
                    fprintf(stderr, "Entry %s not available.\n", cmdv[2].c_str());
                    return 1;
                } else if (!table.rm(entry_num)) {
                    return 1;
                }
            } else {
                return 1;
            }
            table.dump(in_table);
            table.print();
        } else if (cmdv[0] == "print"sv) {
            table.print();
            return 0;
        } else if (cmdv[0] == "add"sv) {
            int type = -1;
            string name;
            uint32_t id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE] = {};
            for (int i = 1; i < cmdv.size(); i += 2) {
                if (cmdv[i] == "--type"sv && i + 1 < cmdv.size()) {
                    if (cmdv[i+1] == "none"sv)
                        type = VENDOR_RAMDISK_TYPE_NONE;
                    else if (cmdv[i+1] == "dlkm"sv)
                        type = VENDOR_RAMDISK_TYPE_DLKM;
                    else if (cmdv[i+1] == "platform"sv)
                        type = VENDOR_RAMDISK_TYPE_PLATFORM;
                    else if (cmdv[i+1] == "recovery"sv)
                        type = VENDOR_RAMDISK_TYPE_RECOVERY;
                    else
                        return 1;
                } else if (cmdv[i] == "--name"sv && i + 1 < cmdv.size()) {
                    if (cmdv[i+1].size() >= 32) {
                        fprintf(stderr, "Name is to long. Maximal length is 32 characters.\n");
                        return 1;
                    }
                    if (table.name_exist(cmdv[i+1])) {
                        fprintf(stderr,
                                "An entry with name %.*s already exist.\n",
                                VENDOR_RAMDISK_NAME_SIZE,
                                cmdv[i+1].c_str());
                        exit(1);
                    }
                    char file_name[PATH_MAX];
                    ssprintf(file_name, sizeof(file_name), "%s/%s.cpio", VND_RAMDISK_DIR, cmdv[i+1].c_str());
                    if (access(file_name, R_OK)) {
                        fprintf(stderr, "Ramdisk file %s doesn't exist.\n", cmdv[i+1].c_str());
                        exit(1);
                    }
                    name = cmdv[i+1];
                } else if (cmdv[i] == "--copy-id"sv && i + 1 < cmdv.size()) {
                    uint32_t id_idx;
                    auto [ptr, ec] = from_chars(cmdv[i+1].c_str(), cmdv[i+1].c_str() + cmdv[i+1].size(), id_idx, 10);
                    if (ec != std::errc() || ptr == cmdv[i+1].c_str() || id_idx >= table.get_table_length()) {
                        fprintf(stderr, "Entry %s not available.\n", cmdv[i+1].c_str());
                        return 1;
                    } else {
                        memcpy(id, table.get_table_entry(id_idx).board_id, sizeof(id));
                    }
                } else if (cmdv[i] == "--id"sv && i + 2 < cmdv.size()) {
                    uint32_t id_idx;
                    auto result = from_chars(cmdv[i+1].c_str(), cmdv[i+1].c_str() + cmdv[i+1].size(), id_idx, 10);
                    if (result.ec != std::errc() || result.ptr == cmdv[i+1].c_str() || id_idx > 15) {
                        fprintf(stderr, "Id index has to be in range [0..15].\n");
                        return 1;
                    }
                    uint32_t id_val;
                    result = std::from_chars(cmdv[i+2].c_str(), cmdv[i+2].c_str() + cmdv[i+2].size(), id_val, 16);
                    if (result.ec != std::errc() || result.ptr == cmdv[i+2].c_str()) {
                        fprintf(stderr, "Id has to be a 32 bit hex value.\n");
                        return 1;
                    }
                    id[id_idx] = id_val;
                    ++i;
                } else {
                    return 1;
                }
            }
            if (name.empty() || type == -1)
                return 1;
            table.add(name, type, id);
            table.dump(in_table);
            table.print();
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
