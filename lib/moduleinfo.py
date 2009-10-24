import collections

# Define value type
STRING_TYPE = "string"
LIST_TYPE = "list"
INT_TYPE = "int"

class ParamInfo:
    """The parameter of one command
    each command parameter have four attributes, 
    parameter name, parameter type, parameter value, and parameter description
    """
    def __init__(self, name, desc = '', type = STRING_TYPE, 
                 optional = False, value = ''):
        self.name = name
        self.type = type
        self.value = value #Save parameter's value
        self.desc = desc
        self.optional = optional
        
    def is_optional(self):
        return self.optional        
    
    def __str__(self):        
        return str("\t%s <type: %s> \t(%s)" % (self.name, self.type, self.desc))
        

class CommandInfo:
    """One command which provide by one bind10 module, it has zero or 
    more parameters
    """

    def __init__(self, name, desc = "", need_inst_param = True):
        self.name = name
        # Wether command needs parameter "instance_name" 
        self.need_inst_param = need_inst_param 
        self.desc = desc
        self.params = collections.OrderedDict()        
        # Set default parameter "help"
        self.add_param(ParamInfo("help", 
                                  desc = "Get help for command",
                                  optional = True))
                
    def __str__(self):
        return str("%s \t(%s)" % (self.name, self.desc))
        

    def add_param(self, paraminfo):
        self.params[paraminfo.name] = paraminfo
        

    def has_param_with_name(self, param_name):
        return param_name in self.params
        

    def get_param_with_name(self, param_name):
        return self.params[param_name]
        

    def get_params(self):
        return list(self.params.values())
        

    def get_param_names(self):
        return list(self.params.keys())
        
        
    def get_mandatory_param_names(self):
        all_names = self.params.keys()
        return [name for name in all_names 
                if not self.params[name].is_optional()]        
        
        
    def need_instance_param(self):
        return self.need_inst_param
        

    def command_help(self, inst_name, inst_type, inst_desc):
        print("Command ", self)
        print("\t\thelp (Get help for command)")
                
        params = self.params.copy()
        del params["help"]
        if self.need_inst_param:
            params[inst_name] = ParamInfo(name = inst_name, type = inst_type,
                                          desc = inst_desc)        
        if len(params) == 0:
            print("\tNo parameters for the command")
            return
        
        print("\n\tMandatory parameters:")
        find_optional = False
        for info in params.values():            
            if not info.is_optional():
                print("\t", info)
            else:
                find_optional = True
                              
        if find_optional:
            print("\n\tOptional parameters:")            
            for info in params.values():
                if info.is_optional():
                    print("\t", info)


class ModuleInfo:
    """Define the information of one module, include module name, 
    module supporting commands, instance name and the value type of instance name
    """    
    
    def __init__(self, name, inst_name = "", inst_type = STRING_TYPE, 
                 inst_desc = "", desc = ""):
        self.name = name
        self.inst_name = inst_name
        self.inst_type = inst_type
        self.inst_desc = inst_desc
        self.desc = desc
        self.commands = collections.OrderedDict()         
        # Add defaut help command
        self.add_command(CommandInfo(name = "help", 
                                     desc = "Get help for module",
                                     need_inst_param = False))
        
    def __str__(self):
        return str("%s \t%s" % (self.name, self.desc))
        
    def add_command(self, commandInfo):        
        self.commands[commandInfo.name] = commandInfo
        
        
    def has_command_with_name(self, command_name):
        return command_name in self.commands
        

    def get_command_with_name(self, command_name):
        return self.commands[command_name]
        
        
    def get_commands(self):
        return list(self.commands.values())
        
    
    def get_command_names(self):
        return list(self.commands.keys())
        
    
    def get_instance_param_name(self):
        return self.inst_name
        
        
    def get_instance_param_type(self):
        return self.inst_type
        

    def module_help(self):
        print("Module ", self, "\nAvailable commands:")
        for k in self.commands.keys():
            print("\t", self.commands[k])
            
            
    def command_help(self, command):
        self.commands[command].command_help(self.inst_name, 
                                            self.inst_type,
                                            self.inst_desc)
        
            
        
