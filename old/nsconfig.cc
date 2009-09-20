/*
 * Configuration data
 */

#include "nsconfig.hh"
#include "util.hh"

using namespace ISC::Config;

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <vector>
#include <map>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>

/*
 * Some utility functions
 */

/* Converts an integer to a string representation */
static std::string
IntToString(int i) {
    std::stringstream ss;
    ss << i;
    return ss.str();
}

/* Splits the given '/'-separated name. The string up to the last
 * / is placed in subtree. The part after the last / in entry.
 * If there are no / characters in the string, the complete string is
 * placed in entry, and the function returns false. Otherwise, it
 * returns true.
 */
static bool
SplitTreeName(const std::string &str,
              std::string &subtree,
              std::string &entry)
{
    std::string::size_type pos = str.find_last_of('/');
    if (pos != std::string::npos) {
        subtree = str.substr(0, pos);
        entry = str.substr(pos + 1);
        return true;
    } else {
        entry = str;
        return false;
    }
}


/*
 * Functions for config entries
 */

ConfigEntry::ConfigEntry(std::string n, int v) : name(n)
{
    type = integer;
    value = IntToString(v);
    parent = NULL;
}

void
ConfigEntry::from_string(const std::string &entrystring)
{
    /* the string is either only a name (i.e. no value), or
     * it is a whitespace-separated tuple (name, type, value) */
    std::stringstream ss(entrystring), vs;
    ss >> name;

    std::string typestr;
    ss >> typestr;
    if (boost::iequals(typestr, "integer")) {
        type = integer;
    } else if (boost::iequals(typestr, "string")) {
        type = string;
    } else if (boost::iequals(typestr, "boolean")) {
        type = boolean;
    }
    /* could do with some value checking right here */
    std::string valuestr;

    /* we've tokenized too much, concat the rest of the elements as
     * one value */
    while (ss >> valuestr) { vs << " " << valuestr; };
    value = vs.str();
    parent = NULL;
}

std::string
ConfigEntry::to_string() const
{
    std::stringstream ss;
    ss << name << "\t";
    switch(type) {
        case integer:
            ss << "integer";
            break;
        case string:
            ss << "string";
            break;
        case boolean:
            ss << "boolean";
            break;
        default:
            // raise exception?
            ss << "unknown";
    }
    ss << "\t";
    ss << value;

    return ss.str();
}

std::string
ConfigEntry::toXML() const
{
    std::stringstream ss;
    ss << "<entry name=\"" << name << "\" type =\"";
    switch(type) {
        case integer:
            ss << "integer";
            break;
        case string:
            ss << "string";
            break;
        case boolean:
            ss << "boolean";
            break;
        default:
            // raise exception?
            ss << "unknown";
    }
    ss << "\">";
    ss << value;
    ss << "</entry>";
    return ss.str();
}

std::string
ConfigEntry::getStringValue() const
{
    /* internal representation is already string atm */
    return value;
}

bool
ConfigEntry::setStringValue(std::string v)
{
    std::vector<on_change>::iterator oci;
    if (!canChange(v)) {
            return false;
    }
    switch (type) {
        case string:
            value = v;
            break;
        case integer:
            if (atoi(v.c_str())) {
                value = v;
            } else {
                throw ConfigTypeError();
            }
            break;
        case boolean:
            if (boost::iequals(v, "true") ||
                boost::iequals(v, "false")) {
                    value = v;
            }
            break;
        default:
            throw ConfigTypeError();
    }
    for (oci = on_change_list.begin(); oci != on_change_list.end(); oci++) {
        (*oci)(this);
    }
    return true;
}

int
ConfigEntry::getIntValue() const
{
    if (type == integer) {
        return atoi(value.c_str());
    } else {
        throw ConfigTypeError();
    }
}

bool
ConfigEntry::setIntValue(int v) 
{
    std::string sv;
    if (type == integer) {
        sv = IntToString(v);
        if (!canChange(sv)) {
                return false;
        }
        value = sv;
    } else {
        throw ConfigTypeError();
    }
    return true;
}

bool
ConfigEntry::getBoolValue() const
{
    if (type == boolean) {
        if (boost::iequals(value, "true")) {
            return true;
        } else {
            return false;
        }
    } else {
        throw ConfigTypeError();
    }
}

bool
ConfigEntry::setBoolValue(bool v)
{
    std::string sv;
    if (type == boolean) {
        v?sv="true":sv="false";
        if (!canChange(sv)) {
                return false;
        }
        value = sv;
    } else {
        throw ConfigTypeError();
    }
    return true;
}

