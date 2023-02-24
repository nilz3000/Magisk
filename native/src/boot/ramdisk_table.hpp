#pragma once

#include <vector>
#include <memory>
#include "bootimg.hpp"

class ramdisk_table {
public:
    void rm(const char *name);
    void add(const char *name, int type, const uint32_t *id);
    void dump(const char *file);
    void load(const char *file);
    void print();
    bool name_exist(const char *name);
    size_t get_table_length();
    std::unique_ptr<struct vendor_ramdisk_table_entry_v4> &get_table_entry(unsigned int idx);

protected:
    std::vector<std::unique_ptr<struct vendor_ramdisk_table_entry_v4>> entries;

};

int ramdisk_table_commands(int argc, char *argv[]);

class Type2Name {
public:
    const char *operator[](uint32_t fmt);
};

extern Type2Name type2name;