import os
import sys
from jinja2 import Environment, FileSystemLoader

CSList = list()
SCList = list()

class Function:
    def __init__(self, Name, Code):
        self.name = Name
        self.code = Code
        self.argList = list()

    def pushArg(self, Name, Type):
        self.argList.append(Arg(Name, Type))

class Arg:
    def __init__(self, Name, Type):
        self.name = Name
        self.type = Type

def PacketListFileParse():
    filePath = sys.argv[1] + '\\PacketList.txt'
    f = open(filePath, 'r')

    PacketDir = str()
    index = 0

    lines = f.readlines()
    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Proxy
        if line == "# Server to Client":
            PacketDir = "SC"
            continue
        
        # Stub
        if line == "# Client to Server":
            PacketDir = "CS"
            continue

        if line[0] != '-':
            p = line.split(' ')
            packetName = PacketDir + p[0]
            packetCode = int(p[1])
            Function(packetName, packetCode)
            if PacketDir == "SC":
                SCList.append(Function(packetName, packetCode))
                index = len(SCList) - 1
            elif PacketDir == "CS":
                CSList.append(Function(packetName, packetCode))
                index = len(CSList) - 1

        else: # argList
            name, type = [l.strip() for l in line[1:].split(':')]

            if PacketDir == "SC":
                SCList[index].pushArg(name, type)
            elif PacketDir == "CS":
                CSList[index].pushArg(name, type)

    f.close()

def JinJaTemplate():
    file_loader = FileSystemLoader(sys.argv[1] + '\\templates')
    env = Environment(loader=file_loader)

    # 디렉터리 내 모든 파일 읽기
    fileList = os.listdir(sys.argv[1] + '\\templates')

    # 읽은 파일 모두 jinja2 template render 하고 파일에 쓰기
    for file in fileList:
        template = env.get_template(file)
        result = template.render(scList=SCList, csList=CSList)
        resultFile = open(sys.argv[1] + '\\result\\' + file, 'w+')
        resultFile.write(result)
        resultFile.close();

def Main():
    PacketListFileParse()
    JinJaTemplate()
    


if (__name__ == "__main__"):
    Main()