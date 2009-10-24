import sys
import readline
import collections
from cmd import Cmd
from lib.exception import *
from lib.moduleinfo import ModuleInfo
from lib.command import BigToolCmd

CONST_BIGTOOL_HELP = """Bigtool, verstion 0.1
usage: <module name> <command name> [param1 = value1 [, param2 = value2]]
Type Tab character to get the hint of module/command/paramters.
Type \"help(? h)\" for help on bigtool.
Type \"<module_name> help\" for help on the specific module.
Type \"<module_name> <command_name> help\" for help on the specific command.
\nAvailable module names: """

class BigTool(Cmd):
    """simple bigtool command example."""    

    def __init__(self):
        Cmd.__init__(self)
        self.prompt = '> '
        self.ruler = '-'
        self.modules = collections.OrderedDict()
        self.add_module_info(ModuleInfo("help", desc = "Get help for bigtool"))
                    

    def validate_cmd(self, cmd):
        if not cmd.module in self.modules:
            raise CmdUnknownModuleSyntaxError(cmd.module)
        
        module_info = self.modules[cmd.module]
        if not module_info.has_command_with_name(cmd.command):
            raise CmdUnknownCmdSyntaxError(cmd.module, cmd.command)

        command_info = module_info.get_command_with_name(cmd.command)
        manda_params = command_info.get_mandatory_param_names()
        all_params = command_info.get_param_names()
        if command_info.need_instance_param():
            inst_name = module_info.get_instance_param_name()
            if inst_name:
                manda_params.append(inst_name)
                all_params.append(inst_name)
        
        # If help is inputed, don't do further paramters validation.
        for val in cmd.params.keys():
            if val == "help":
                return
        
        params = cmd.params.copy()       
        if not params and manda_params:            
            raise CmdMissParamSyntaxError(cmd.module, cmd.command, manda_params[0])            
        elif params and not all_params:
            raise CmdUnknownParamSyntaxError(cmd.module, cmd.command, 
                                             list(params.keys())[0])
        elif params:
            for name in params:
                if not name in all_params:
                    raise CmdUnknownParamSyntaxError(cmd.module, cmd.command, name)
            for name in manda_params:
                if not name in params:
                    raise CmdMissParamSyntaxError(cmd.module, cmd.command, name)
                              

    def _handle_cmd(self, cmd):
        #to do, consist xml package and send to bind10
        if cmd.command == "help" or ("help" in cmd.params.keys()):
            self._handle_help(cmd)
        else:
            self._temp_print_parse_result(cmd)

    def add_module_info(self, module_info):        
        self.modules[module_info.name] = module_info
        
    def get_module_names(self):
        return list(self.modules.keys())

    #override methods in cmd
    def default(self, line):
        self._parse_cmd(line)

    def emptyline(self):
        pass
    
    def cmdloop(self):
        try:
            Cmd.cmdloop(self)
        except KeyboardInterrupt:
            return True
            
    def do_help(self, name):
        print(CONST_BIGTOOL_HELP)
        for k in self.modules.keys():
            print("\t", self.modules[k])
                
    
    def onecmd(self, line):
        if line == 'EOF'or line.lower() == "quit":
            return True
            
        if line == 'h':
            line = 'help'
        
        Cmd.onecmd(self, line)
                    
    def complete(self, text, state):
        if 0 == state:
            text = text.strip()
            hints = []
            cur_line = readline.get_line_buffer()            
            try:
                cmd = BigToolCmd(cur_line)
                if not cmd.params and text:
                    hints = self._get_command_startswith(cmd.module, text)
                else:                       
                    hints = self._get_param_startswith(cmd.module, cmd.command,
                                                       text)
            except CmdModuleNameFormatError:
                if not text:
                    hints = list(self.modules.keys())
                    
            except CmdMissCommandNameFormatError as e:
                if not text.strip(): # command name is empty
                    hints = self.modules[e.module].get_command_names()                    
                else: 
                    hints = self._get_module_startswith(text)

            except CmdCommandNameFormatError as e:
                if e.module in self.modules:
                    hints = self._get_command_startswith(e.module, text)

            except CmdParamFormatError as e:
                hints = self._get_param_startswith(e.module, e.command, text)

            except BigToolException:
                hints = []
            
            self.hint = hints
            self._append_space_to_hint()

        if state < len(self.hint):
            return self.hint[state]
        else:
            return None
            

    def _get_module_startswith(self, text):       
        return [module
                for module in self.modules 
                if module.startswith(text)]


    def _get_command_startswith(self, module, text):
        if module in self.modules:            
            return [command
                    for command in self.modules[module].get_command_names() 
                    if command.startswith(text)]
        
        return []                    
                        

    def _get_param_startswith(self, module, command, text):        
        if module in self.modules:
            module_info = self.modules[module]            
            if command in module_info.get_command_names():                
                cmd_info = module_info.get_command_with_name(command)
                params = cmd_info.get_param_names() 
                if cmd_info.need_instance_param():
                    inst_name = module_info.get_instance_param_name()
                    if inst_name:
                        params.append(inst_name)
                
                hint = []
                if text:    
                    hint = [val for val in params if val.startswith(text)]
                else:
                    hint = list(params)
                
                if len(hint) == 1 and hint[0] != "help":
                    hint[0] = hint[0] + " ="    
                
                return hint

        return []
        

    def _parse_cmd(self, line):
        try:
            cmd = BigToolCmd(line)
            self.validate_cmd(cmd)
            self._handle_cmd(cmd)
        except BigToolException as e:
            print("Error! ", e)
            self._print_correct_usage(e)
            
            
    def _print_correct_usage(self, ept):        
        if isinstance(ept, CmdUnknownModuleSyntaxError):
            self.do_help(None)
            
        elif isinstance(ept, CmdUnknownCmdSyntaxError):
            self.modules[ept.module].module_help()
            
        elif isinstance(ept, CmdMissParamSyntaxError) or \
             isinstance(ept, CmdUnknownParamSyntaxError):
             self.modules[ept.module].command_help(ept.command)
                 
    
    def _temp_print_parse_result(self, cmd):
        print("command line is parsed:\nmodule_name: %s\ncommand_name: %s\n"\
              "paramters:" % (cmd.module, cmd.command))
              
        for name in cmd.params.keys():            
            print("\t%s \t%s" % (name, cmd.params[name]))
            
                
    def _append_space_to_hint(self):
        """Append one space at the end of complete hint."""
        
        self.hint = [(val + " ") for val in self.hint]
            
            
    def _handle_help(self, cmd):
        if cmd.command == "help":
            self.modules[cmd.module].module_help()
        else:
            self.modules[cmd.module].command_help(cmd.command)
            
            
        
            
            
            