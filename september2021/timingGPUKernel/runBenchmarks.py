#!/usr/bin/python

# MIT License
#
# Copyright (c) 2021, Juan Fumero
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

from os.path import exists
from subprocess import Popen, PIPE
import sys
import sqlite3
import re
import argparse

class BenchmarkDBHandler:
    
    def __init__(self, dbName):
        self.dbName = dbName
        self.conn = None
    
    def createDataBase(self):
        self.conn = sqlite3.connect(self.dbName)
        print("Opened database successfully")
        self.conn.execute('''
            CREATE TABLE KERNEL_PERFORMANCE
                (SIZE INT NOT NULL, 
                 NAME TEXT NOT NULL, 
                 TIME INT NOT NULL);
            ''')
        print("Table created successfully")
        self.conn.close()
   
    def openDB(self):
        self.conn = sqlite3.connect(dbName)
        return self.conn

    def closeDB(self):
        self.conn.close()

    def insertRowInDataBase(self, size, name, timer):
        name = "'" + name + "'"
        self.conn.execute("INSERT INTO KERNEL_PERFORMANCE(SIZE, NAME, TIME) \
            VALUES(" + str(size) + "," + str(name) + "," + str(timer) + ")")
        self.conn.commit()

    def queryDB(self):
        cursor = self.conn.execute("SELECT size, name, avg(time), Count(*) from KERNEL_PERFORMANCE group by size, name ORDER BY name")
        print("SIZE NAME TIMER COUNT")
        for row in cursor:
            print(row[0], row[1], row[2], row[3])
     

    def queryAllDB(self):
        cursor = self.conn.execute("SELECT size, name, time from KERNEL_PERFORMANCE")
        for row in cursor:
            print("SIZE = ", row[0])
            print("NAME = ", row[1])
            print("TIMER = ", row[2], "\n")
         
    def checkDBFileExists(self):
        file_exists = exists(self.dbName)
        return file_exists


class CommandBenchmark:

    def __init__(self, dbHandler):
        self.dbHandler = dbHandler

    def runCommand(self, command, size):
        p = Popen([command, str(size)], stdin=PIPE, stdout=PIPE, stderr=PIPE, encoding='utf8')
        out, err = p.communicate()
        returncode = p.returncode
        return out,err,returncode

    def runBenchmarksKernelTimer(self):

        sizes = [32, 64, 128, 256, 512, 1024, 2048]
        max_iterations = 10

        command = "./mxm"
        for size in sizes:
            for i in range(max_iterations):
                print("Running with Size: " + str(size) + " -- #iteration: " + str(i))
                out, err, returncode = self.runCommand(command, size)
                while (returncode != 0):
                    out, err, returncode = self.runCommand(command, size)
            
                #print(out)

                m = re.search(r"GPU-KERNEL = (\d+)", out)
                timer = int(m.group(1))
                self.dbHandler.insertRowInDataBase(size, 'GPU-KERNEL', timer)

                m = re.search(r"PARALLEL = (\d+)", out)
                timer = int(m.group(1))
                self.dbHandler.insertRowInDataBase(size, 'PARALLEL', timer)

                m = re.search(r"SEQ = (\d+)", out)
                timer = int(m.group(1))
                self.dbHandler.insertRowInDataBase(size, 'SEQ', timer)

    def runAll(self):
        b = self.dbHandler.checkDBFileExists()
        if (b == False):
            self.dbHandler.createDataBase()

        self.dbHandler.openDB()
    
        self.runBenchmarksKernelTimer()

        print("============================")
        self.dbHandler.queryDB()
        self.dbHandler.closeDB() 


def parseArguments():
    """ Parse command line arguments """
    parser = argparse.ArgumentParser(description='Tool run benchmarks and query database')
    parser.add_argument('--version', action="store_true", dest="version", default=False, help="Print version")
    parser.add_argument("--query", "-q", action="store_true", dest="queryDataBase", default=False, help="Query Data Base")
    parser.add_argument("--performance", "-p", action="store_true", dest="queryPerformance", default=False, help="Query Data Base - Performance Metrics")
    parser.add_argument("--run", "-r", action="store_true", dest="runBenchmarks", default=False, help="Run Benchmarks and store results in the DB")
    args = parser.parse_args()
    return args

if __name__ == "__main__":

    dbName = "performanceTableKernel.db"
    args = parseArguments()
        
    if (args.queryDataBase):
        dbHandler = BenchmarkDBHandler(dbName)
        dbHandler.openDB()
        dbHandler.queryAllDB()
        dbHandler.closeDB()

    elif (args.runBenchmarks):
        dbHandler = BenchmarkDBHandler(dbName)
        benchmarks = CommandBenchmark(dbHandler)
        benchmarks.runAll()

    elif (args.version):
        print("0.1")

    elif (args.queryPerformance):
        dbHandler = BenchmarkDBHandler(dbName)
        dbHandler.openDB()
        dbHandler.queryDB()
        dbHandler.closeDB()
    
    else:
        dbHandler = BenchmarkDBHandler(dbName)
        dbHandler.openDB()
        dbHandler.queryDB()
        dbHandler.closeDB()


