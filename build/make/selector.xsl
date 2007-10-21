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
Author: Marina V. Goldburt, Sergey V. Dmitriev
Version: $Revision: 1.1.2.2 $
-->
<!--
    The xsl stylesheet prints the content of component descriptors to the output xml
    choosing the only select tags that match build configuration
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="xml" omit-xml-declaration="yes" indent="yes" />

    <xsl:param name="os" />
    <xsl:param name="osfamily" />
    <xsl:param name="cfg" />
    <xsl:param name="arch" />
    <xsl:param name="cxx" />
    <xsl:param name="hynosig" />

    <xsl:template match="/">
        <xsl:apply-templates />
    </xsl:template>

    <!-- the  template prints the child of the select tag only if the
         select matches build configuration --> 
    <xsl:template match="select">
        <xsl:if test="(contains(@hynosig,$hynosig) or not(@hynosig)) and (contains(@osfamily,$osfamily) or not(@osfamily))and (contains(@os,$os) or not(@os))and (contains(@cfg,$cfg) or not(@cfg)) and (contains(@arch,$arch) or not(@arch)) and (contains(concat(' ',@cxx), concat(' ',$cxx)) or not(@cxx))">
            <xsl:apply-templates select="*" />
        </xsl:if>
    </xsl:template>

    <xsl:template match="*">
        <xsl:copy>
            <xsl:copy-of select="@*" />
            <xsl:apply-templates />
        </xsl:copy>
    </xsl:template>

    <xsl:template match="@*">
        <xsl:copy-of select="." />
    </xsl:template>
    

</xsl:stylesheet>
