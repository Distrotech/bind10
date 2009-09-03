/*
 * Configuration data
 */

#ifndef __NSCONFIG_HH
#define __NSCONFIG_HH 1

#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include <boost/shared_ptr.hpp>


namespace ISC {
    namespace Config {
        /* exceptions */
        class ConfigTypeError { };
        class ConfigNoSuchNameError { };

        /* empty declaration for reference in ConfigEntry,
         * full decl below */
        class ConfigTree;

        /* A ConfigEntry is one (name,type,value) tuple containing
         * one piece of configuration data. Multiple entries can
         * be combined in a ConfigTree */
        class ConfigEntry {
            enum etype { integer, string, boolean };
            typedef bool (*can_change)(const std::string &value);
            typedef void (*on_change)(const ConfigEntry *entry);

            std::string name;
            std::string value;
            etype type;
            ConfigTree *parent;
            std::vector<can_change> can_change_list;
            std::vector<on_change> on_change_list;

        public:
            ConfigEntry(const std::string s) { from_string(s); };
            ConfigEntry(const std::string n, std::string v) : name(n), value(v), parent(NULL) { type = string; };
            ConfigEntry(const std::string n, const char *v) : name(n), value(v), parent(NULL) { type = string; };
            ConfigEntry(const std::string n, bool v) : name(n), parent(NULL) { v?value="true":value="false"; type = boolean; };
            /* this one defined in .cc for the conversion of int */
            ConfigEntry(const std::string n, int v);

            std::string to_string() const;
            /* toXML a string <entry name="" type="">value</entry> */
            std::string toXML() const;

            /* accessors */
            std::string getName() const { return name; };
            void setName(const std::string n) { name = n; };
            ConfigTree *getParent() const { return parent; };
            void setParent(ConfigTree *p) { parent = p; };

            /* Adds a can-change callback function to this entry.
             * Whenever setValue is called, all callbacks set are run.
             * If any of them does not return true, the new value is
             * discarded
             * Callbacks are currently of the form
             * bool can_change(std::string *new_value)
             */
            void addCanChange(can_change c) { can_change_list.push_back(c); };
            /* Adds a on-change callback function to this entry.
             * Whenever setValue has been called (and the setting has
             * succeeded), all callbacks set are called with a reference
             * to the changed ConfigEntry
             * Callbacks are currently of the form
             * void on_change(ConfigEntry *entry)
             */
            void addOnChange(on_change c) { on_change_list.push_back(c); };

            /* Returns the value as a string. If the type of this entry
             * is not a string, it is converted */
            std::string getStringValue() const;
            /* Sets the value to the given string. If the type of this
             * entry is not a string, it is converted
             * Returns false if the value could not be set because
             * of a can_change callback */
            bool setStringValue(std::string);
            /* Returns the integer value of this entry. If the type of
             * the entry is not an integer, a ConfigTypeError exception
             * is thrown */
            int getIntValue() const;
            /* Sets the integer value of this entry. If the type of
             * the entry is not an integer, a ConfigTypeError exception
             * is thrown */
            bool setIntValue(int v);
            /* Returns the boolean value of this entry. If the type of
             * the entry is not an integer, a ConfigTypeError exception
             * is thrown */
            bool getBoolValue() const;
            /* Sets the boolean value of this entry. If the type of
             * the entry is not an integer, a ConfigTypeError exception
             * is thrown */
            bool setBoolValue(bool v);

        private:
            void from_string(const std::string &nodestring);
            bool canChange(std::string v);
        };

        /* A ConfigTree is a collection of ConfigEntry values. Every
         * node in the tree has a map of child nodes and a map of
         * entries. */
        class ConfigTree {
            std::string name;
            ConfigTree *parent;
            std::map<std::string, ConfigTree*> children;
            std::map<std::string, ConfigEntry*> entries;

        public:
            ConfigTree(std::string n = "", ConfigTree *p = NULL) : name(n), parent(p) { };
            ~ConfigTree();

            /* accessors */
            std::string getName() const { return name; };
            void setName(std::string n) { name = n; };
            ConfigTree *getParent() const { return parent; };
            void setParent(ConfigTree *p) { parent = p; };

            /* return the branch with the given name (sub-branches
             * separated by /)
             * throws ConfigNoSuchNameError if it doesn't exist
             */
            ConfigTree *getBranch(std::string name) const;
            void setBranch(ConfigTree *branch);
            ConfigTree *addBranch(std::string name);

            /* Returns the entry with the given name. Tree branches
             * are separated by the character '/' in the name, so
             * system/general/port for instance would return the
             * 'port' configentry in the 'general' branch of the
             * 'system' branch of this tree */
            ConfigEntry *getEntry(std::string name) const;
            /* Add or update the given entry, branch recognition works
             * like in getEntry. The name of the entry is changed if
             * it contains slashes, so that it only contains the last
             * name. Non-existent branches are created */
            void setEntry(ConfigEntry *entry);
            void setEntry(const std::string n, ConfigEntry *entry);
            void setEntry(const std::string n, std::string v) { setEntry(n, new ConfigEntry(n, v)); };
            void setEntry(const std::string n, const char *v) { setEntry(n, new ConfigEntry(n, v)); };
            void setEntry(const std::string n, int v) { setEntry(n, new ConfigEntry(n, v)); };
            void setEntry(const std::string n, bool v) { setEntry(n, new ConfigEntry(n, v)); };

            /* Prints this (sub)tree to the given output stream, with
             * one ConfigEntry per line (prefixed by the full branch
             * names separated by '/' */
            void print(std::ostream &out, const std::string prefix = "") const;
            /* Prints a rudimentary XML-like representation of this
             * tree to the given output stream */
            void printXML(std::ostream &out, const std::string prefix = "") const;
        };

    }
}
#endif
