#include "config_obj.hh"

#include <string>
#include <iostream>
#include <fstream>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>

using namespace xercesc;

/*
 * Some helper functions
 */

/* we could perhaps use the MemoryManager functionality
 * instead of going through these hoops */
static std::string
xmlstring_to_string(const XMLCh *xml_str)
{
    char *str_c;
    std::string str;
    str_c = XMLString::transcode(xml_str);
    str = std::string(str_c);
    XMLString::release(&str_c);
    return str;
}

static MemBufInputSource *
get_membuf_inputsource(std::istream &in) {
    XMLCh *buf_id = XMLString::transcode("in_source");
    XMLByte *bytes;
    XMLSize_t size;

    /* code to read stream into the source goes here */
    
    MemBufInputSource *in_source = new MemBufInputSource(bytes, size, buf_id);
    return in_source;
}

/* 'parse' the first identifier from an identifier string into specific
 * components
 *
 * identifier is the string to look at
 * 
 * new_identifier will contain the first node name if the identifier
 * starts with /
 * attribute will contain the attribute name if the identifier starts
 * with @
 * selector_name will contain the attribute or element name if the
 * identifier contains a [selector=value] directive
 * selector_value will contain the attribute or element value to match
 * if the identifier contains a [selector=value] directive
 * rest will contain the remaining identifier string
 */
static bool
read_identifier_part(std::string identifier,
                     std::string &new_identifier,
                     std::string &attribute,
                     std::string &selector_name,
                     std::string &selector_value,
                     std::string &rest)
{
    new_identifier = "";
    attribute = "";
    rest = "";
    if (identifier.length() < 2) {
        return false;
    }
    if (identifier.at(0) == '/') {
        int end_pos = identifier.find_first_of("/@[", 1);
        if (end_pos == std::string::npos) {
            new_identifier = identifier.substr(1);
        } else {
            new_identifier = identifier.substr(1, end_pos - 1);

            if (identifier.length() > end_pos) {
                if (identifier.at(end_pos) == '[') {
                    int sel_eq_pos = identifier.find_first_of('=', end_pos);
                    int sel_end_pos = identifier.find_first_of(']', end_pos);
                    // selector is name=value pair, name can be another identifier
                    // or an attribute of the current.
                    if (sel_eq_pos == std::string::npos || sel_end_pos == std::string::npos) {
                        throw ISC::Config::ConfigError("Bad selector, not in form name=value: " + identifier);
                    }
                    selector_name = identifier.substr(end_pos + 1, sel_eq_pos - end_pos - 1);
                    selector_value = identifier.substr(sel_eq_pos + 1, sel_end_pos - sel_eq_pos - 1);
                    rest = identifier.substr(sel_end_pos + 1);
                } else {
                    rest = identifier.substr(end_pos);
                }
            }
        }
    } else if (identifier.at(0) == '@') {
        attribute = identifier.substr(1);
    }
}

namespace ISC { namespace Config {

    Config::Config(std::string filename)
    {
        parser = new XercesDOMParser();
        read_file(filename);
    }

    Config::Config(std::istream &in)
    {
        parser = new XercesDOMParser();
        read_stream(in);
    }

    /*
     * some simple accessors
     */
    std::string
    Config::get_name() {
        return get_node_name(node);
    }

    std::string
    Config::get_value() {
        return get_node_value(node);
    }

    std::string
    Config::get_value(std::string identifier) {
        DOMNode *n = find_sub_node(node, identifier);
        return get_node_value(n);
    }

    void
    Config::set_value(std::string const &value) {
        set_node_value(node, value);
    }

    void
    Config::set_value(std::string identifier, std::string const &value) {
        DOMNode *n = find_sub_node(node, identifier);
        set_node_value(n, value);
    }

    void
    Config::add_child(std::string name)
    {
        add_node_child(node, name);
    }

    void
    Config::add_child(std::string identifier, std::string name)
    {
        DOMNode *n = find_sub_node(node, identifier);
        add_node_child(n, name);
    }

    void
    Config::read_file(const std::string &filename)
    {
        if (node) {
            throw ConfigError("Configuration already read");
        }
        /* we could set validation scheme and/or namespaces here */
        parser->setValidationScheme(XercesDOMParser::Val_Always);

        ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
        parser->setErrorHandler(errHandler);
        /* this will probably not work until we have a dtd */
        parser->setIncludeIgnorableWhitespace(false);

        try {
            parser->parse(filename.c_str());
        } catch (const XMLException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        } catch (const DOMException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        } catch (const SAXParseException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        }

        node = parser->getDocument()->getDocumentElement();

        /* if we have a DTD that shows what whitespace is ignorable
         * we can omit this step */
        remove_empty_text_nodes(node);

        delete errHandler;
    }

