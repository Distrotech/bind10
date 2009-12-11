from moduleinfo  import *
from bigtool import *

if __name__ == '__main__':
    #try:       
        tool = BigTool('localhost:8080')
        tool.cmdloop()
    #except:
        print("Failed to communicate with control/command module")


