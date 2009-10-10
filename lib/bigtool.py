import sys
import readline
from cmd import Cmd
from lib.exception import *
from lib.moduleinfo import ModuleInfo
from lib.command import BigToolCmd

class BigTool(Cmd):
    """simple bigtool command example."""
    
    doc_leader = ""
    doc_header = "sub commands (type help <topic>):"

    def __init__(self):
        Cmd.__init__(self)
        self.prompt = '> '
        self.ruler = '-'
        self.modules = {}
        self.complete_hint = []

    def parse_cmd(self, line):
        try:
            cmd = BigToolCmd(line)
            self._validate_cmd(cmd)
            self._handle_cmd(cmd)
        except BigToolException as e:
            print(e)
    

    def _validate_cmd(self, cmd):
        if not cmd.module_name in self.modules:
            raise UnknownModule(cmd.module_name)
        
        module_info = self.modules[cmd.module_name]
        if not module_info.has_command_with_name(cmd.command_name):
            raise UnknownCmd(cmd.module_name, cmd.command_name)

        command_info = module_info.get_command_with_name(cmd.command_name)
        support_params = command_info.get_parameter_names()
        if not cmd.parameters and support_params:
            raise MissingParameter(cmd.module_name, cmd.command_name, support_params[0])
        elif cmd.parameters and not support_params:
            raise UnknownParameter(cmd.module_name, cmd.command_name, list(cmd.parameters.keys())[0])
        elif cmd.parameters:
            for param in cmd.parameters:
                if not param in support_params:
                    raise UnknownParameter(cmd.module_name, cmd.command_name, param)
            for param in support_params:
                if not param in cmd.parameters:
                    raise MissingParameter(cmd.module_name, cmd.command_name, param)
            

    def _handle_cmd(self, cmd):
        #to do, consist xml package and send to bind10
        pass

    def add_module_info(self, module_info):
        if not isinstance(module_info, ModuleInfo):
            raise UnknownModule(type(module_info))
        self.modules[module_info.name] = module_info

    #override methods in cmd
    def default(self, line):        
        self.parse_cmd(line)

    def emptyline(self):
        pass
    
    def cmdloop(self):
        try:
            Cmd.cmdloop(self)
        except KeyboardInterrupt:
            return True
            
    def do_help(self, module_name):
        if module_name in self.modules.keys():
            print(self.modules[module_name].desc)
        else:
            print("Unknow module")
    
    def onecmd(self, line):
        if line == 'EOF'or line.lower() == "quit":
            return True
        Cmd.onecmd(self, line)
                    
    def complete(self, text, state):
        if 0 == state:
            self.complete_hint = []
            current_input_line = readline.get_line_buffer()
            try:
                cmd = BigToolCmd(current_input_line)
                if cmd.parameters:
                    self.complete_hint = self._get_support_parameters_startswith_name(cmd.module_name, cmd.command_name, text)
                else:
                    self.complete_hint = self._get_support_commands_startswith_name(cmd.module_name, text)
                    

            except CmdWithoutModuleName:
                self.complete_hint = list(self.modules.keys())

            except CmdWithoutCommandName as e:
                if e.module_name in self.modules and text == "":
                    self.complete_hint = self.modules[e.module_name].get_command_names()
                else:
                    self.complete_hint = self._get_support_modules_startswith_name(text)

            except CmdParameterFormatError as e:
                self.complete_hint = self._get_support_parameters_startswith_name(e.module_name, e.command_name, text)

            except BigToolException:
                self.complete_hint = []

        if state < len(self.complete_hint):
            return self.complete_hint[state]
        else:
            return None

    def _get_support_modules_startswith_name(self, full_or_partial_module_name):
        return [module_name for module_name in self.modules if module_name.startswith(full_or_partial_module_name)]


    def _get_support_commands_startswith_name(self, module_name, full_or_partial_command_name):
        if module_name in self.modules:
            return [command_name 
                    for command_name in self.modules[module_name].get_command_names() 
                    if command_name.startswith(full_or_partial_command_name)]
        else:
            return []

    def _get_support_parameters_startswith_name(self, module_name, command_name, full_or_partial_parameter_name):
        if module_name in self.modules:
            if command_name in self.modules[module_name].get_command_names():
                command_info = self.modules[module_name].get_command_with_name(command_name)
                return [param for param in command_info.get_parameter_names() if param.startswith(full_or_partial_parameter_name)]
        return []


                