    void
    Config::write_file(const std::string &filename)
    {
        std::ofstream ofile;
        ofile.open(filename.c_str());
        if (!ofile) {
            std::string err = "Unable to open ";
            err += filename + " for writing";
            throw ConfigError(std::string(err));
        }
        serialize(ofile);
        ofile.close();
    }

    void
    Config::read_stream(std::istream &in)
    {
        if (node) {
            throw ConfigError("Configuration already read");
        }
        /* read the stream into an inputsource */
        MemBufInputSource *in_source = get_membuf_inputsource(in);
        
        /* we could set validation scheme and/or namespaces here */
        parser->setValidationScheme(XercesDOMParser::Val_Always);

        ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
        parser->setErrorHandler(errHandler);
        /* this will probably not work until we have a dtd */
        parser->setIncludeIgnorableWhitespace(false);

        try {
            parser->parse(*in_source);
        } catch (const XMLException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        } catch (const DOMException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        } catch (const SAXParseException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        }

        node = parser->getDocument()->getDocumentElement();

        /* if we have a DTD that shows what whitespace is ignorable
         * we can omit this step */
        remove_empty_text_nodes(node);
        delete in_source;

        delete errHandler;
    }

    void
    Config::write_stream(std::ostream &out)
    {
        serialize(out);
    }

    Config *
    Config::get_config_part(std::string const &identifier) {
        Config *config_part = new Config();
        try {
            config_part->node = find_sub_node(node, identifier)->cloneNode(true);
            return config_part;
        } catch (ConfigError& ce) {
            delete config_part;
            throw ce;
        }
    }

    void
    Config::set_config_part(std::string const &identifier, Config *config_part)
    {
        /* should throw exception if node not found, so check
         * not necessary */
        DOMNode *part_node = find_sub_node(node, identifier);
        DOMNode *parent_node = part_node->getParentNode();
        parent_node->removeChild(part_node);
        parent_node->appendChild(config_part->node->cloneNode(true));
    }
        
    /*
     *
     * private functions
     *
     */

    /* my version of xerces has no serialize yet...*/
    /* if we use a newer version or a lib that does have direct
     * serialization support, please replace this */
    void
    Config::serialize_dom_node(std::ostream &out, DOMNode *n, std::string prefix="")
    {
        if (n->getNodeType() == n->TEXT_NODE) {
            out << prefix
                << xmlstring_to_string(n->getNodeValue())
                << std::endl;
        } else if (n->getNodeType() == n->ATTRIBUTE_NODE) {
            out << " "
                << xmlstring_to_string(n->getNodeName())
                << "=\""
                << xmlstring_to_string(n->getNodeValue())
                << "\"";
        } else {
            /* 'normal' node */
            /* name of this node */
            out << prefix << "<" << xmlstring_to_string(n->getNodeName());
            /* attributes */
            DOMNamedNodeMap *attrs = n->getAttributes();
            if (attrs) {
                for (XMLSize_t i = 0; i < attrs->getLength(); i++) {
                    serialize_dom_node(out, attrs->item(i), prefix);
                }
            }
            /* do we have children? */
            if (n->hasChildNodes()) {
                std::string new_prefix = prefix + "\t";
                out << ">" << std::endl;
                DOMNodeList *children = n->getChildNodes();
                for (XMLSize_t i = 0; i < children->getLength(); i++) {
                    serialize_dom_node(out, children->item(i), new_prefix);
                }
                out << prefix << "</" << xmlstring_to_string(n->getNodeName());
            } else {
                out << "/";
            }
            out << ">" << std::endl;
        }
    }

    void
    Config::serialize(std::ostream &out)
    {
        out << "<?xml version=\"1.0\"?>" << std::endl;
        serialize_dom_node(out, node, "");
    }

    void
    Config::remove_empty_text_nodes(DOMNode *n)
    {
        /* iterate over all children and their children,
         * if a child is a text-node containing only whitespace
         * remove it from the parent
         */
        if (!n) { return; }
        DOMNodeList *children = n->getChildNodes();
        DOMNode *c;
        if (children) {
            for (XMLSize_t i = 0; i < children->getLength(); i++) {
                c = children->item(i);
                if (c->getNodeType() == c->TEXT_NODE) {
                    std::string str = xmlstring_to_string(c->getNodeValue());
                    if (str.find_first_not_of("\t\n\r ") == str.npos) {
                        /* ok only whitespace, remove this child */
                        n->removeChild(c);
                        i--;
                    }
                } else {
                    remove_empty_text_nodes(children->item(i));
                }
            }
        }
    }

