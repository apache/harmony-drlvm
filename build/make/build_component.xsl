<?xml version="1.0"?>
<!--
    Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
  
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
  
       http://www.apache.org/licenses/LICENSE-2.0
  
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
-->
<!--
Author: Marina V. Goldburt
Version: $Revision: 1.2.2.3 $
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="xml" omit-xml-declaration="yes" indent="yes" />
    
    <xsl:include href="selector.xsl"/>

    <!-- the name of the component can be found in <project name="..."> in the component descriptor -->
    <xsl:variable name="component" select="/project/@name" />
    

    <xsl:template match="property[@name='libname']">
        <xsl:copy-of select="." />
        <property name="{$component}.libname" value="{concat('${','libname}')}" />
    </xsl:template>

    <xsl:template match="property[@name='includes']">
        <xsl:copy-of select="." />
        <property name="{$component}.includes" value="{concat('${','includes}')}" />
    </xsl:template>

    <xsl:template match="property[@name='src']">
        <xsl:copy-of select="." />
        <property name="{$component}.src" value="{concat('${','src}')}" />
    </xsl:template>

    <xsl:template match="property[@name='libdir']">
        <xsl:copy-of select="." />
        <property name="{$component}.libdir" value="{concat('${','libdir}')}" />
    </xsl:template>

    <xsl:template match="property[@name='jardir']">
        <xsl:copy-of select="." />
        <property name="{$component}.jardir" value="{concat('${','jardir}')}" />
    </xsl:template>

    <xsl:template match="property[@name='jarname']">
        <xsl:copy-of select="." />
        <property name="{$component}.jarname" value="{concat('${','jarname}')}" />
    </xsl:template>

</xsl:stylesheet>
