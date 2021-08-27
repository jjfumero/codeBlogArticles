#!/usr/bin/python

from os.path import exists
from subprocess import Popen, PIPE
import sqlite3
import re
import argparse

class BenchmarkDBHandler:
    
    def __init__(self, dbName):
        self.dbName = dbName
        self.conn = None
    
    def createDataBase(self):
        self.conn = sqlite3.connect(self.dbName)
        print("Opened database successfully: " + str(self.dbName))
        self.conn.execute('''
            CREATE TABLE COPY_PERFORMANCE
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
        self.conn.execute("INSERT INTO COPY_PERFORMANCE(SIZE, NAME, TIME) \
            VALUES(" + str(size) + "," + str(name) + "," + str(timer) + ")")
        self.conn.commit()

    def queryDB(self):
        cursor = self.conn.execute("SELECT size, name, avg(time), Count(*) from COPY_PERFORMANCE group by size, name ORDER BY name")
        print("SIZE NAME TIMER COUNT")
        for row in cursor:
            print(row[0], row[1], row[2], row[3])
     

    def queryAllDB(self):
        cursor = self.conn.execute("SELECT size, name, time from COPY_PERFORMANCE")
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

    def runBenchmark(self):

        sizeInBytes = []
        s = 512
        for i in range(22):
            sizeInBytes.append(s) 
            s = s * 2

        print(sizeInBytes)

        command = "./timeDataTransfers"
        for size in sizeInBytes:
            print("Running with Size: " + str(size))
            out, err, returncode = self.runCommand(command, size)
            while (returncode != 0):
                out, err, returncode = self.runCommand(command, size)     


            m = re.findall(r"SHARED: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Shared->Shared', timer)       

            m = re.findall(r"Heap->Device: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Heap->Device', timer)


            m = re.findall(r"Device->Heap: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Device->Heap', timer)


            m = re.findall(r"DEVICE->DEVICE: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Device->Device', timer)


            m = re.findall(r"HOST->DEVICE: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Host->Device', timer)        


            m = re.findall(r"DEVICE->HOST: (\d+)", out)
            
            if (m != None):
                for match in m:
                    timer = int(match)
                    self.dbHandler.insertRowInDataBase(size, 'Device->Host', timer)


    def runAll(self):
        b = self.dbHandler.checkDBFileExists()
        if (b == False):
            self.dbHandler.createDataBase()

        self.dbHandler.openDB()
    
        self.runBenchmark()

        print("============================")
        self.dbHandler.queryDB()
        self.dbHandler.closeDB() 


def parseArguments():
    """ Parse command line arguments """
    parser = argparse.ArgumentParser(description='Tool run benchmarks and query database')
    parser.add_argument('--db', dest="dbName", help='Data Base File Name', required=True)
    parser.add_argument('--version', action="store_true", dest="version", default=False, help="Print version")
    parser.add_argument("--query", "-q", action="store_true", dest="queryDataBase", default=False, help="Query Data Base")
    parser.add_argument("--performance", "-p", action="store_true", dest="queryPerformance", default=False, help="Query Data Base - Performance Metrics")
    parser.add_argument("--run", "-r", action="store_true", dest="runBenchmarks", default=False, help="Run Benchmarks and store results in the DB")
    args = parser.parse_args()
    return args

if __name__ == "__main__":

    args = parseArguments()

    if (args.dbName):
        dbName = args.dbName


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