    std::string
    Config::get_node_name(const DOMNode *n)
    {
        if (n) {
            return xmlstring_to_string(n->getNodeName());
        } else {
            return std::string("<empty/>");
        }
    }
    
    std::string
    Config::get_node_value(const DOMNode *n)
    {
        if (!n) {
            throw ConfigError("null element");
        }
        /* check whether this node has a gettable value */
        if (n->getNodeType() == n->ATTRIBUTE_NODE ||
             (n->getNodeType() == n->ELEMENT_NODE &&
              n->hasChildNodes() &&
              n->getFirstChild() == n->getLastChild() &&
              n->getFirstChild()->getNodeType() == n->TEXT_NODE
             )
            ) {
            return xmlstring_to_string(n->getTextContent());
        } else {
            throw ConfigError("Not a value leaf or attribute " + get_node_name(n));
        }
    }

    void
    Config::set_node_value(DOMNode *n, std::string const &value)
    {
        DOMNode *c;
        XMLCh *xml_str;
        
        if (!n) {
            throw ConfigError("null element");
        }
        /* fail if this is not an attribute or an empty
         * element, or one with only one text child */
        if (n->getNodeType() == n->ATTRIBUTE_NODE ||
             (n->getNodeType() == n->ELEMENT_NODE &&
              !n->hasChildNodes() ||
              (n->getFirstChild() == n->getLastChild() &&
               n->getFirstChild()->getNodeType() == n->TEXT_NODE
              )
             )
            ) {
            xml_str = XMLString::transcode(value.c_str());
            n->setTextContent(xml_str);
            XMLString::release(&xml_str);
        } else {
            throw ConfigError("Not a value leaf or attribute " + get_node_name(n));
        }
    }

    void
    Config::add_node_child(DOMNode *n, std::string const &name)
    {
        XMLCh *xml_str = XMLString::transcode(name.c_str());
        n->appendChild(n->getOwnerDocument()->createElement(xml_str));
        XMLString::release(&xml_str);
    }

    DOMNode *
    Config::find_sub_node(DOMNode *n, std::string const &identifier)
    {
        /* some subset of xpath like stuff */
        /* only / and @ are supported */
        DOMNode *result_n = NULL;
        XMLCh *xml_str;

        if (identifier.length() == 0) {
            throw ConfigError("Empty identifier");
        }
        std::string new_identifier, attribute, selector_name, selector_value, rest;
        read_identifier_part(identifier, new_identifier, attribute, selector_name, selector_value, rest);

        if (attribute.length() > 0) {
            DOMNamedNodeMap *attrs = n->getAttributes();
            xml_str = XMLString::transcode(attribute.c_str());
            result_n = attrs->getNamedItem(xml_str);
            XMLString::release(&xml_str);
            if (!result_n) {
                throw ConfigError("Unknown attribute " + attribute
                                  + " in element " + get_node_name(n));
            }
            return result_n;
        } else if (new_identifier.length() > 0) {
            if (n->hasChildNodes()) {
                DOMNodeList *children = n->getChildNodes();
                for (XMLSize_t i = 0; i < children->getLength(); i++) {
                    /* name of the node must match */
                    if (new_identifier.compare(get_node_name(children->item(i))) == 0) {
                        /* if a selector is present, so
                         * must the value of the selector */
                        if (selector_name.length() > 0) {
                            DOMNode *sel_node = find_sub_node(children->item(i), selector_name);
                            if (selector_value.compare(get_node_value(sel_node)) == 0) {
                                result_n = children->item(i);
                            }
                        } else {
                            result_n = children->item(i);
                        }
                    }
                    /* found one, continue down the tree */
                    if (result_n) {
                        if (rest.length() > 0) {
                            result_n = find_sub_node(result_n, rest);
                        }
                        i = children->getLength();
                    }
                }
            }
            if (!result_n) {
                if (selector_name.length() == 0) {
                    throw ConfigError("Unknown element " + new_identifier);
                } else {
                    throw ConfigError("No element " + new_identifier + " found with " + selector_name + " set to " + selector_value);
                }
            }
            return result_n;
            
        } else {
            throw ConfigError("Bad identifier " + identifier);
        }
                
        return NULL;
    }

    void config_init() {
        try {
            XMLPlatformUtils::Initialize();
        } catch (const XMLException& toCatch) {
            throw ConfigError(xmlstring_to_string(toCatch.getMessage()));
        }
    }

    void config_cleanup() {
        XMLPlatformUtils::Terminate();
    }

}}
