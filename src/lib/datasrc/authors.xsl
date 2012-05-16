<?xml version="1.0" encoding="UTF-8"?>

<!--

   Simple XSL transformation to create a header file from
   authors.xml. This file was adapted from GIMP.

-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:dc="http://purl.org/dc/elements/1.1/">

  <xsl:output method="text" />

  <xsl:template name="recent-contributor">
    <xsl:param name="role" />
    <xsl:apply-templates select="dc:contributor[contains(@role, $role)]" />
  </xsl:template>

  <xsl:template match="/dc:authors">
<xsl:text>
/* NOTE: This file is auto-generated from authors.xml and authors.xsl. Do not edit it. */

#include "authors.h"
#include &lt;cstdio&gt;

namespace isc {
namespace datasrc {

static const char * const authors[] =
{
</xsl:text>
  <xsl:call-template name="recent-contributor">
    <xsl:with-param name="role" select="'author'"/>
  </xsl:call-template>
<xsl:text>    NULL
};
</xsl:text>

<xsl:text>
static const char * const documenters[] =
{
</xsl:text>
  <xsl:call-template name="recent-contributor">
    <xsl:with-param name="role" select="'documenter'"/>
  </xsl:call-template>
<xsl:text>    NULL
};

const char * const *
get_authors (void)
{
    return authors;
}

const char * const *
get_documenters (void)
{
    return documenters;
}

} // namespace isc::datasrc
} // namespace isc

</xsl:text>
  </xsl:template>

  <xsl:template match="dc:contributor">    "<xsl:apply-templates />",
</xsl:template>

</xsl:stylesheet>
