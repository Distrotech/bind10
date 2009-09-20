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
XMLStringToString(const XMLCh *xml_str)
{
    char *str_c;
    std::string str;
    str_c = XMLString::transcode(xml_str);
    str = std::string(str_c);
    XMLString::release(&str_c);
    return str;
}

static MemBufInputSource *
getMemBufInputSource(std::istream &in) {
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
        readFile(filename);
    }

    Config::Config(std::istream &in)
    {
        parser = new XercesDOMParser();
        readStream(in);
    }

    /*
     * some simple accessors
     */
    std::string
    Config::getName() {
        return getNodeName(node);
    }

    std::string
    Config::getValue() {
        return getNodeValue(node);
    }

    std::string
    Config::getValue(std::string identifier) {
        DOMNode *n = findSubNode(node, identifier);
        return getNodeValue(n);
    }

    void
    Config::setValue(std::string const &value) {
        setNodeValue(node, value);
    }

    void
    Config::setValue(std::string identifier, std::string const &value) {
        DOMNode *n = findSubNode(node, identifier);
        setNodeValue(n, value);
    }

    void
    Config::addChild(std::string name)
    {
        addNodeChild(node, name);
    }

    void
    Config::addChild(std::string identifier, std::string name)
    {
        DOMNode *n = findSubNode(node, identifier);
        addNodeChild(n, name);
    }

    void
    Config::readFile(const std::string &filename)
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
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        } catch (const DOMException& toCatch) {
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        } catch (const SAXParseException& toCatch) {
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        }

        node = parser->getDocument()->getDocumentElement();

        /* if we have a DTD that shows what whitespace is ignorable
         * we can omit this step */
        removeEmptyTextNodes(node);

        delete errHandler;
    }

    void
    Config::writeFile(const std::string &filename)
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
    Config::readStream(std::istream &in)
    {
        if (node) {
            throw ConfigError("Configuration already read");
        }
        /* read the stream into an inputsource */
        MemBufInputSource *in_source = getMemBufInputSource(in);
        
        /* we could set validation scheme and/or namespaces here */
        parser->setValidationScheme(XercesDOMParser::Val_Always);

        ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
        parser->setErrorHandler(errHandler);
        /* this will probably not work until we have a dtd */
        parser->setIncludeIgnorableWhitespace(false);

        try {
            parser->parse(*in_source);
        } catch (const XMLException& toCatch) {
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        } catch (const DOMException& toCatch) {
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        } catch (const SAXParseException& toCatch) {
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        }

        node = parser->getDocument()->getDocumentElement();

        /* if we have a DTD that shows what whitespace is ignorable
         * we can omit this step */
        removeEmptyTextNodes(node);
        delete in_source;

        delete errHandler;
    }

    void
    Config::writeStream(std::ostream &out)
    {
        serialize(out);
    }

    Config *
    Config::getConfigPart(std::string const &identifier) {
        Config *config_part = new Config();
        try {
            config_part->node = findSubNode(node, identifier)->cloneNode(true);
            return config_part;
        } catch (ConfigError ce) {
            delete config_part;
            throw ce;
        }
    }

    void
    Config::setConfigPart(std::string const &identifier, Config *config_part)
    {
        /* should throw exception if node not found, so check
         * not necessary */
        DOMNode *part_node = findSubNode(node, identifier);
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
    Config::serializeDOMNode(std::ostream &out, DOMNode *n, std::string prefix="")
    {
        if (n->getNodeType() == n->TEXT_NODE) {
            out << prefix
                << XMLStringToString(n->getNodeValue())
                << std::endl;
        } else if (n->getNodeType() == n->ATTRIBUTE_NODE) {
            out << " "
                << XMLStringToString(n->getNodeName())
                << "=\""
                << XMLStringToString(n->getNodeValue())
                << "\"";
        } else {
            /* 'normal' node */
            /* name of this node */
            out << prefix << "<" << XMLStringToString(n->getNodeName());
            /* attributes */
            DOMNamedNodeMap *attrs = n->getAttributes();
            if (attrs) {
                for (XMLSize_t i = 0; i < attrs->getLength(); i++) {
                    serializeDOMNode(out, attrs->item(i), prefix);
                }
            }
            /* do we have children? */
            if (n->hasChildNodes()) {
                std::string new_prefix = prefix + "\t";
                out << ">" << std::endl;
                DOMNodeList *children = n->getChildNodes();
                for (XMLSize_t i = 0; i < children->getLength(); i++) {
                    serializeDOMNode(out, children->item(i), new_prefix);
                }
                out << prefix << "</" << XMLStringToString(n->getNodeName());
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
        serializeDOMNode(out, node, "");
    }

    void
    Config::removeEmptyTextNodes(DOMNode *n)
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
                    std::string str = XMLStringToString(c->getNodeValue());
                    if (str.find_first_not_of("\t\n\r ") == str.npos) {
                        /* ok only whitespace, remove this child */
                        n->removeChild(c);
                        i--;
                    }
                } else {
                    removeEmptyTextNodes(children->item(i));
                }
            }
        }
    }

    std::string
    Config::getNodeName(const DOMNode *n)
    {
        if (n) {
            return XMLStringToString(n->getNodeName());
        } else {
            return std::string("<empty/>");
        }
    }
    
    std::string
    Config::getNodeValue(const DOMNode *n)
    {
        if (!n) {
            throw ConfigError("null element");
        }
        if (n->getNodeType() == n->ATTRIBUTE_NODE ||
             (n->getNodeType() == n->ELEMENT_NODE &&
              !n->hasChildNodes() ||
              (n->getFirstChild() == n->getLastChild() &&
               n->getFirstChild()->getNodeType() == n->TEXT_NODE
              )
             )
            ) {
            return XMLStringToString(n->getTextContent());
        } else {
            throw ConfigError("Not a value leaf or attribute " + getNodeName(n));
        }
    }

    void
    Config::setNodeValue(DOMNode *n, std::string const &value)
    {
        DOMNode *c;
        XMLCh *xml_str;
        
        if (!n) {
            throw ConfigError("null element");
        }
        /* fail if this is not an attribute or an
         * element with only one text child */
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
            throw ConfigError("Not a value leaf or attribute " + getNodeName(n));
        }
    }

    void
    Config::addNodeChild(DOMNode *n, std::string const &name)
    {
        XMLCh *xml_str = XMLString::transcode(name.c_str());
        n->appendChild(n->getOwnerDocument()->createElement(xml_str));
        XMLString::release(&xml_str);
    }

    DOMNode *
    Config::findSubNode(DOMNode *n, std::string const &identifier)
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
                                  + " in element " + getNodeName(n));
            }
            return result_n;
        } else if (new_identifier.length() > 0) {
            if (n->hasChildNodes()) {
                DOMNodeList *children = n->getChildNodes();
                for (XMLSize_t i = 0; i < children->getLength(); i++) {
                    /* name of the node must match */
                    if (new_identifier.compare(getNodeName(children->item(i))) == 0) {
                        /* if a selector is present, so
                         * must the value of the selector */
                        if (selector_name.length() > 0) {
                            DOMNode *sel_node = findSubNode(children->item(i), selector_name);
                            if (selector_value.compare(getNodeValue(sel_node)) == 0) {
                                result_n = children->item(i);
                            }
                        } else {
                            result_n = children->item(i);
                        }
                    }
                    /* found one, continue down the tree */
                    if (result_n) {
                        if (rest.length() > 0) {
                            result_n = findSubNode(result_n, rest);
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
            throw ConfigError(XMLStringToString(toCatch.getMessage()));
        }
    }

    void config_cleanup() {
        XMLPlatformUtils::Terminate();
    }

}}
