#pragma once

#include <vector>
#include <memory>
#include "bootimg.hpp"

class ramdisk_table {
public:
    void load(const char *file);
    void rm(const char *name, bool r = false);
    void add(mode_t mode, const char *name, const char *file);
    void dump(const char *file);

protected:
    std::vector<std::unique_ptr<struct vendor_ramdisk_table_entry_v4>> entries;

};

int ramdisk_table_commands(int argc, char *argv[]);