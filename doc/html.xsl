<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                exclude-result-prefixes="#default">

<!-- <xsl:import
href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/> -->
<xsl:import
href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>
 
<xsl:variable name="toc.max.depth">2</xsl:variable>
<xsl:variable name="html.stylesheet">style.css</xsl:variable>
<xsl:template name="user.head.content"><link rel="shortcut icon" href="favicon.ico"/></xsl:template>
<!--<xsl:variable name="admon.graphics">0</xsl:variable>-->
<!--<xsl:variable name="chunk.section.depth">1</xsl:variable>-->
<!--<xsl:param name="generate.toc">book toc,title</xsl:param> -->

</xsl:stylesheet>
