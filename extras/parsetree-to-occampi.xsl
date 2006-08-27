<?xml version="1.0" encoding="iso-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<!--
	parsetree-to-occampi.xsl - XML stylesheet for generating occam-pi (ish)
	code from NOCC parse trees
-->

<xsl:output method="text" encoding="UTF-8" media-type="text/plain" />

<xsl:template match="mwsync/leaftype"><xsl:value-of select="@type" /></xsl:template>

<xsl:template match="occampi/namenode"><xsl:value-of select="name/@name" /></xsl:template>

<xsl:template match="occampi/procdecl">
PROC <xsl:apply-templates select="child::*[1]" /> (<xsl:apply-templates select="child::*[2]" />)
  <xsl:apply-templates select="child::*[3]" />
:

<xsl:apply-templates select="child::*[4]" />
</xsl:template>

<xsl:template match="occampi/vardecl"><xsl:apply-templates select="child::*[2]" /><xsl:text> </xsl:text><xsl:apply-templates select="child::*[1]" />:
<xsl:apply-templates select="child::*[3]" />
</xsl:template>

<xsl:template match="mwsync/mwsyncvar"><xsl:apply-templates select="child::*[2]" /><xsl:text> </xsl:text><xsl:apply-templates select="child::*[1]" />:
<xsl:apply-templates select="child::*[3]" />
</xsl:template>

<xsl:template match="nocc/treedump">-- compiler tree-dump
<xsl:apply-templates /></xsl:template>
<xsl:template match="nocc/namespace">-- using NOCC namespace
<xsl:apply-templates /></xsl:template>
</xsl:stylesheet>

