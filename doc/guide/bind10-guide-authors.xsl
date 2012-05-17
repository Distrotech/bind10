<?xml version="1.0" encoding="UTF-8"?>

<!--
   Simple XSL transformation to create a DocBook file from
   authors.xml.
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:dc="http://purl.org/dc/elements/1.1/">

  <xsl:output method="text" />

  <xsl:template name="contributor">
    <xsl:param name="role" />
    <xsl:apply-templates select="dc:contributor[contains(@role, $role)]" />
  </xsl:template>

  <xsl:template match="/dc:authors">
    <xsl:text>&lt;?xml version="1.0" encoding="UTF-8"?&gt;
&lt;!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"
"http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd"&gt;
&lt;chapter id="contributors"&gt;
&lt;title&gt;Contributors&lt;/title&gt;

&lt;para&gt;
The following people have contributed code to BIND 10:
&lt;/para&gt;

&lt;itemizedlist&gt;
</xsl:text>
    <xsl:call-template name="contributor">
      <xsl:with-param name="role" select="'author'"/>
    </xsl:call-template>
    <xsl:text>
&lt;/itemizedlist&gt;

&lt;para&gt;
The following people have helped to document BIND 10:
&lt;/para&gt;

&lt;itemizedlist&gt;
</xsl:text>
    <xsl:call-template name="contributor">
      <xsl:with-param name="role" select="'documenter'"/>
    </xsl:call-template>
    <xsl:text>
&lt;/itemizedlist&gt;

&lt;/chapter&gt;
</xsl:text>

  </xsl:template>

  <xsl:template match="dc:contributor">
    <xsl:text>    &lt;listitem&gt;&lt;simpara&gt;</xsl:text><xsl:apply-templates /><xsl:text>&lt;/simpara&gt;&lt;/listitem&gt;
</xsl:text>
  </xsl:template>

</xsl:stylesheet>
