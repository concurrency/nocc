<?xml version="1.0" encoding="iso-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<!--
	parsetree-to-occampi.xsl - XML stylesheet for generating occam-pi (ish)
	code from NOCC parse trees
-->

<xsl:output method="text" encoding="UTF-8" media-type="text/plain" />
<xsl:variable name="il" select="0" />

<!--{{{  TEMPLATE mwsync:leaftype-->
<xsl:template match="mwsync/leaftype"><xsl:value-of select="@type" /></xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:namenode-->
<xsl:template match="occampi/namenode"><xsl:value-of select="name/@name" /></xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:fparam-->
<xsl:template match="occampi/fparam"><xsl:apply-templates select="child::*[2]" /><xsl:text> </xsl:text><xsl:apply-templates select="child::*[1]" />
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi-formalparams-->
<xsl:template name="occampi-formalparams"><xsl:for-each select="child::*[2]"><xsl:apply-templates /><xsl:if test="position() != last()">, </xsl:if></xsl:for-each>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi-actualparams-->
<xsl:template name="occampi-actualparams">
<xsl:for-each select="child::*[2]"><xsl:apply-templates /><xsl:if test="position() != last()">, </xsl:if></xsl:for-each>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:instancenode-->
<xsl:template match="occampi/instancenode"><xsl:param name="il" />
+<xsl:value-of select="$il" />+<xsl:apply-templates select="child::*[1]" /> (<xsl:call-template name="occampi-actualparams" />)
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:procdecl-->
<xsl:template match="occampi/procdecl"><xsl:param name="il" />
+<xsl:value-of select="$il" />+PROC <xsl:apply-templates select="child::*[1]" /> (<xsl:call-template name="occampi-formalparams" />)
<xsl:apply-templates select="child::*[3]"><xsl:with-param name="il" select="$il+1" /></xsl:apply-templates>
+<xsl:value-of select="$il" />+:

<xsl:apply-templates select="child::*[4]"><xsl:with-param name="il" select="$il" /></xsl:apply-templates>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:cnode-->
<xsl:template match="occampi/cnode"><xsl:param name="il" />+<xsl:value-of select="$il" />+<xsl:value-of select="@type" /><xsl:text>
</xsl:text><xsl:apply-templates select="child::*[2]"><xsl:with-param name="il" select="$il+1" /></xsl:apply-templates>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:actionnode-->
<xsl:template match="occampi/actionnode"><xsl:param name="il" />+<xsl:value-of select="$il" />+<xsl:choose>
<xsl:when test="@type='SYNC'">SYNC <xsl:apply-templates select="child::*[1]" /><xsl:text>
</xsl:text></xsl:when>
</xsl:choose>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE occampi:vardecl-->
<xsl:template match="occampi/vardecl"><xsl:param name="il" />+<xsl:value-of select="$il" />+<xsl:apply-templates select="child::*[2]" /><xsl:text> </xsl:text><xsl:apply-templates select="child::*[1]" />:
<xsl:apply-templates select="child::*[3]"><xsl:with-param name="il" select="$il" /></xsl:apply-templates>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE mwsync:mwsyncvar-->
<xsl:template match="mwsync/mwsyncvar"><xsl:param name="il" />+<xsl:value-of select="$il" />+<xsl:apply-templates select="child::*[2]" /><xsl:text> </xsl:text><xsl:apply-templates select="child::*[1]" /> (<xsl:apply-templates select="child::*[4]" />):
<xsl:apply-templates select="child::*[3]"><xsl:with-param name="il" select="$il" /></xsl:apply-templates>
</xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE nocc:treedump-->
<xsl:template match="nocc/treedump">-- compiler tree-dump
<xsl:apply-templates><xsl:with-param name="il" select="0" /></xsl:apply-templates></xsl:template>
<!--}}}-->
<!--{{{  TEMPLATE nocc:namespace-->
<xsl:template match="nocc/namespace">-- using NOCC namespace
<xsl:apply-templates /></xsl:template>
<!--}}}-->

</xsl:stylesheet>

