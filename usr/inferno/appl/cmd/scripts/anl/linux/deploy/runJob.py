#!/usr/bin/env python


import socket
import sys
import os
import getopt
import getpass
import code
import readline
import atexit
sys.path.append ('./py9p/py9p')
import py9p

remoteURL = ""
#baseURL = "/cmd2/local/"
#baseURL = "/task/remote/"
baseURL = "/csrv/local/"
class XCPU3Client():
    
    sessionID = None
    DEBUG = False
    bufsize = 512
    
    def __init__(self, srv, port, authmode='none', user='', passwd=None, authsrv=None, chatty=0, key=None):
        self.sessionID = None
        self.bufSize = 512
        self.DEBUG = False
        
        self.extraLink = self.get9pClient(srv, port, authmode, user, passwd, authsrv, chatty, key)
        self.IOLink = self.get9pClient(srv, port, authmode, user, passwd, authsrv, chatty, key)
        self.ctlLink = self.get9pClient(srv, port, authmode, user, passwd, authsrv, chatty, key)

    def dPrint (self, msg):
        if (self.DEBUG):
            print msg

    def get9pClient (self, srv, port, authmode='none', user='', passwd=None, authsrv=None, chatty=0, key=None) :
        privkey = None
           
        sock = socket.socket(socket.AF_INET)
        try:
            sock.connect((srv, port),)
        except socket.error,e:
            print "%s: %s" % (srv, e.args[1])
            raise
        return py9p.Client(py9p.Sock(sock, 0, chatty), authmode, user, passwd, authsrv, chatty, key=privkey)        
        
    def cat(self, name, obj=None, out=None):
        if out is None:
            out = sys.stdout
        if obj is None:
            obj = self.extraLink
        if obj.open(name) is None:
            raise Exception("XCPU3: Could not open " + name)
        while 1:
            buf = obj.read(self.bufSize)
            if len(buf) <= 0:
                break
            out.write(buf)
        obj.close()

    def topStat (self, out=None):
        name =  baseURL + "status"
        self.cat(name, None, out)
        
    def getSessionStatus (self, out =  None) :
        if self.sessionID is None :
            print "Error: Session is not started yet"
        
        name = baseURL + str(self.sessionID) + "/status"
        self.cat (name, self.extraLink, out)

    def startSession (self):
        if self.sessionID is not None :
            print "Session already running"
            return self.sessionID
        
        name = baseURL + "clone"
        if self.ctlLink.open (name, py9p.ORDWR) is None :
            raise Exception("XCPU3: Could not open " + name)
        
        buf = self.ctlLink.read(self.bufSize)
        self.sessionID = int (buf)
        return self.sessionID
    
    def requestReservation (self, res) :
        if self.sessionID is None :
            self.startSession()
        if res is None :
            return
        param = "res " + res
        self.ctlLink.write(param)
    
    def requestExecution (self, cmd) :    
        if self.sessionID is None :
            self.startSession()
        param = "exec " + cmd
        self.ctlLink.write(param)
        
    def sendInput (self, input):
        if self.sessionID is None :
            raise Exception("XCPU3: Session not created")

        if input is None :
            return
        inf = open(input, "r", 0)
        
        name = baseURL + str(self.sessionID) + "/stdio"
        if self.IOLink.open(name, py9p.OWRITE) is None:
            raise Exception("XCPU3: Could not open stdio file " + name)
        
        sz = self.bufSize
        while 1:
            buf = inf.read(sz)
            if len(buf) <= 0:
                break
            self.IOLink.write(buf)
        self.IOLink.close()
        inf.close()
        
    def getOutput (self) :
        if self.sessionID is None :
            raise Exception("XCPU3: Session not created")
        
        name = baseURL + str(self.sessionID) + "/stdio"
        self.cat (name, self.IOLink )
    
    def endSession (self) :
        if self.sessionID is None :
            self.dPrint ( "Session already close")
            return
        self.ctlLink.close()
        self.sessionID = None


            
    def runJob (self, cmd, res = None, input = None ):
        self.dPrint ("Requesting reservation..")
        self.requestReservation(res)
        self.dPrint ( "Requesting execution..")
        self.requestExecution(cmd)
        if input is not None :
            self.dPrint ( "sending input")
            self.sendInput (input)
        self.dPrint ( "getting output")
        self.getOutput()
        if self.DEBUG:
            self.dPrint ( "checking session status")
            self.getSessionStatus ()
        self.dPrint ( "Closing session")
        self.endSession ()
        self.dPrint ( "Done..")
    
    def myPipe (self, inputFile, outputFile) :
        inf = open(inputFile, "r", 0)
        otf = open(outputFile, "w", 0)
         
        sz = self.bufSize
        while 1:
            buf = inf.read(sz)
            if len(buf) <= 0:
                break
            otf.write(buf)
        otf.close()
        inf.close()
        
    def command2child (self, child, cmd, inp):
        name = baseURL + str(self.sessionID) + "/" + str(child) + "/" + "ctl"
        childCtlFile = open (name, "rw", 0) 
        if childCtlFile  is None :
            raise Exception("XCPU3: Could not open child" + name)
        
        cmdbuf = "exec " + cmd
        buf = childCtlFile.write(cmdbuf)
        self.dPrint ("sent command ["+ cmdbuf +"] to child "+ name )
        if input is not None :
            self.dPrint ( "sending input")
            self.myPipe (inputFile, outoutFile)
        childCtlFile.close()
            
    def pipelineCmds (self, input = None ):
        self.dPrint ("Requesting reservation..")
        res = str(3)
        self.requestReservation(res)
        lastOutput = input
        
        child = 0
        self.command2child (child, "ls -l", lastOutput)
        lastOutput = baseURL + str(self.sessionID) + "/" + str(child) + "/" + "stdio"
        
        child = 1
        self.command2child (child, "sort", lastOutput )
        lastOutput = baseURL + str(self.sessionID) + "/" + str(child) + "/" + "stdio"
        
        child = 2
        self.command2child (child, "wc -l", lastOutput)
        lastOutput = baseURL + str(self.sessionID) + "/" + str(child) + "/" + "stdio"

        self.dPrint ( "getting output")
        self.cat(lastOutput)
        if self.DEBUG:
            self.dPrint ( "checking session status")
            self.getSessionStatus ()
        self.dPrint ( "Closing session")
        self.endSession ()
        self.dPrint ( "Done..")

        
def showUsage () :
        print "Usage: " + sys.argv[0] + " <server-ip> <port> <number of resources>"\
        + " <command> <inputFile>"
        print "Example: " + sys.argv[0] + " localhost 5544 0 \"ls -l\""
        print "Example: " + sys.argv[0] + " localhost 5544 \"3 Linux 386\" \"wc -l\" " + sys.argv[0]
        

def main ():
    if (len(sys.argv) > 4 ) :
        remoteHost = sys.argv[1]
        remotePort = int(sys.argv[2])
        resReq = sys.argv[3]
        command = sys.argv[4]
        inFile = None
    else :
        showUsage ()
        sys.exit()

    
    if (len(sys.argv) > 5 ) :
        inFile = sys.argv[5]

    # acutal code responsible for working
    mycpu = XCPU3Client (remoteHost, remotePort)
#    mycpu.DEBUG = True
    print "Top statistics is"
    mycpu.topStat()
    
    #mycpu.pipelineCmds ()
    
    mycpu.runJob (command, resReq, inFile)

    
if __name__ == "__main__" :
    main ()
        
