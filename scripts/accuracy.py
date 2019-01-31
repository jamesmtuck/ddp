#!/usr/bin/python

import sqlite3
import sys
import os
import numpy as np
from math import sqrt
from optparse import OptionParser

global conn
global cur

def getVector(name,table):
    cur.execute('select begin,end from files where app="%s";'%name);
    r = None
    for row in cur:
        r = row
    if r==None:
        return []
    cur.execute('select refid,count from %s where filename like (select name from files where app like "%s");' % (table,name))
    h = {}
    for row in cur:
        h[row[0]] = row[1]
    v = []
    for i in range(r[0],r[1]):
        if h.has_key(i):
            v.append(h[i])
        else:
            v.append(0)            
    return v


def getNAED(name,table,norm):
    v = getVector(name,table)
    vht = getVector(name,norm)    
    v1 = np.array(v,float) 
    v2 = np.array(vht,float)     
    md = []
    if len(v1)==0 and len(v2)==0:
        return "-none-"
    elif len(v1) != len(v2):
        return "-error-"
    for i in range(0,len(v1)):
        m = max(v1[i],v2[i])
        mm = min(v1[i],v2[i])
        if m==0 or m==mm:
            md.append(1)
        else:
            md.append( (m-mm)*(m-mm) )        
    vmd = np.array(md,float)
    diff = (v1-v2)
    prod = sum( ((diff*diff)/vmd) )
    return str(sqrt(prod/len(v1))/sqrt(len(v1)))

def printHeader(tables,width):
    s = ""
    for t in tables:
        s += "%s" % t.center(width) 
    print "%s%s" % ("Application".ljust(width),s)

def printData(tables,data,width):
    printHeader(tables,width)
    for row in data:
        s = ""
        s += '%s' % row[0].ljust(width)
        for col in row[1:]:
            s += col[0:7].center(width)
        print s
            

parser = OptionParser()
parser.add_option("-d","--db",action="store",dest="db",default="ddp.db",
            help="name of database to use")

parser.add_option("-t","--tables",action="store",dest="tables",default="feedback",
            help="name of table to measure accuracy of")

parser.add_option("-n","--norm",action="store",dest="norm",default="perf_feedback",
            help="name of table to normalize accuracy against")


(options,args) = parser.parse_args(sys.argv[1:])
tables = options.tables.split(',')
norm = options.norm

conn = sqlite3.connect(options.db)
cur = conn.cursor();

Apps = []

if len(args)==0:
    cur.execute('select app from files;');
    apps={}
    for row in cur:
        apps[ row[0] ] = 0
    Apps = apps.keys()
else:
    Apps = args[:]

Data = []
for a in Apps[:]:
    d = [a]
    for t in tables:
        d.append(getNAED(a,t,norm))
    Data.append(d)

printData(tables,Data,20)


