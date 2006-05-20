<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
                version="1.0">

<xsl:import
href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>

<xsl:template match='replaceable'>
  <xsl:text>\fI&lt;</xsl:text><xsl:apply-templates/><xsl:text>&gt;\fR</xsl:text>
</xsl:template>

<xsl:template match="literal">
  <xsl:text>\fB</xsl:text><xsl:apply-templates/><xsl:text>\fR</xsl:text>
</xsl:template>

<xsl:param name="preferred.mediaobject.role">text</xsl:param>

</xsl:stylesheet>
