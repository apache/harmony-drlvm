<?xml version="1.0"?>
<!--
    Licensed to the Apache Software Foundation (ASF) under one or more
    contributor license agreements.  See the NOTICE file distributed with
    this work for additional information regarding copyright ownership.
    The ASF licenses this file to You under the Apache License, Version 2.0
    (the "License"); you may not use this file except in compliance with
    the License.  You may obtain a copy of the License at
  
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
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">libname</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='includes']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">includes</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='src']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">src</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='libdir']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">libdir</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='jardir']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">jardir</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='jarname']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">jarname</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template match="property[@name='srcjarname']">
        <xsl:copy-of select="." />
        <xsl:call-template name="insert-property">
            <xsl:with-param name="suffix">srcjarname</xsl:with-param>
        </xsl:call-template>
    </xsl:template>

    <xsl:template name="insert-property">
        <xsl:param name="suffix"/>
        <xsl:element name="property">
            <xsl:attribute name="name">
                <xsl:value-of select="$component"/>
                <xsl:text>.</xsl:text>
                <xsl:value-of select="$suffix"/>
            </xsl:attribute>
            <xsl:attribute name="value">
                <xsl:text>${</xsl:text>
                <xsl:value-of select="$suffix"/>
                <xsl:text>}</xsl:text>
            </xsl:attribute>
        </xsl:element>
    </xsl:template>
    

    
</xsl:stylesheet>
