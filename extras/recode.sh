#! /bin/bash

./denamespace $1 | xsltproc ./parsetree-to-occampi.xsl - | ./reindent

