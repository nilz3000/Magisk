#pragma once

#include <vector>
#include <string>
#include "bootimg.hpp"

class ramdisk_table {
public:
    bool rm(const std::string &name);
    bool rm(uint32_t id);
    void add(const std::string &name, int type, const uint32_t *id);
    void dump(const std::string &file);
    void load(const std::string &file);
    void print();
    bool name_exist(const std::string &name);
    size_t get_table_length();
    vendor_ramdisk_table_entry_v4 &get_table_entry(unsigned int idx);
    std::vector<vendor_ramdisk_table_entry_v4> &get_table();

protected:
    std::vector<vendor_ramdisk_table_entry_v4> entries;

};

int ramdisk_table_commands(int argc, char *argv[]);

class Type2Name {
public:
    const char *operator[](uint32_t fmt);
};

extern Type2Name type2name;