bool
ConfigEntry::canChange(std::string v)
{
    /* check the list of registered can_change callbacks to see
     * if we may change into the value provided. if one of them
     * returns false, abort */
    std::vector<can_change>::iterator cci;
    for (cci = can_change_list.begin(); cci != can_change_list.end(); cci++) {
        if (!(*cci)(v)) {
            /* should we feedback with error here?
             * or throw an exception (not being able to change a
             * value is not really an exception...)
             */
            return false;
        }
    }
    return true;
}


/*
 * Functions for the tree structure
 */
ConfigTree::~ConfigTree()
{
    /* use pairs to iterate over maps */
    std::pair<std::string, ConfigEntry*> ep;
    std::pair<std::string, ConfigTree*> tp;
    BOOST_FOREACH(ep, entries) {
        delete ep.second;
    }
    BOOST_FOREACH(tp, children) {
        delete tp.second;
    }
}


ConfigTree *
ConfigTree::getBranch(std::string name) const
{
    /* we're not modifying, but are changing the pointer
     * we are using ourselves */
    ConfigTree *tree = (ConfigTree *) this;
    std::vector<std::string> nameparts;
    Tokenize(name, nameparts, "/");
    
    BOOST_FOREACH(std::string s, nameparts) {
        if (tree->children.count(s) > 0) {
            tree = tree->children[s];
        } else {
            throw ConfigNoSuchNameError();
        }
    }
    return tree;
}

void
ConfigTree::setBranch(ConfigTree *branch)
{
    if (children.count(branch->getName()) == 0) {
        delete children[branch->getName()];
    }
    // check if a parent already exists and remove it as a child there?
    branch->setParent(this);
    children[branch->getName()] = branch;
}

ConfigTree *
ConfigTree::addBranch(std::string name)
{
    ConfigTree *tree = this;
    
    std::vector<std::string> nameparts;
    Tokenize(name, nameparts, "/");
    
    BOOST_FOREACH(std::string s, nameparts) {
        
        if (tree->children.count(s) > 0) {
            tree = tree->children[s];
        } else {
            tree = new ConfigTree(s, tree);
            tree->getParent()->children[s] = tree;
        }
    }
    return tree;
}

ConfigEntry *
ConfigTree::getEntry(std::string name) const
{
    std::string subtreename;
    std::string entryname;
    ConfigEntry *entry;
    ConfigTree *subtree = (ConfigTree *) this;

    if (SplitTreeName(name, subtreename, entryname)) {
        subtree = this->getBranch(subtreename);
    }
    if (subtree->entries.count(entryname) > 0) {
        entry = subtree->entries[entryname];
    } else {
        throw ConfigNoSuchNameError();
    }
    return entry;
}

void
ConfigTree::setEntry(ConfigEntry *entry)
{
    if (entries.count(entry->getName()) == 0) {
        delete entries[entry->getName()];
    }
    // check if a parent already exists and remove it as a child there?
    entries[entry->getName()] = entry;
}

void
ConfigTree::setEntry(std::string n, ConfigEntry *entry)
{
    std::string subtreename;
    std::string entryname;
    ConfigTree *tree = this;
    if (SplitTreeName(n, subtreename, entryname)) {
        tree = addBranch(subtreename);
        entry->setName(entryname);
        entry->setParent(tree);
    } else {
        entry->setParent(this);
    }
    tree->setEntry(entry);
}

void
ConfigTree::print(std::ostream &out, const std::string prefix) const
{
    std::pair<std::string, ConfigEntry*> ep;
    std::pair<std::string, ConfigTree*> tp;
    std::string newprefix = prefix;
    if (newprefix.length() > 0) {
        newprefix += "/";
    }
    newprefix += getName();
    BOOST_FOREACH(ep, entries) {
        out << newprefix << "/" << ep.second->to_string() << std::endl;
    }
    BOOST_FOREACH(tp, children) {
        tp.second->print(out, newprefix);
    }
}

void
ConfigTree::printXML(std::ostream &out, const std::string prefix) const
{
    std::pair<std::string, ConfigEntry*> ep;
    std::pair<std::string, ConfigTree*> tp;
    std::string newprefix;

    if (getName().length() > 0) {
        out << prefix << "<" << getName() << ">" << std::endl;
    } else {
        out << prefix << "<config>" << std::endl;
    }
    newprefix = prefix + "\t";
    BOOST_FOREACH(ep, entries) {
        out << newprefix << ep.second->toXML() << std::endl;
    }
    BOOST_FOREACH(tp, children) {
        tp.second->printXML(out, newprefix);
    }
    if (getName().length() > 0) {
        out << prefix << "</" << getName() << ">" << std::endl;
    } else {
        out << prefix << "</config>" << std::endl;
    }
}

